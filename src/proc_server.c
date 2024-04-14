#include <csp_proc/proc_server.h>
#include <csp_proc/proc_pack.h>
#include <csp_proc/proc_store.h>
#include <csp_proc/proc_runtime.h>

#include <stdlib.h>
#include <csp/csp_types.h>
#include <csp/csp.h>

int proc_server_init() {
	int ret = 0;

	if (proc_store_init != NULL) {
		csp_print("Initializing proc store\n");
		ret = proc_store_init();
		if (ret != 0) {
			return ret;
		}
	}

	if (proc_runtime_init != NULL) {
		csp_print("Initializing proc runtime\n");
		ret = proc_runtime_init();
		if (ret != 0) {
			return ret;
		}
	}
	csp_print("Proc server initialized\n");
	return ret;
}

static void proc_serve_del_request(csp_packet_t * packet) {
	uint8_t slot = packet->data[1];
	delete_proc(slot);

	packet->data[0] = PROC_DEL_RESPONSE;
	packet->data[0] |= PROC_FLAG_END;
	packet->length = 1;

	csp_sendto_reply(packet, packet, CSP_O_SAME);
}

static void proc_serve_pull_request(csp_packet_t * packet) {
	uint8_t slot = packet->data[1];
	proc_t * procedure = get_proc(slot);
	if (procedure == NULL) {
		printf("Procedure not found\n");
		packet->data[0] = PROC_PULL_RESPONSE;
		packet->data[0] |= PROC_FLAG_END;
		packet->data[0] |= PROC_FLAG_ERROR;
		packet->length = 1;
		csp_sendto_reply(packet, packet, CSP_O_SAME);
		return;
	}

	int ret = pack_proc_into_csp_packet(procedure, packet);
	if (ret < 0) {
		printf("Failed to pack procedure to packet\n");
		packet->data[0] = PROC_PULL_RESPONSE;
		packet->data[0] |= PROC_FLAG_END;
		packet->data[0] |= PROC_FLAG_ERROR;
		packet->length = 1;
		csp_sendto_reply(packet, packet, CSP_O_SAME);
		return;
	}

	packet->data[0] = PROC_PULL_RESPONSE;
	packet->data[0] |= PROC_FLAG_END;

	csp_sendto_reply(packet, packet, CSP_O_SAME);
}

static void proc_serve_push_request(csp_packet_t * packet) {
	proc_t * procedure = malloc(sizeof(proc_t));
	if (procedure == NULL) {
		printf("Failed to allocate memory for procedure\n");
		packet->data[0] = PROC_PUSH_RESPONSE;
		packet->data[0] |= PROC_FLAG_END;
		packet->data[0] |= PROC_FLAG_ERROR;
		packet->length = 1;
		return;
	}

	int ret = unpack_proc_from_csp_packet(procedure, packet);
	if (ret < 0) {
		printf("Failed to unpack procedure from packet\n");
		free_proc(procedure);
		packet->data[0] = PROC_PUSH_RESPONSE;
		packet->data[0] |= PROC_FLAG_END;
		packet->data[0] |= PROC_FLAG_ERROR;
		packet->length = 1;
		csp_sendto_reply(packet, packet, CSP_O_SAME);
		return;
	}

	ret = set_proc(procedure, packet->data[1], 0);
	if (ret < 0) {
		printf("Failed to set procedure\n");
		free_proc(procedure);
		packet->data[0] = PROC_PUSH_RESPONSE;
		packet->data[0] |= PROC_FLAG_END;
		packet->data[0] |= PROC_FLAG_ERROR;
		packet->length = 1;
		csp_sendto_reply(packet, packet, CSP_O_SAME);
		return;
	}

	packet->data[0] = PROC_PUSH_RESPONSE;
	packet->data[0] |= PROC_FLAG_END;
	packet->length = 1;

	csp_sendto_reply(packet, packet, CSP_O_SAME);
}

static void proc_serve_slots_request(csp_packet_t * packet) {
	int * slots = get_proc_slots();

	packet->data[0] = PROC_SLOTS_RESPONSE;
	packet->data[0] |= PROC_FLAG_END;
	packet->length = 1;

	for (int i = 0; slots[i] != -1; i++) {
		packet->data[i + 1] = slots[i];
		packet->length++;
	}

	free(slots);
	csp_sendto_reply(packet, packet, CSP_O_SAME);
}

static void proc_serve_run_request(csp_packet_t * packet) {
	uint8_t slot = packet->data[1];

	if (proc_runtime_run == NULL) {
		printf("No csp_proc runtime available\n");
		packet->data[0] = PROC_RUN_RESPONSE;
		packet->data[0] |= PROC_FLAG_END;
		packet->data[0] |= PROC_FLAG_ERROR;
		packet->length = 1;
		csp_sendto_reply(packet, packet, CSP_O_SAME);
		return;
	}

	int ret = proc_runtime_run(slot);
	if (ret != 0) {
		printf("Failed to run procedure\n");
		packet->data[0] = PROC_RUN_RESPONSE;
		packet->data[0] |= PROC_FLAG_END;
		packet->data[0] |= PROC_FLAG_ERROR;
		packet->length = 1;
		csp_sendto_reply(packet, packet, CSP_O_SAME);
		return;
	}

	packet->data[0] = PROC_RUN_RESPONSE;
	packet->data[0] |= PROC_FLAG_END;
	packet->length = 1;

	csp_sendto_reply(packet, packet, CSP_O_SAME);
}

void proc_serve(csp_packet_t * packet) {
	switch (packet->data[0] & PROC_TYPE_MASK) {
		case PROC_DEL_REQUEST:
			proc_serve_del_request(packet);
			break;
		case PROC_PULL_REQUEST:
			proc_serve_pull_request(packet);
			break;
		case PROC_PUSH_REQUEST:
			proc_serve_push_request(packet);
			break;
		case PROC_SLOTS_REQUEST:
			proc_serve_slots_request(packet);
			break;
		case PROC_RUN_REQUEST:
			proc_serve_run_request(packet);
			break;
		default:
			printf("Unknown procedure request\n");
			csp_buffer_free(packet);
			break;
	}
}
