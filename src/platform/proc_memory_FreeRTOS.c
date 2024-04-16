#include <csp_proc/proc_memory.h>
#include <FreeRTOS.h>

/**
 * Allocates a block of memory of the specified size.
 * An extra sizeof(size_t) bytes are allocated at the beginning of the block to store the size of the block.
 * This is used by proc_realloc to know how much data to copy to the new block.
 */
void * proc_malloc(size_t size) {
	size_t * actual_ptr = pvPortMalloc(size + sizeof(size_t));
	if (actual_ptr == NULL) {
		return NULL;
	}
	*actual_ptr = size;
	return actual_ptr + 1;
}

/**
 * Allocates a block of memory for an array of num elements, each of them size bytes long, and initializes all its bits to zero.
 * An extra sizeof(size_t) bytes are allocated at the beginning of the block to store the size of the block.
 * This is used by proc_realloc to know how much data to copy to the new block.
 */
void * proc_calloc(size_t num, size_t size) {
	size_t total_size = num * size;
	size_t * actual_ptr = pvPortMalloc(total_size + sizeof(size_t));
	if (actual_ptr == NULL) {
		return NULL;
	}
	*actual_ptr = total_size;
	memset(actual_ptr + 1, 0, total_size);
	return actual_ptr + 1;
}

/**
 * Changes the size of the memory block pointed to by ptr to size bytes.
 * The function may move the memory block to a new location, in which case the new location is returned.
 * The content of the memory block is preserved up to the lesser of the new and old sizes, even if the block is moved.
 * If the new size is larger, the value of the newly allocated portion is indeterminate.
 * If ptr is NULL, the function behaves like malloc for the specified size.
 * If size is zero, the memory previously allocated at ptr is deallocated as if a call to free was made, and a NULL pointer is returned.
 */
void * proc_realloc(void * ptr, size_t size) {
	if (size == 0) {
		proc_free(ptr);
		return NULL;
	} else if (ptr == NULL) {
		return proc_malloc(size);
	} else {
		size_t * actual_ptr = (size_t *)ptr - 1;
		size_t old_size = *actual_ptr;
		size_t * new_ptr = pvPortMalloc(size + sizeof(size_t));
		if (new_ptr) {
			*new_ptr = size;
			memcpy(new_ptr + 1, ptr, old_size < size ? old_size : size);
			vPortFree(actual_ptr);
		}
		return new_ptr ? new_ptr + 1 : NULL;
	}
}

/**
 * Deallocates the memory previously allocated by proc_malloc, proc_calloc or proc_realloc.
 * If ptr is a NULL pointer, no operation is performed.
 */
void proc_free(void * ptr) {
	if (ptr) {
		size_t * actual_ptr = (size_t *)ptr - 1;
		vPortFree(actual_ptr);
	}
}

/**
 * Allocates a new string which is a duplicate of the string str (strdup).
 * The memory for the new string is obtained using proc_malloc.
 */
char * proc_strdup(const char * s) {
	size_t len = strlen(s) + 1;
	char * new = proc_malloc(len);
	if (new == NULL) {
		return NULL;
	}
	return memcpy(new, s, len);
}
