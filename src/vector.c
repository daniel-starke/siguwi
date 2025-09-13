/**
 * @file vector.c
 * @author Daniel Starke
 * @see vector.h
 * @date 2018-04-06
 * @version 2025-07-27
 */
#include <stdlib.h>
#include <string.h>
#include "vector.h"


/**
 * Defines the initial capacity of a vector in number of elements.
 */
#ifndef LIBPCF_VEC_INIT_CAPACITY
#define LIBPCF_VEC_INIT_CAPACITY 16
#endif /* LIBPCF_VEC_INIT_CAPACITY */


/**
 * Internal helper function to swap two memory blocks.
 *
 * @param[in,out] lhs - left-hand side block pointer
 * @param[in,out] rhs - right-hand side block pointer
 * @param[in] n - number of bytes block size
 * @remarks Loop unroll and vector optimization by the compiler is recommended.
 */
static void libpcf_swap_internal(void * const lhs, void * const rhs, const size_t n) {
	uint8_t * p;
	uint8_t * q;
	uint8_t * const lhsEnd = (uint8_t *)lhs + n;

	for (p = lhs, q = rhs; p < lhsEnd; ++p, ++q) {
		const uint8_t t = *p;
		*p = *q;
		*q = t;
	}
}


/**
 * The function creates a new instance of a vector.
 *
 * @param[in] dataSize - size of the data for each element in bytes
 * @return returns the created vector instance or NULL
 */
tVector * vec_create(const size_t dataSize) {
	tVector * obj;
	if (dataSize < 1) return NULL;
	obj = (tVector *)malloc(sizeof(tVector));
	if (obj == NULL) return NULL;
	obj->capacity = 0;
	obj->elementSize = dataSize;
	obj->size = 0;
	obj->buffer = NULL;
	return obj;
}


/**
 * The function creates a new instance of a vector based
 * on the passed instance.
 *
 * @param[in,out] v - a vector instance
 * @return returns the created vector instance or NULL
 */
tVector * vec_clone(const tVector * const v) {
	if (v == NULL) return NULL;
	tVector * obj = (tVector *)malloc(sizeof(tVector));
	if (obj == NULL) return NULL;
	const size_t bufferSize = v->capacity * v->elementSize;
	uint8_t * buffer = (uint8_t *)malloc(bufferSize);
	if (buffer == NULL) {
		free(obj);
		return NULL;
	};
	memcpy(obj, v, sizeof(tVector));
	memcpy(buffer, v->buffer, bufferSize);
	obj->buffer = buffer;
	return obj;
}


/**
 * Pushes a new element to the end of the vector.
 *
 * @param[in,out] v - a vector instance
 * @return pointer to the newly added element data, NULL on error
 */
void * vec_pushBack(tVector * const v) {
	if (v == NULL) return NULL;
	if (v->size >= v->capacity) {
		if (vec_reserve(v, v->capacity > 0 ? (v->capacity * 2) : LIBPCF_VEC_INIT_CAPACITY) != 1) {
			return NULL;
		}
	}
	v->size++;
	return vec_back(v);
}


/**
 * Removes the last element from the vector.
 *
 * @param[in,out] v - a vector instance
 * @return 1 on success, else 0
 */
int vec_popBack(tVector * const v) {
	if (v == NULL) return 0;
	if (v->size <= 0) return 0;
	v->size--;
	return 1;
}


/**
 * Returns the pointer to the first element within the vector.
 * NULL is returned if there is no element in the buffer or an error
 * occurred.
 *
 * @param[in,out] v - a vector instance
 * @return pointer to the first element, else NULL on error
 */
void * vec_front(tVector * const v) {
	if (v == NULL) return NULL;
	if (v->size < 1) return NULL;
	return (void *)(v->buffer);
}


/**
 * Returns the pointer to the last element within the vector.
 * NULL is returned if there is no element in the buffer or an error
 * occurred.
 *
 * @param[in,out] v - a vector instance
 * @return pointer to the last element, else NULL on error
 */
void * vec_back(tVector * const v) {
	if (v == NULL) return NULL;
	if (v->size < 1) return NULL;
	return (void *)(v->buffer + ((v->size - 1) * v->elementSize));
}


/**
 * Appends the passed vector to the end of the current vector.
 * The pointer to the first element of the newly added element
 * list is returned on success or NULL if an error occurred.
 *
 * @param[in,out] v - a vector instance
 * @param[in] src - a vector instance
 * @return pointer to the first newly added element, else NULL on error
 */
void * vec_append(tVector * const v, const tVector * const src) {
	if (v == NULL || src == NULL) return NULL;
	if (src->size <= 0 || v->elementSize != src->elementSize) return NULL;
	const size_t srcSize = src->size;
	const size_t oldSize = v->size;
	if (vec_reserve(v, v->size + src->size) != 1) return NULL;
	v->size += srcSize;
	memcpy(v->buffer + (oldSize * v->elementSize), src->buffer, srcSize * v->elementSize);
	return vec_at(v, oldSize);
}


