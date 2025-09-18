/**
 * @file rcwstr.c
 * @author Daniel Starke
 * @see rcwstr.h
 * @date 2025-07-23
 * @version 2025-07-27
 */
#include <stdlib.h>
#include "rcwstr.h"


/**
 * Creates a new reference counted wide-character string.
 *
 * @param[in] str - string to duplicate (can be `NULL`)
 * @return allocated object or `NULL` on allocation error
 */
tRcWStr * rws_create(const wchar_t * str) {
	size_t len = (((str != NULL) ? wcslen(str) : 0) + 1);
	size_t size = sizeof(tRcWStr) + (len * sizeof(wchar_t));
	tRcWStr * res = malloc(size);
	if (res != NULL) {
		res->refCount = 1;
		if (str != NULL) {
			memcpy(res->ptr, str, len * sizeof(wchar_t));
		} else {
			res->ptr[0] = 0;
		}
	}
	return res;
}


/**
 * Increases the reference counter of the given object.
 *
 * @param[in,out] s - reference counted wide-character string
 * @return `s`
 */
tRcWStr * rws_aquire(tRcWStr * s) {
	if (s == NULL) {
		return NULL;
	}
	InterlockedIncrement(&(s->refCount));
	return s;
}


/**
 * Decreases the reference counter of the given object.
 * Frees the object if the reference counter reaches zero.
 * This sets the given object pointer to `NULL`.
 *
 * @param[in,out] s - pointer reference counted wide-character string
 */
void rws_release(tRcWStr ** s) {
	if (s == NULL || *s == NULL) {
		return;
	}
	if (InterlockedDecrement(&((*s)->refCount)) == 0) {
		free(*s);
	}
	*s = NULL;
}
