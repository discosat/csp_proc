#include <csp_proc/proc_memory.h>
#include <stdlib.h>

void * proc_malloc(size_t size) {
	return malloc(size);
}

void * proc_calloc(size_t num, size_t size) {
	return calloc(num, size);
}

void * proc_realloc(void * ptr, size_t size) {
	return realloc(ptr, size);
}

void proc_free(void * ptr) {
	free(ptr);
}

char * proc_strdup(const char * str) {
	return strdup(str);
}
