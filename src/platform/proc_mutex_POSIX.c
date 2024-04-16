#include <csp_proc/proc_mutex.h>
#include <csp_proc/proc_memory.h>
#include <pthread.h>

struct proc_mutex_t {
	pthread_mutex_t handle;
};

proc_mutex_t * proc_mutex_create() {
	proc_mutex_t * mutex = proc_malloc(sizeof(proc_mutex_t));
	if (pthread_mutex_init(&mutex->handle, NULL) != 0) {
		proc_free(mutex);
		return NULL;
	}
	return mutex;
}

int proc_mutex_take(proc_mutex_t * mutex) {
	if (pthread_mutex_lock(&mutex->handle) != 0) {
		return -1;
	}
	return 0;
}

int proc_mutex_give(proc_mutex_t * mutex) {
	if (pthread_mutex_unlock(&mutex->handle) != 0) {
		return -1;
	}
	return 0;
}

void proc_mutex_destroy(proc_mutex_t * mutex) {
	pthread_mutex_destroy(&mutex->handle);
	proc_free(mutex);
}
