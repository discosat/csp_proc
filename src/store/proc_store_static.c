#include <csp_proc/proc_store.h>
#include <csp_proc/proc_mutex.h>
#include <stdlib.h>

proc_t proc_store[MAX_PROC_SLOT + 1] = {0};
proc_mutex_t * proc_store_mutex = NULL;

int _delete_proc(uint8_t slot) {
	if (slot < 0 || slot > MAX_PROC_SLOT) {
		return -1;
	}
	for (int i = 0; i < proc_store[slot].instruction_count; i++) {
		proc_free_instruction(&proc_store[slot].instructions[i]);
	}
	proc_store[slot].instruction_count = 0;
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

void reset_proc_store() {
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

int set_proc(proc_t * proc, uint8_t slot, int overwrite) {
	if (slot < 0 || slot > MAX_PROC_SLOT) {
		return -1;
	}
	if (proc_mutex_take(proc_store_mutex) != PROC_MUTEX_OK) {
		return PROC_MUTEX_ERR;
	}

	int ret = -1;
	if (proc_store[slot].instruction_count == 0) {
		_delete_proc(slot);
		proc_store[slot] = *proc;
		ret = slot;
	} else if (overwrite) {
		_delete_proc(slot);
		proc_store[slot] = *proc;
		ret = slot;
	}
	proc_mutex_give(proc_store_mutex);
	return ret;
}

proc_t * get_proc(uint8_t slot) {
	if (slot < 0 || slot > MAX_PROC_SLOT) {
		return NULL;
	}

	if (proc_mutex_take(proc_store_mutex) != PROC_MUTEX_OK) {
		return PROC_MUTEX_ERR;
	}

	if (proc_store[slot].instruction_count == 0) {
		proc_mutex_give(proc_store_mutex);
		return NULL;
	}

	proc_mutex_give(proc_store_mutex);
	return &proc_store[slot];
}

int * get_proc_slots() {
	int * slots = malloc((MAX_PROC_SLOT + 2) * sizeof(int));
	int count = 0;

	if (proc_mutex_take(proc_store_mutex) != PROC_MUTEX_OK) {
		return NULL;
	}
	for (int i = 0; i < MAX_PROC_SLOT + 1; i++) {
		if (proc_store[i].instruction_count > 0) {
			slots[count++] = i;
		}
	}
	slots[count] = -1;  // Terminate the array with -1

	proc_mutex_give(proc_store_mutex);
	return slots;
}
