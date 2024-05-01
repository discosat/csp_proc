#include <csp_proc/proc_store.h>
#include <csp_proc/proc_mutex.h>
#include <csp_proc/proc_memory.h>

proc_t * proc_store[MAX_PROC_SLOT + 1];
proc_mutex_t * proc_store_mutex = NULL;

int _delete_proc(uint8_t slot) {
	if (slot < 0 || slot > MAX_PROC_SLOT) {
		return -1;
	}
	if (proc_store[slot] != NULL) {
		for (int i = 0; i < proc_store[slot]->instruction_count; i++) {
			proc_free_instruction(&proc_store[slot]->instructions[i]);
		}
		proc_free(proc_store[slot]);
		proc_store[slot] = NULL;
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
	// Free any existing procedure in the slot
	if (proc_store[slot] != NULL) {
		if (!overwrite) {
			return -1;
		}

		proc_free(proc_store[slot]);
	}

	proc_store[slot] = proc_malloc(sizeof(proc_t));
	if (proc_store[slot] == NULL) {
		return -1;
	}

	memcpy(proc_store[slot], proc, sizeof(proc_t));

	return slot;
}

proc_t * get_proc(uint8_t slot) {
	if (proc_store[slot] == NULL) {
		return NULL;
	}
	return proc_store[slot];
}

int * get_proc_slots() {
	int * slots = proc_malloc((MAX_PROC_SLOT + 2) * sizeof(int));
	int count = 0;

	if (proc_mutex_take(proc_store_mutex) != PROC_MUTEX_OK) {
		return NULL;
	}
	for (int i = 0; i < MAX_PROC_SLOT + 1; i++) {
		if (proc_store[i] != NULL) {
			slots[count++] = i;
		}
	}
	slots[count] = -1;  // Terminate the array with -1

	proc_mutex_give(proc_store_mutex);
	return slots;
}
