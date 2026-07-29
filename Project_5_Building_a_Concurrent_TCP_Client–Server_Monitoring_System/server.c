#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"
#include "net_utils.h"
#include "shared.h"

#define DEFAULT_PORT 8080
#define LISTEN_BACKLOG 16

/**
 * is_registered_user - check whether a user id is on the registered list.
 * @user_id: id supplied by the client
 *
 * Return: 1 if registered, 0 otherwise.
 */
static int is_registered_user(const char *user_id)
{
	int i;

	for (i = 0; i < registered_user_count; i++)
	{
		if (strcmp(registered_users[i], user_id) == 0)
			return (1);
	}
	return (0);
}

/**
 * add_connected_user - record a user id as currently connected.
 * Thread-safe: acquires connected_users_lock internally.
 * @user_id: id to add
 */
static void add_connected_user(const char *user_id)
{
	pthread_mutex_lock(&connected_users_lock);
	if (connected_user_count < MAX_CONNECTED)
	{
		strcpy(connected_users[connected_user_count], user_id);
		connected_user_count++;
	}
	pthread_mutex_unlock(&connected_users_lock);
}

/**
 * remove_connected_user - remove a user id from the connected list,
 * e.g. on graceful logout or unexpected disconnect.
 * Thread-safe: acquires connected_users_lock internally.
 * @user_id: id to remove
 */
static void remove_connected_user(const char *user_id)
{
	int i, j;

	pthread_mutex_lock(&connected_users_lock);
	for (i = 0; i < connected_user_count; i++)
	{
		if (strcmp(connected_users[i], user_id) == 0)
		{
			for (j = i; j < connected_user_count - 1; j++)
				strcpy(connected_users[j], connected_users[j + 1]);
			connected_user_count--;
			break;
		}
	}
	pthread_mutex_unlock(&connected_users_lock);
}

/**
 * print_status - print the current connected-users list and equipment
 * reservation table. Locks both mutexes so the snapshot it prints is
 * internally consistent (no other thread can mutate either structure
 * mid-print).
 */
static void print_status(void)
{
	int i;

	pthread_mutex_lock(&connected_users_lock);
	pthread_mutex_lock(&equipment_lock);

	printf("---- STATUS ----\n");
	printf("Connected users (%d): ", connected_user_count);
	for (i = 0; i < connected_user_count; i++)
		printf("%s%s", connected_users[i],
		       (i < connected_user_count - 1) ? ", " : "");
	printf("\n");

	printf("Equipment status:\n");
	for (i = 0; i < equipment_count; i++)
	{
		if (equipment_table[i].reserved)
			printf("  [%d] %s -> RESERVED by %s\n",
			       equipment_table[i].id, equipment_table[i].name,
			       equipment_table[i].reserved_by);
		else
			printf("  [%d] %s -> available\n",
			       equipment_table[i].id, equipment_table[i].name);
	}
	printf("----------------\n");

	pthread_mutex_unlock(&equipment_lock);
	pthread_mutex_unlock(&connected_users_lock);
}

/**
 * build_equipment_payload - render the equipment table into the
 * comma-separated "id=name" wire format defined in protocol.h.
 * Locks equipment_lock so the listing reflects a single consistent
 * moment in time, even while other threads may be reserving items.
 * @out: destination buffer
 * @out_sz: size of out
 */
static void build_equipment_payload(char *out, size_t out_sz)
{
	char item[64];
	int i;

	out[0] = '\0';
	pthread_mutex_lock(&equipment_lock);
	for (i = 0; i < equipment_count; i++)
	{
		snprintf(item, sizeof(item), "%s%d=%s",
			 (i > 0) ? "," : "",
			 equipment_table[i].id, equipment_table[i].name);
		strncat(out, item, out_sz - strlen(out) - 1);
	}
	pthread_mutex_unlock(&equipment_lock);
}

/**
 * try_reserve - attempt to reserve equipment `equip_id` for `user_id`.
 * This is the critical section that prevents two users from reserving
 * the same item simultaneously: the check ("is it free?") and the
 * update ("mark it reserved") happen atomically under equipment_lock,
 * so no other thread can observe or act on an in-between state.
 * @equip_id: 1-based id of the equipment requested
 * @user_id: id of the requesting user
 * @msg_out: buffer to receive a human-readable result message
 * @msg_sz: size of msg_out
 *
 * Return: 1 if reservation succeeded, 0 if it failed.
 */
static int try_reserve(int equip_id, const char *user_id,
			char *msg_out, size_t msg_sz)
{
	int i, result = 0;

	pthread_mutex_lock(&equipment_lock);
	for (i = 0; i < equipment_count; i++)
	{
		if (equipment_table[i].id != equip_id)
			continue;
		if (equipment_table[i].reserved)
		{
			snprintf(msg_out, msg_sz, "%s already reserved by %s",
				 equipment_table[i].name,
				 equipment_table[i].reserved_by);
		}
		else
		{
			equipment_table[i].reserved = 1;
			strcpy(equipment_table[i].reserved_by, user_id);
			snprintf(msg_out, msg_sz, "%s reserved successfully",
				 equipment_table[i].name);
			result = 1;
		}
		break;
	}
	if (i == equipment_count)
		snprintf(msg_out, msg_sz, "No such equipment id: %d", equip_id);
	pthread_mutex_unlock(&equipment_lock);
	return (result);
}

