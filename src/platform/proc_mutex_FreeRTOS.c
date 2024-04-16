#include <csp_proc/proc_mutex.h>
#include <csp_proc/proc_memory.h>
#include <FreeRTOS.h>
#include <semphr.h>

struct proc_mutex_t {
	SemaphoreHandle_t handle;
};

proc_mutex_t * proc_mutex_create() {
	proc_mutex_t * mutex = proc_malloc(sizeof(proc_mutex_t));
	mutex->handle = xSemaphoreCreateMutex();
	if (mutex->handle == NULL) {
		proc_free(mutex);
		return NULL;
	}
	return mutex;
}

int proc_mutex_take(proc_mutex_t * mutex) {
	if (xSemaphoreTake(mutex->handle, portMAX_DELAY) != pdTRUE) {
		return -1;
	}
	return 0;
}

int proc_mutex_give(proc_mutex_t * mutex) {
	if (xSemaphoreGive(mutex->handle) != pdTRUE) {
		return -1;
	}
	return 0;
}

void proc_mutex_destroy(proc_mutex_t * mutex) {
	vSemaphoreDelete(mutex->handle);
	proc_free(mutex);
}
