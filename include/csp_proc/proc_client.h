#ifndef CSP_PROC_CLIENT_H
#define CSP_PROC_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <csp/csp.h>
#include <csp_proc/proc_types.h>
#include <csp_proc/proc_server.h>
#include <csp_proc/proc_pack.h>

typedef int (*response_callback_t)(csp_packet_t *, void *);

int proc_transaction(
	csp_packet_t * packet,
	response_callback_t response_callback,
	void * callback_arg,
	int host,
	int timeout);

int proc_del_request(uint8_t proc_slot, int host, int timeout);

int proc_pull_request(proc_t * procedure, uint8_t proc_slot, int host, int timeout);

int proc_push_request(proc_t * procedure, uint8_t proc_slot, int host, int timeout);

int proc_slots_request(uint8_t * slots, uint8_t * slot_count, int host, int timeout);

int proc_run_request(uint8_t proc_slot, int host, int timeout);

#ifdef __cplusplus
}
#endif

#endif  // CSP_PROC_CLIENT_H
