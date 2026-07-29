#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * protocol.h - Shared wire protocol definitions for the lab equipment
 * reservation client-server system.
 *
 * Wire format: every message is a single line of ASCII text of the form
 *
 *     TYPE:PAYLOAD\n
 *
 * - TYPE is one of the constants below (without the trailing colon).
 * - PAYLOAD is a UTF-8/ASCII string with no embedded newline characters.
 * - Every message is terminated by a single '\n' character. This is the
 *   ONLY framing mechanism: a full message is "everything up to and
 *   including the next newline byte" on the socket.
 * - Max line length (including type, colon, payload, newline) is
 *   MAX_MSG_LEN bytes.
 *
 * Message directions:
 *   C->S  = sent by client, received by server
 *   S->C  = sent by server, received by client
 */

/* Maximum size, in bytes, of a single protocol message (including \n) */
#define MAX_MSG_LEN 512

/* C->S: client requests authentication.  PAYLOAD = user id
 *   Example: "AUTH:alice123\n"
 */
#define MSG_AUTH "AUTH"

/* S->C: authentication succeeded.  PAYLOAD = human-readable message */
#define MSG_AUTH_OK "AUTH_OK"

/* S->C: authentication failed.  PAYLOAD = reason */
#define MSG_AUTH_FAIL "AUTH_FAIL"

/* S->C: list of available equipment.
 * PAYLOAD = comma-separated "id=name" pairs
 *   Example: "EQUIP_LIST:1=Oscilloscope,2=SolderStation,3=LogicAnalyzer\n"
 */
#define MSG_EQUIP_LIST "EQUIP_LIST"

/* C->S: client requests reservation of one item.  PAYLOAD = equipment id */
#define MSG_RESERVE "RESERVE"

/* S->C: reservation succeeded.  PAYLOAD = human-readable message */
#define MSG_RESERVE_OK "RESERVE_OK"

/* S->C: reservation failed (already taken, or invalid id).
 * PAYLOAD = reason
 */
#define MSG_RESERVE_FAIL "RESERVE_FAIL"

/* C->S: client is closing the session.  PAYLOAD = user id */
#define MSG_BYE "BYE"

/* S->C: server acknowledges session close.  PAYLOAD = farewell message */
#define MSG_BYE_OK "BYE_OK"

#endif /* PROTOCOL_H */