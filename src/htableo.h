/**
 * @file htableo.h
 * @author Daniel Starke
 * @see htableo.c
 * @date 2010-01-26
 * @version 2025-07-17
 */
#ifndef __LIBPCF_HTABLEO_H__
#define __LIBPCF_HTABLEO_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Defines the callback function for key cloning.
 * It is recommended to make the callback function inline
 * for speed increase.
 *
 * @param[in] key - pointer to the key value
 * @return cloned key or `NULL`
 */
typedef void * (* HashFunctionCloneO)(const void *);


/**
 * Defines the callback function for key deletion.
 * It is recommended to make the callback function inline
 * for speed increase.
 *
 * @param[in] key - pointer to the key value
 */
typedef void (* HashFunctionDelO)(const void *);


/**
 * Defines the callback function for key comparison.
 * It is recommended to make the callback function inline
 * for speed increase.
 *
 * @param[in] lhs - pointer to the left-hand sided key value
 * @param[in] rhs - pointer to the right-hand sided key value
 * @return 0 if equal, not 0 in every other case
 */
typedef int (* HashFunctionCmpO)(const void *, const void *);


/**
 * Defines the callback function for key hash calculation.
 * It is recommended to make the callback function inline
 * for speed increase.
 *
 * @param[in] key - pointer to the key value
 * @param[in] limit - size of hash table
 * @return hash value x with 0 <= x < limit
 */
typedef size_t (* HashFunctionHashO)(const void *, const size_t);


/**
 * Defines a callback function which is called when
 * traversing the hash table.
 *
 * @param[in] key - pointer to the key value
 * @param[in] data - pointer to the element data
 * @param[in] param - user defined pointer
 * @return 0 to abort
 * @return 1 to continue
 */
typedef int (* HashVisitorO)(const void *, void *, void *);


/**
 * The defined structure is for internal usage only.
 * It defines an element for the hash table.
 */
typedef struct tHTableOElement {
	struct tHTableOElement * after; /**< pointer to next element */
	void * key; /**< unique key */
} tHTableOElement;


/**
 * The defined structure is for internal usage only.
 * It defines context information for the hash table.
 */
typedef struct {
	size_t elementSize; /**< size of an element in bytes */
	size_t tableSize; /**< max. number of table indices */
	size_t size; /**< number of elements in hash table */
	tHTableOElement ** table; /**< hash table array */
	HashFunctionCloneO clone; /**< key clone function */
	HashFunctionDelO del; /**< key delete function */
	HashFunctionCmpO cmp; /**< key compare function */
	HashFunctionHashO hash; /**< key hash function */
} tHTableO;


tHTableO * hto_create(const size_t dataSize, const size_t tableSize, HashFunctionCloneO clone, HashFunctionDelO del, HashFunctionCmpO cmp, HashFunctionHashO hash);
void * hto_addKey(tHTableO * ht, const void * key);
void * hto_getKey(tHTableO * ht, const void * key);
void * hto_delKey(tHTableO * ht, const void * key);
size_t hto_size(tHTableO * ht);
int    hto_clear(tHTableO * ht);
int    hto_traverse(tHTableO * ht, HashVisitorO v, void * param);
void   hto_delete(tHTableO * ht);


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_HTABLEO_H__ */
