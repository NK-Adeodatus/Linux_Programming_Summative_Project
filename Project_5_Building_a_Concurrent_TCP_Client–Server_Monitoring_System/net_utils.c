#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "net_utils.h"

/**
 * send_all - loop over send() until every byte in buf has been written,
 * since a single send() call is not guaranteed to write the whole buffer.
 * @sockfd: destination socket
 * @buf: bytes to send
 * @len: number of bytes in buf
 *
 * Return: 0 on success, -1 on error.
 */
static int send_all(int sockfd, const char *buf, size_t len)
{
	size_t sent = 0;
	ssize_t n;

	while (sent < len)
	{
		n = send(sockfd, buf + sent, len - sent, 0);
		if (n <= 0)
			return (-1);
		sent += (size_t)n;
	}
	return (0);
}

int send_msg(int sockfd, const char *type, const char *payload)
{
	char line[MAX_MSG_LEN];
	int written;

	written = snprintf(line, sizeof(line), "%s:%s\n", type, payload);
	if (written < 0 || (size_t)written >= sizeof(line))
		return (-1); /* message would not fit our framing limit */

	return (send_all(sockfd, line, (size_t)written));
}

/**
 * recv_line - read from sockfd one byte at a time until a newline is
 * seen, the buffer is full, or the connection ends. This is the core of
 * our framing strategy: it is what lets us treat "one line" as "one
 * message" regardless of how TCP happened to chunk the underlying bytes.
 * @sockfd: source socket
 * @buf: destination buffer
 * @maxlen: size of buf, including room for the terminating '\0'
 *
 * Return: number of bytes stored in buf (not counting '\0') on success,
 * 0 if the peer closed the connection before any data arrived,
 * -1 on a genuine socket error.
 */
static int recv_line(int sockfd, char *buf, size_t maxlen)
{
	size_t len = 0;
	char c;
	ssize_t n;

	while (len < maxlen - 1)
	{
		n = recv(sockfd, &c, 1, 0);
		if (n == 0)
			return (len == 0 ? 0 : (int)len); /* peer closed */
		if (n < 0)
			return (-1);
		if (c == '\n')
			break;
		buf[len++] = c;
	}
	buf[len] = '\0';
	return ((int)len);
}

int recv_msg(int sockfd, char *type_buf, size_t type_sz,
	     char *payload_buf, size_t payload_sz)
{
	char line[MAX_MSG_LEN];
	int n;
	char *sep;

	n = recv_line(sockfd, line, sizeof(line));
	if (n <= 0)
		return (n); /* 0 = clean disconnect, -1 = error */

	sep = strchr(line, ':');
	if (sep == NULL)
		return (-1); /* malformed: missing TYPE:PAYLOAD separator */

	*sep = '\0';
	if (strlen(line) >= type_sz || strlen(sep + 1) >= payload_sz)
		return (-1); /* would overflow caller's buffers */

	strcpy(type_buf, line);
	strcpy(payload_buf, sep + 1);
	return (1);
}