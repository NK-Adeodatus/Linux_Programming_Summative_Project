#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"
#include "net_utils.h"

#define DEFAULT_PORT 8080

/**
 * print_equipment_list - pretty-print the raw "id=name,id=name" payload
 * received from the server as a readable numbered list.
 * @payload: raw EQUIP_LIST payload
 */
static void print_equipment_list(const char *payload)
{
	char buf[MAX_MSG_LEN];
	char *token, *eq;

	strncpy(buf, payload, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	printf("Available equipment:\n");
	token = strtok(buf, ",");
	while (token != NULL)
	{
		eq = strchr(token, '=');
		if (eq != NULL)
		{
			*eq = '\0';
			printf("  [%s] %s\n", token, eq + 1);
		}
		token = strtok(NULL, ",");
	}
}

/**
 * main - connect to the server, run one authenticate -> browse ->
 * reserve -> close session, then exit.
 */
int main(int argc, char *argv[])
{
	int sockfd;
	struct sockaddr_in server_addr;
	const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
	int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
	char user_id[64], choice[16];
	char type[32], payload[MAX_MSG_LEN];

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("socket");
		return (1);
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
	{
		fprintf(stderr, "Invalid server address: %s\n", server_ip);
		close(sockfd);
		return (1);
	}

	if (connect(sockfd, (struct sockaddr *)&server_addr,
		    sizeof(server_addr)) < 0)
	{
		perror("connect");
		close(sockfd);
		return (1);
	}

	printf("Enter your user ID: ");
	if (fgets(user_id, sizeof(user_id), stdin) == NULL)
	{
		close(sockfd);
		return (1);
	}
	user_id[strcspn(user_id, "\n")] = '\0';

	if (send_msg(sockfd, MSG_AUTH, user_id) != 0)
	{
		fprintf(stderr, "Failed to send authentication request.\n");
		close(sockfd);
		return (1);
	}

	if (recv_msg(sockfd, type, sizeof(type), payload, sizeof(payload)) <= 0)
	{
		fprintf(stderr, "Server closed the connection unexpectedly.\n");
		close(sockfd);
		return (1);
	}

	if (strcmp(type, MSG_AUTH_FAIL) == 0)
	{
		printf("Authentication failed: %s\n", payload);
		close(sockfd);
		return (0);
	}
	if (strcmp(type, MSG_AUTH_OK) != 0)
	{
		fprintf(stderr, "Unexpected server response.\n");
		close(sockfd);
		return (1);
	}
	printf("Authentication successful: %s\n", payload);

	if (recv_msg(sockfd, type, sizeof(type), payload, sizeof(payload)) <= 0
	    || strcmp(type, MSG_EQUIP_LIST) != 0)
	{
		fprintf(stderr, "Did not receive equipment list.\n");
		close(sockfd);
		return (1);
	}
	print_equipment_list(payload);

	printf("Enter the ID of the equipment to reserve: ");
	if (fgets(choice, sizeof(choice), stdin) == NULL)
	{
		close(sockfd);
		return (1);
	}
	choice[strcspn(choice, "\n")] = '\0';

	if (send_msg(sockfd, MSG_RESERVE, choice) != 0)
	{
		fprintf(stderr, "Failed to send reservation request.\n");
		close(sockfd);
		return (1);
	}

	if (recv_msg(sockfd, type, sizeof(type), payload, sizeof(payload)) <= 0)
	{
		fprintf(stderr, "Server closed the connection unexpectedly.\n");
		close(sockfd);
		return (1);
	}
	if (strcmp(type, MSG_RESERVE_OK) == 0)
		printf("Reservation successful: %s\n", payload);
	else
		printf("Reservation failed: %s\n", payload);

	send_msg(sockfd, MSG_BYE, user_id);
	if (recv_msg(sockfd, type, sizeof(type), payload, sizeof(payload)) > 0
	    && strcmp(type, MSG_BYE_OK) == 0)
	{
		printf("Session closed. Goodbye, %s\n", user_id);
	}
	else
	{
		printf("Session closed. Goodbye, %s\n", user_id);
	}

	close(sockfd);
	return (0);
}