/**
 * Appends the passed array to the end of the current vector.
 * The pointer to the first element of the newly added element
 * list is returned on success or NULL if an error occurred.
 *
 * @param[in,out] v - a vector instance
 * @param[in] src - an element array
 * @param[in] size - number of elements in src
 * @return pointer to the first newly added element, else NULL on error
 */
void * vec_appendArray(tVector * const v, const void * const src, const size_t size) {
	if (v == NULL || src == NULL || size < 1) return NULL;
	if (vec_reserve(v, v->size + size) != 1) return NULL;
	const size_t oldSize = v->size;
	v->size += size;
	memcpy(v->buffer + (oldSize * v->elementSize), src, size * v->elementSize);
	return vec_at(v, oldSize);
}


/**
 * Removes size elements starting at position i from the given vector.
 *
 * @param[in,out] v - a vector instance
 * @param[in] i - element index
 * @param[in] size - number of elements
 * @return 1 on success, 0 on error
 */
int vec_erase(tVector * const v, const size_t i, const size_t size) {
	if (v == NULL) return 0;
	if (i >= v->size) return 0;
	if (size < 1) return 1;
	if ((i + size) < v->size) {
		memmove(
			v->buffer + (i * v->elementSize),
			v->buffer + ((i + size) * v->elementSize),
			size * v->elementSize
		);
		v->size -= size;
	} else {
		v->size = i;
	}
	return 1;
}


/**
 * Returns the pointer to a single element within the vector at the given
 * index. NULL is returned if the index is out of bounds or an error
 * occurred.
 *
 * @param[in,out] v - a vector instance
 * @param[in] i - element index
 * @return pointer to the element at index i, else NULL on error
 */
void * vec_at(tVector * const v, const size_t i) {
	if (v == NULL) return NULL;
	if (i >= v->size) return NULL;
	return (void *)(v->buffer + (i * v->elementSize));
}


/**
 * The function traverses through all elements from the passed
 * vector and calls the specific callback function.
 *
 * @param[in] v - a vector instance
 * @param[in] v - callback function which is called for every element
 * @param[in] param - user defined parameter
 * @return 1 on success, 0 on error
 * @see VectorVisitor
 */
int vec_traverse(const tVector * const v, VectorVisitor cb, void * param) {
	if (v == NULL || cb == NULL) return 0;
	const size_t elementSize = v->elementSize;
	const size_t size = v->size;
	uint8_t * e = v->buffer;
	for (size_t i = 0; i < size; i++) {
		if ((* cb)(i, (void *)e, param) == 0) return 0;
		e += elementSize;
	}
	return 1;
}


/**
 * The function returns 1 if the vector is empty and 0 otherwise.
 *
 * @param[in] v - a vector instance
 * @return 1 if empty, else 0
 */
int vec_empty(const tVector * const v) {
	if (v == NULL) return 1;
	return (v->size == 0) ? 1 : 0;
}


/**
 * The function returns the number of elements in the passed vector.
 *
 * @param[in] v - a vector instance
 * @return number of elements within the vector
 */
size_t vec_size(const tVector * const v) {
	if (v == NULL) return 0;
	return v->size;
}


/**
 * The function returns the number of elements the passed vector
 * can store at most before a resize is needed.
 *
 * @param[in] v - a vector instance
 * @return maximum number of elements the vector can currently store
 */
size_t vec_capacity(const tVector * const v) {
	if (v == NULL) return 0;
	return v->capacity;
}


/**
 * The function resizes the passed vector to the given number
 * of elements. The allocated memory is not reduced by setting
 * a smaller size. Use vec_shrinkToFit() for this purpose.
 *
 * @param[in,out] v - a vector instance
 * @param[in] size - requested number of elements
 * @return 1 on success, 0 on error
 */
int vec_resize(tVector * const v, const size_t size) {
	if (v == NULL) return 0;
	if (size > v->capacity) {
		if (vec_reserve(v, size) != 1) return 0;
	}
	v->size = size;
	return 1;
}


/**
 * The function increases the capacity of the passed vector to
 * the given number of elements.
 *
 * @param[in,out] v - a vector instance
 * @param[in] size - requested number of elements
 * @return 1 on success, 0 on error
 */
int vec_reserve(tVector * const v, const size_t size) {
	if (v == NULL) return 0;
	if (size > v->capacity) {
		const size_t bufferSize = (size * v->elementSize);
		uint8_t * buffer = (uint8_t *)malloc(bufferSize);
		if (buffer == NULL) return 0;
		if (v->buffer != NULL) {
			memcpy(buffer, v->buffer, (v->size * v->elementSize));
			free(v->buffer);
		}
		v->capacity = size;
		v->buffer = buffer;
	}
	return 1;
}


