// Default FreeRTOS implementation of csp_proc runtime

#include <csp_proc/proc_runtime.h>
#include <csp_proc/proc_analyze.h>
#include <csp_proc/proc_memory.h>

#include <csp/csp.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#ifndef PROC_RUNTIME_TASK_SIZE
#define PROC_RUNTIME_TASK_SIZE (512U)
#endif

#ifndef PROC_RUNTIME_TASK_PRIORITY
#define PROC_RUNTIME_TASK_PRIORITY (tskIDLE_PRIORITY + 2U)
#endif

// forward declaration
int dsl_proc_exec(proc_union_t proc_union);

typedef struct {
	proc_union_t * proc_union;
	TaskHandle_t task_handle;
} task_t;

task_t * running_tasks = NULL;
volatile size_t running_tasks_count = 0;
SemaphoreHandle_t running_tasks_mutex;

int proc_runtime_init() {
	running_tasks_mutex = xSemaphoreCreateMutex();
	if (running_tasks_mutex == NULL) {
		return -1;
	}
	return 0;
}

/**
 * Stop a runtime task and free its resources.
 *
 * @param task The task to stop
 * @return 0 on success, -1 on failure
 */
int proc_stop_runtime_task(TaskHandle_t task_handle) {
	int ret = xSemaphoreTake(running_tasks_mutex, portMAX_DELAY);  // Prevent race condition on running_tasks array
	if (ret != pdTRUE) {
		return -1;
	}

	for (size_t i = 0; i < running_tasks_count; i++) {
		if (running_tasks[i].task_handle == task_handle) {
			vTaskDelete(running_tasks[i].task_handle);
			if (running_tasks[i].proc_union->type == PROC_TYPE_DSL) {
				free_proc(running_tasks[i].proc_union->proc.dsl_proc);
			}
			running_tasks[i] = running_tasks[running_tasks_count - 1];
			running_tasks = proc_realloc(running_tasks, --running_tasks_count * sizeof(task_t));
			break;
		}
	}
	xSemaphoreGive(running_tasks_mutex);

	return 0;
}

/**
 * Stop all currently running runtime tasks.
 *
 * @return 0 on success, -1 on failure
 */
int proc_stop_all_runtime_tasks() {
	int inf_loop_guard = 0;
	while (running_tasks_count > 0 && (inf_loop_guard++ < 1000)) {
		if (proc_stop_runtime_task(&running_tasks[0]) != 0) {
			return -1;
		}
	}
	return inf_loop_guard < 1000 ? 0 : -1;
}

void runtime_task(void * pvParameters) {
	proc_union_t * proc_union = (proc_union_t *)pvParameters;

	int ret;
	switch (proc_union->type) {
		case PROC_TYPE_DSL:
			ret = dsl_proc_exec(*proc_union);
			break;
		case PROC_TYPE_COMPILED:
			ret = proc_union->proc.compiled_proc();
			break;
		default:
			ret = -1;
	}
	// TODO: set error flag param if ret != 0

	// Procedure finished, clean up
	if (xSemaphoreTake(running_tasks_mutex, portMAX_DELAY) != pdTRUE) {
		vTaskDelete(NULL);
		return;
	}
	TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();
	for (size_t i = 0; i < running_tasks_count; i++) {
		if (running_tasks[i].task_handle == task_handle) {
			running_tasks[i] = running_tasks[running_tasks_count - 1];
			running_tasks = proc_realloc(running_tasks, --running_tasks_count * sizeof(task_t));
			break;
		}
	}
	xSemaphoreGive(running_tasks_mutex);
	proc_free(proc_union);
	csp_print("Procedure finished (%s)\n", pcTaskGetName(task_handle));
	vTaskDelete(NULL);
}

int proc_runtime_run(uint8_t proc_slot) {
	csp_print("Running procedure %d\n", proc_slot);
	if (running_tasks_count >= MAX_PROC_CONCURRENT) {
		csp_print("Maximum number of concurrent procedures reached\n");
		return -1;
	}

	proc_union_t * stored_proc = proc_malloc(sizeof(proc_union_t));
	*stored_proc = get_proc(proc_slot);

	if (stored_proc->type != PROC_TYPE_DSL && stored_proc->type != PROC_TYPE_COMPILED) {
		csp_print("Procedure in slot %d not found\n", proc_slot);
		return -1;
	}

	if (stored_proc->type == PROC_TYPE_DSL) {
		// Copy procedure to detach from proc store
		proc_t * detached_proc = proc_malloc(sizeof(proc_t));
		if (deepcopy_proc(stored_proc->proc.dsl_proc, detached_proc) != 0) {
			csp_print("Failed to copy procedure\n");
			return -1;
		}
		stored_proc->proc.dsl_proc = detached_proc;
	}

	// Create task
	if (xSemaphoreTake(running_tasks_mutex, portMAX_DELAY) != pdTRUE) {  // taking mutex early to prevent clean-up from the newly spawned task before it's added to the task array
		if (stored_proc->type == PROC_TYPE_DSL) {
			free_proc(stored_proc->proc.dsl_proc);
		}
		proc_free(stored_proc);
		return -1;
	}

	TaskHandle_t task_handle;
	char task_name[configMAX_TASK_NAME_LEN];
	sprintf(task_name, "RNTM%d", proc_slot);
	BaseType_t task_create_ret;
	task_create_ret = xTaskCreate(runtime_task, task_name, PROC_RUNTIME_TASK_SIZE, stored_proc, PROC_RUNTIME_TASK_PRIORITY, &task_handle);

	if (task_create_ret != pdPASS) {
		csp_print("Failed to create task\n");
		if (stored_proc->type == PROC_TYPE_DSL) {
			free_proc(stored_proc->proc.dsl_proc);
		}
		proc_free(stored_proc);
		xSemaphoreGive(running_tasks_mutex);
		return -1;
	}

	// Add task to array
	running_tasks = proc_realloc(running_tasks, ++running_tasks_count * sizeof(task_t));
	running_tasks[running_tasks_count - 1] = (task_t){.proc_union = stored_proc, .task_handle = task_handle};
	xSemaphoreGive(running_tasks_mutex);

	return 0;
}
