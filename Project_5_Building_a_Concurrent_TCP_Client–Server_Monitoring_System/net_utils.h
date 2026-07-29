#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stddef.h>
#include "protocol.h"

/*
 * net_utils.h - reliable, framing-aware send/receive helpers built on top
 * of raw TCP sockets. These guarantee that a "message" as defined in
 * protocol.h (a single TYPE:PAYLOAD line) is sent and received as a whole,
 * regardless of how the underlying socket happens to split or coalesce
 * the bytes.
 */

/* Send a single protocol message "type:payload\n" over sockfd.
 * Returns 0 on success, -1 on error (including a peer that has closed
 * the connection, reported as EPIPE since SIGPIPE is ignored).
 */
int send_msg(int sockfd, const char *type, const char *payload);

/* Receive a single protocol message from sockfd and split it into
 * type_buf and payload_buf (both caller-supplied, size type_sz/payload_sz).
 * Returns:
 *    1  on success (a full message was read and parsed)
 *    0  if the peer performed an orderly shutdown (EOF, no data)
 *   -1  on error, or a malformed message (no ':' separator found)
 */
int recv_msg(int sockfd, char *type_buf, size_t type_sz,
	     char *payload_buf, size_t payload_sz);

#endif /* NET_UTILS_H */