/**
 * handle_client - per-connection thread entry point. Runs the full
 * session for one client: authenticate, send equipment list, process
 * one reservation request, then wait for a clean logout. Any read/write
 * failure at any point is treated as a disconnect: the session is torn
 * down and the thread exits without taking down the server.
 * @arg: heap-allocated int* holding the accepted socket fd (freed here)
 *
 * Return: NULL always (required by pthread_create's signature).
 */
static void *handle_client(void *arg)
{
	int client_fd = *(int *)arg;
	char type[32], payload[MAX_MSG_LEN];
	char user_id[USER_ID_LEN] = "";
	char equip_payload[256], result_msg[128];
	int authenticated = 0;

	free(arg);

	if (recv_msg(client_fd, type, sizeof(type), payload, sizeof(payload)) <= 0
	    || strcmp(type, MSG_AUTH) != 0)
	{
		close(client_fd);
		return (NULL);
	}
	strncpy(user_id, payload, sizeof(user_id) - 1);

	if (is_registered_user(user_id))
	{
		authenticated = 1;
		add_connected_user(user_id);
		printf("[AUTH OK] %s authenticated.\n", user_id);
		send_msg(client_fd, MSG_AUTH_OK, "Welcome");
	}
	else
	{
		printf("[AUTH FAIL] Unknown user id '%s'.\n", user_id);
		send_msg(client_fd, MSG_AUTH_FAIL, "Unknown user id");
	}
	print_status();

	if (!authenticated)
	{
		close(client_fd);
		return (NULL);
	}

	build_equipment_payload(equip_payload, sizeof(equip_payload));
	if (send_msg(client_fd, MSG_EQUIP_LIST, equip_payload) != 0)
	{
		remove_connected_user(user_id);
		close(client_fd);
		return (NULL);
	}

	if (recv_msg(client_fd, type, sizeof(type), payload, sizeof(payload)) <= 0
	    || strcmp(type, MSG_RESERVE) != 0)
	{
		printf("[DISCONNECT] %s disconnected before reserving.\n", user_id);
		remove_connected_user(user_id);
		print_status();
		close(client_fd);
		return (NULL);
	}

	if (try_reserve(atoi(payload), user_id, result_msg, sizeof(result_msg)))
	{
		printf("[RESERVE OK] %s -> %s\n", user_id, result_msg);
		send_msg(client_fd, MSG_RESERVE_OK, result_msg);
	}
	else
	{
		printf("[RESERVE FAIL] %s -> %s\n", user_id, result_msg);
		send_msg(client_fd, MSG_RESERVE_FAIL, result_msg);
	}
	print_status();

	if (recv_msg(client_fd, type, sizeof(type), payload, sizeof(payload)) > 0
	    && strcmp(type, MSG_BYE) == 0)
	{
		printf("[BYE] %s closed session gracefully.\n", user_id);
		send_msg(client_fd, MSG_BYE_OK, user_id);
	}
	else
	{
		printf("[DISCONNECT] %s disconnected unexpectedly.\n", user_id);
	}

	remove_connected_user(user_id);
	print_status();
	close(client_fd);
	return (NULL);
}

/**
 * main - set up the listening socket and accept loop. Each accepted
 * connection is handed to a new detached thread via handle_client, so
 * multiple clients are served concurrently.
 */
int main(int argc, char *argv[])
{
	int server_fd, *client_fd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
	int opt = 1;
	pthread_t tid;

	/* A client disconnecting mid-write would otherwise deliver SIGPIPE
	 * and kill the whole server; ignore it and rely on send()'s -1/EPIPE
	 * return value instead, which we already check.
	 */
	signal(SIGPIPE, SIG_IGN);

	/* Unbuffered stdout so log lines appear immediately/in order even
	 * when multiple threads are printing concurrently and the process
	 * may be stopped abruptly (e.g. Ctrl+C, kill).
	 */
	setvbuf(stdout, NULL, _IONBF, 0);

	shared_init();

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		perror("socket");
		return (1);
	}
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr *)&server_addr,
		 sizeof(server_addr)) < 0)
	{
		perror("bind");
		close(server_fd);
		return (1);
	}

	if (listen(server_fd, LISTEN_BACKLOG) < 0)
	{
		perror("listen");
		close(server_fd);
		return (1);
	}

	printf("Server listening on port %d (Ctrl+C to stop)...\n", port);

	while (1)
	{
		client_fd = malloc(sizeof(int));
		if (client_fd == NULL)
			continue;
		*client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
				     &addr_len);
		if (*client_fd < 0)
		{
			perror("accept");
			free(client_fd);
			continue;
		}
		printf("[CONNECT] New connection from %s:%d\n",
		       inet_ntoa(client_addr.sin_addr),
		       ntohs(client_addr.sin_port));

		if (pthread_create(&tid, NULL, handle_client, client_fd) != 0)
		{
			perror("pthread_create");
			close(*client_fd);
			free(client_fd);
			continue;
		}
		pthread_detach(tid);
	}

	close(server_fd);
	return (0);
}