/**
 * The function shrinks the passed vector memory to fit exactly the
 * current number of elements stored in it.
 *
 * @param[in,out] v - a vector instance
 * @return 1 on success, 0 on error
 */
int vec_shrinkToFit(tVector * const v) {
	if (v == NULL) return 0;
	if (v->capacity > v->size) {
		const size_t bufferSize = (v->size * v->elementSize);
		uint8_t * buffer = (uint8_t *)malloc(bufferSize);
		if (buffer == NULL) return 0;
		memcpy(buffer, v->buffer, (v->size * v->elementSize));
		v->capacity = v->size;
		free(v->buffer);
		v->buffer = buffer;
	}
	return 1;
}


/**
 * The function clears the content of the passed vector.
 *
 * @param[in,out] v - a vector instance
 */
void vec_clear(tVector * const v) {
	if (v == NULL) return;
	v->capacity = 0;
	v->size = 0;
	if (v->buffer != NULL) {
		free(v->buffer);
		v->buffer = NULL;
	}
}


/**
 * The function sorts the the passed vector according
 * to the comparer function.
 *
 * @param[in,out] v - a vector instance
 * @param[in] comparer - callback function which compares two elements
 * @param[in] options - parameter passed to the comparer function
 * @return 1 on success, else 0
 * @remarks Allocates a temporary buffer of the size sizeof(void *) * N * 2
 */
int vec_mergeSort(tVector * const v, VectorCompareFunction comparer, const int options) {
	size_t left, right, start, sortLen, length, dir;
	size_t index, posL, posR, posM;
	uint8_t ** hArray, ** perm;
	if (v == NULL || comparer == NULL) return 0;
	if (v->size < 2) return 1;
	hArray = (uint8_t **)malloc(sizeof(uint8_t *) * v->size * 2);
	if (hArray == NULL) return 0;
	for (index = 0; index < v->size; index++) {
		hArray[index] = v->buffer + (index * v->elementSize);
	}
	/* sort here >> */
	left    = 1;
	right   = 1;
	start   = 0;
	sortLen = 1;
	length  = v->size;
	dir     = 1;
	while (left <= length) {
		sortLen <<= 1;
		start = 0;
		while (start < length) {
			if ((length - start) < sortLen) {
				if ((length - start) > (sortLen >> 1)) {
					left = sortLen >> 1;
					right = length - start - left;
				} else {
					left = length - start;
					right = 0;
				}
			}
			posL = start;
			posR = start + left;
			posM = start;
			while (posL < (start + left) && posR < (start + left + right)) {
				if ((*comparer)((const void *)(hArray[(v->size * (dir ^ 1)) + posL]),
								(const void *)(hArray[(v->size * (dir ^ 1)) + posR]),
								options) <= 0) {
					hArray[(v->size * dir) + posM] = hArray[(v->size * (dir ^ 1)) + posL];
					posL++;
				} else {
					hArray[(v->size * dir) + posM] = hArray[(v->size * (dir ^ 1)) + posR];
					posR++;
				}
				posM++;
			}
			while (posL < (start + left)) {
				hArray[(v->size * dir) + posM] = hArray[(v->size * (dir ^ 1)) + posL];
				posL++;
				posM++;
			}
			while (posR < (start + left + right)) {
				hArray[(v->size * dir) + posM] = hArray[(v->size * (dir ^ 1)) + posR];
				posR++;
				posM++;
			}
			start = start + sortLen;
		}
		dir  ^= 1;
		left  = sortLen;
		right = sortLen;
	}
	/* sort here << */
	dir ^= 1;
	perm = hArray + (v->size * dir);
	length = v->size;
	/* apply permutation according to pointers in temporary buffer; requires at most N swaps */
	for (index = 0; index < length; index++) {
		if (perm[index] == NULL) continue; /* already visited -> ignore */
		const uint8_t * i = v->buffer + (index * v->elementSize);
		start = index;
		while (perm[start] != i) {
			const size_t next = ((size_t)(perm[start] - v->buffer)) / v->elementSize;
			libpcf_swap_internal(
				v->buffer + (start * v->elementSize),
				v->buffer + (next * v->elementSize),
				v->elementSize
			);
			perm[start] = NULL; /* mark visited */
			start = next;
		}
		perm[start] = NULL; /* mark visited */
	}
	free(hArray);
	return 1;
}


/**
 * The function deletes the content and instance of the passed vector.
 *
 * @param[in,out] v - a vector instance
 */
void vec_delete(tVector * v) {
	if (v == NULL) return;
	if (v->buffer != NULL) free(v->buffer);
	free(v);
}
