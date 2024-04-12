#ifndef CSP_PROC_SERVER_H
#define CSP_PROC_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <csp/csp_types.h>

#define PROC_PORT_SERVER 14

/**
 * First byte of the packet is composed of the following:
 * - 4 bits for the packet type
 * 0b----xxxx
 * - 4 bits for packet flags
 * 0bxxxx----
 *	- 0b1xxx----: end of transmission
 *	- 0b0xxx----: not end of transmission (more packets to come)
 *	- 0bx1xx----: request caused error
 *	- 0bx0xx----: request successful
 *	- remaining bits are reserved for future use
 */

typedef enum {

	PROC_DEL_REQUEST,
	PROC_DEL_RESPONSE,
	PROC_PULL_REQUEST,
	PROC_PULL_RESPONSE,
	PROC_PUSH_REQUEST,
	PROC_PUSH_RESPONSE,
	PROC_SLOTS_REQUEST,
	PROC_SLOTS_RESPONSE,
	PROC_RUN_REQUEST,
	PROC_RUN_RESPONSE,

} proc_packet_type_e;

#define PROC_TYPE_MASK 0b00001111

#define PROC_FLAG_END_MASK 0b10000000
#define PROC_FLAG_END      0b10000000

#define PROC_FLAG_ERROR_MASK 0b01000000
#define PROC_FLAG_ERROR      0b01000000

/**
 * Handle incoming procedure requests
 *
 * @param packet
 */
void proc_serve(csp_packet_t * packet);

#ifdef __cplusplus
}
#endif

#endif  // CSP_PROC_SERVER_H
