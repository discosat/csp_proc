#ifndef CSP_PROC_MEMORY_H
#define CSP_PROC_MEMORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void * proc_malloc(size_t size);

void * proc_calloc(size_t num, size_t size);

void * proc_realloc(void * ptr, size_t size);

void proc_free(void * ptr);

char * proc_strdup(const char * str);

#ifdef __cplusplus
}
#endif

#endif  // CSP_PROC_MEMORY_H
