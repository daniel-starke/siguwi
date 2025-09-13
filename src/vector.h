/**
 * @file vector.h
 * @author Daniel Starke
 * @see vector.c
 * @date 2018-04-06
 * @version 2025-07-27
 */
#ifndef __LIBPCF_VECTOR_H__
#define __LIBPCF_VECTOR_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @internal
 */
typedef struct {
	size_t capacity; /**< number of elements that can be stored in total before a resize */
	size_t elementSize; /**< size of an element in bytes */
	size_t size; /**< number of elements in vector */
	uint8_t * buffer; /**< internal buffer */
} tVector;


/**
 * This callback function is defined to pass an appropriate compare
 * function which is needed to compare two elements. It is recommended
 * to make the callback function inline for speed increase.
 * <br><br>Expample:<pre>
 * inline int intComparer(const void * var1, const void * var2, const int options) {
 * 	const int * x, * y;
 * 	x = (const int *)var1;
 * 	y = (const int *)var2;
 * 	if (*x > *y) return 1 * options;
 * 	if (*x == *y) return 0;
 * 	return -1 * options;
 * }
 * </pre>
 *
 * @param[in] var1 - first element
 * @param[in] var2 - second element
 * @param[in] options - user defined options
 * @return 1 for var1 > var2
 * @return 0 for var1 == var2
 * @return -1 for var1 < var2 if ascending ( *(-1) else)
 */
typedef int (* const VectorCompareFunction)(const void *, const void *, const int);


/**
 * Defines a callback function which is called when
 * traversing the vector.
 *
 * @param[in] index - vector index
 * @param[in] data - pointer to the element data
 * @param[in] param - user defined pointer
 * @return 0 to abort
 * @return 1 to continue
 */
typedef int (* VectorVisitor)(const size_t, void *, void *);


tVector * vec_create(const size_t dataSize);
tVector * vec_clone(const tVector * const v);
void * vec_pushBack(tVector * const v);
int    vec_popBack(tVector * const v);
void * vec_front(tVector * const v);
void * vec_back(tVector * const v);
void * vec_append(tVector * const v, const tVector * const src);
void * vec_appendArray(tVector * const v, const void * const src, const size_t size);
int    vec_erase(tVector * const v, const size_t i, const size_t size);
void * vec_at(tVector * const v, const size_t i);
int    vec_traverse(const tVector * const v, VectorVisitor cb, void * param);
int    vec_empty(const tVector * const v);
size_t vec_size(const tVector * const v);
size_t vec_capacity(const tVector * const v);
int    vec_resize(tVector * const v, const size_t size);
int    vec_reserve(tVector * const v, const size_t size);
int    vec_shrinkToFit(tVector * const v);
void   vec_clear(tVector * const v);
int    vec_mergeSort(tVector * const v, VectorCompareFunction comparer, const int options);
void   vec_delete(tVector * v);


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_VECTOR_H__ */
