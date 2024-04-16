// Default POSIX implementation of csp_proc runtime

#include <csp_proc/proc_runtime.h>
#include <csp_proc/proc_analyze.h>
#include <csp_proc/proc_memory.h>

#include <csp/csp.h>

#include <pthread.h>
#include <stdlib.h>

// forward declaration
int proc_instructions_exec(proc_t * proc, proc_analysis_t * analysis);

typedef struct {
	proc_t * proc;
	pthread_t thread;
} thread_t;

thread_t * running_threads = NULL;
volatile size_t running_threads_count = 0;
pthread_mutex_t running_threads_mutex;

pthread_key_t recursion_depth_key;

int proc_runtime_init() {
	if (pthread_mutex_init(&running_threads_mutex, NULL) != 0) {
		return -1;
	}
	return 0;
}

/**
 * Stop a runtime thread and free its resources.
 *
 * @param thread The thread to stop
 * @return 0 on success, -1 on failure
 */
int proc_stop_runtime_thread(pthread_t thread) {
	int ret = pthread_mutex_lock(&running_threads_mutex);  // Prevent race condition on running_threads array
	if (ret != 0) {
		return -1;
	}

	for (size_t i = 0; i < running_threads_count; i++) {
		if (pthread_equal(running_threads[i].thread, thread)) {
			pthread_cancel(running_threads[i].thread);
			pthread_join(running_threads[i].thread, NULL);
			free_proc(running_threads[i].proc);
			running_threads[i] = running_threads[running_threads_count - 1];
			running_threads = proc_realloc(running_threads, --running_threads_count * sizeof(thread_t));
			break;
		}
	}
	pthread_mutex_unlock(&running_threads_mutex);

	return 0;
}

/**
 * Stop all currently running runtime threads.
 *
 * @return 0 on success, -1 on failure
 */
int proc_stop_all_runtime_threads() {
	int inf_loop_guard = 0;
	while (running_threads_count > 0 && (inf_loop_guard++ < 1000)) {
		if (proc_stop_runtime_thread(running_threads[0].thread) != 0) {
			return -1;
		}
	}
	return inf_loop_guard < 1000 ? 0 : -1;
}

void * runtime_thread(void * pvParameters) {
	proc_t * proc = (proc_t *)pvParameters;

	// Perform static analysis
	proc_analysis_config_t analysis_config = {
		.analyzed_procs = proc_calloc(MAX_PROC_SLOT + 1, sizeof(int)),
		.analyses = proc_calloc(MAX_PROC_SLOT + 1, sizeof(proc_analysis_t *)),
		.analyzed_proc_count = 0};
	proc_analysis_t * analysis = proc_malloc(sizeof(proc_analysis_t));
	if (analysis == NULL) {
		csp_print("Error allocating memory for analysis\n");
		return NULL;
	}

	if (proc_analyze(proc, analysis, &analysis_config) != 0) {
		csp_print("Error analyzing procedure\n");
		return NULL;
	}

	if (pthread_key_create(&recursion_depth_key, NULL) != 0) {
		csp_print("Error creating pthread key\n");
		return NULL;
	}

	int ret = proc_instructions_exec(proc, analysis);  // TODO: set error flag param

	// Procedure finished, clean up
	pthread_mutex_lock(&running_threads_mutex);
	pthread_t thread = pthread_self();
	for (size_t i = 0; i < running_threads_count; i++) {
		if (pthread_equal(running_threads[i].thread, thread)) {
			running_threads[i] = running_threads[running_threads_count - 1];
			running_threads = proc_realloc(running_threads, --running_threads_count * sizeof(thread_t));
			break;
		}
	}
	pthread_mutex_unlock(&running_threads_mutex);
	csp_print("Procedure finished\n");
	free_proc_analysis(analysis);
	free_proc(proc);
	return NULL;
}

int proc_runtime_run(uint8_t proc_slot) {
	csp_print("Running procedure %d\n", proc_slot);
	if (running_threads_count >= MAX_PROC_CONCURRENT) {
		csp_print("Maximum number of concurrent procedures reached\n");
		return -1;
	}

	proc_t * stored_proc = get_proc(proc_slot);

	if (stored_proc == NULL) {
		csp_print("Procedure in slot %d not found\n", proc_slot);
		return -1;
	}
	if (stored_proc->instruction_count == 0) {
		csp_print("Procedure in slot %d has no instructions\n", proc_slot);
		return -1;
	}

	// Copy procedure to detach from proc store
	proc_t * detached_proc = proc_malloc(sizeof(proc_t));
	if (deepcopy_proc(stored_proc, detached_proc) != 0) {
		csp_print("Failed to copy procedure\n");
		return -1;
	}

	// Create thread
	pthread_mutex_lock(&running_threads_mutex);  // taking mutex early to prevent clean-up from the newly spawned thread before it's added to the thread array
	pthread_t thread;
	if (pthread_create(&thread, NULL, runtime_thread, detached_proc) != 0) {
		csp_print("Failed to create thread\n");
		free_proc(detached_proc);
		pthread_mutex_unlock(&running_threads_mutex);
		return -1;
	}

	// Add thread to array
	running_threads = proc_realloc(running_threads, ++running_threads_count * sizeof(thread_t));
	running_threads[running_threads_count - 1] = (thread_t){.proc = detached_proc, .thread = thread};
	pthread_mutex_unlock(&running_threads_mutex);

	return 0;
}
