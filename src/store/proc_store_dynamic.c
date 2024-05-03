#include <csp_proc/proc_store.h>
#include <csp_proc/proc_mutex.h>
#include <csp_proc/proc_memory.h>

compiled_proc_t proc_reserved_slots_array[RESERVED_PROC_SLOTS];
proc_t * proc_store[MAX_PROC_SLOT + 1 - RESERVED_PROC_SLOTS];
proc_mutex_t * proc_store_mutex = NULL;

int _delete_proc(uint8_t slot) {
	if (slot < RESERVED_PROC_SLOTS || slot > MAX_PROC_SLOT) {
		return -1;
	}
	int shifted_slot = slot - RESERVED_PROC_SLOTS;
	if (proc_store[shifted_slot] != NULL) {
		for (int i = 0; i < proc_store[shifted_slot]->instruction_count; i++) {
			proc_free_instruction(&proc_store[shifted_slot]->instructions[i]);
		}
		proc_free(proc_store[shifted_slot]);
		proc_store[shifted_slot] = NULL;
	}
	return 0;
}

int delete_proc(uint8_t slot) {
	if (proc_mutex_take(proc_store_mutex) != PROC_MUTEX_OK) {
		return PROC_MUTEX_ERR;
	}
	int ret = _delete_proc(slot);
	proc_mutex_give(proc_store_mutex);
	return ret;
}

int reset_proc_store() {
	if (proc_mutex_take(proc_store_mutex) != PROC_MUTEX_OK) {
		return PROC_MUTEX_ERR;
	}
	for (int i = 0; i < MAX_PROC_SLOT + 1; i++) {
		_delete_proc(i);
	}
	proc_mutex_give(proc_store_mutex);
}

int proc_store_init() {
	proc_store_mutex = proc_mutex_create();
	if (proc_store_mutex == NULL) {
		return -1;
	}
	return 0;
}

void destroy_proc_store() {
	if (proc_store != NULL) {
		for (int i = 0; i < MAX_PROC_SLOT + 1; i++) {
			_delete_proc(i);
		}
	}
	if (proc_store_mutex != NULL) {
		proc_mutex_destroy(proc_store_mutex);
		proc_store_mutex = NULL;
	}
}

int set_proc(proc_t * proc, uint8_t slot, int overwrite) {
	if (slot < RESERVED_PROC_SLOTS || slot > MAX_PROC_SLOT) {
		return -1;
	}

	if (proc_mutex_take(proc_store_mutex) != PROC_MUTEX_OK) {
		return PROC_MUTEX_ERR;
	}

	int shifted_slot = slot - RESERVED_PROC_SLOTS;
	// Free any existing procedure in the slot
	if (proc_store[shifted_slot] != NULL) {
		if (!overwrite) {
			proc_mutex_give(proc_store_mutex);
			return -1;
		}

		proc_free(proc_store[shifted_slot]);
	}

	proc_store[shifted_slot] = proc_malloc(sizeof(proc_t));
	if (proc_store[shifted_slot] == NULL) {
		proc_mutex_give(proc_store_mutex);
		return -1;
	}

	memcpy(proc_store[shifted_slot], proc, sizeof(proc_t));

	proc_mutex_give(proc_store_mutex);
	return slot;
}

proc_union_t get_proc(uint8_t slot) {
	proc_union_t proc_union;
	if (slot < RESERVED_PROC_SLOTS) {
		proc_union.type = PROC_TYPE_COMPILED;
		proc_union.proc.compiled_proc = proc_reserved_slots_array[slot];
	} else {
		proc_union.type = PROC_TYPE_DSL;
		proc_union.proc.dsl_proc = proc_store[slot - RESERVED_PROC_SLOTS];
	}
	if (proc_union.proc.compiled_proc == NULL && proc_union.proc.dsl_proc == NULL) {
		proc_union.type = PROC_TYPE_NONE;
	}
	return proc_union;
}

int * get_proc_slots() {
	int * slots = proc_malloc((MAX_PROC_SLOT + 2) * sizeof(int));
	int count = 0;

	if (proc_mutex_take(proc_store_mutex) != PROC_MUTEX_OK) {
		return NULL;
	}
	for (int i = 0; i < RESERVED_PROC_SLOTS; i++) {
		if (proc_reserved_slots_array[i] != NULL) {
			slots[count++] = i;
		}
	}
	for (int i = RESERVED_PROC_SLOTS; i < MAX_PROC_SLOT + 1; i++) {
		if (proc_store[i - RESERVED_PROC_SLOTS] != NULL) {
			slots[count++] = i;
		}
	}
	slots[count] = -1;  // Terminate the array with -1

	proc_mutex_give(proc_store_mutex);
	return slots;
}
