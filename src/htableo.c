/**
 * @file htableo.c
 * @author Daniel Starke
 * @see htableo.h
 * @date 2010-01-26
 * @version 2025-07-25
 */
#include <stdlib.h>
#include <string.h>
#include "htableo.h"


/**
 * The function creates a new instance of a hash table.
 *
 * @param[in] dataSize - size of the data for each element in bytes
 * @param[in] tableSize - maximum number of elements in the base table
 * @param[in] clone - key clone function
 * @param[in] del - key delete function
 * @param[in] cmp - key compare function
 * @param[in] hash - key hash function
 * @return returns the created hash table instance or NULL
 */
tHTableO * hto_create(const size_t dataSize, const size_t tableSize, HashFunctionCloneO clone, HashFunctionDelO del, HashFunctionCmpO cmp, HashFunctionHashO hash) {
	tHTableO * obj;
	if (dataSize < 1 || tableSize < 1 || clone == NULL || del == NULL || cmp == NULL || hash == NULL) return NULL;
	obj = (tHTableO *)malloc(sizeof(tHTableO));
	if (obj == NULL) return NULL;
	obj->elementSize = sizeof(tHTableOElement) + dataSize;
	obj->tableSize   = tableSize;
	obj->size        = 0;
	obj->table       = (tHTableOElement **)calloc(tableSize, sizeof(tHTableOElement *));
	obj->clone       = clone;
	obj->del         = del;
	obj->cmp         = cmp;
	obj->hash        = hash;
	if (obj->table == NULL) {
		free(obj);
		return NULL;
	}
	return obj;
}


/**
 * The function adds a new element to the passed hash table.
 *
 * @param[in,out] ht - a hash table instance
 * @param[in] key - pointer to the key value of the new element
 * @return pointer to the newly added element data, NULL on error
 */
void * hto_addKey(tHTableO * ht, const void * key) {
	tHTableOElement * element, * nextElement;
	void * newKey;
	size_t tablePosition;
	if (ht == NULL || key == NULL) return NULL;
	element = (tHTableOElement *)hto_getKey(ht, key);
	if (element != NULL) return element;
	newKey = ht->clone(key);
	if (newKey == NULL) return NULL;
	element = (tHTableOElement *)calloc(1, ht->elementSize);
	if (element == NULL) {
		ht->del(newKey);
		return NULL;
	}
	element->after = NULL;
	element->key = newKey;
	tablePosition = ht->hash(key, ht->tableSize);
	nextElement = ht->table[tablePosition];
	if (nextElement == NULL) {
		ht->table[tablePosition] = element;
	} else {
		while (nextElement->after != NULL) {
			nextElement = nextElement->after;
		}
		nextElement->after = element;
	}
	ht->size++;
	return (void *)(((uint8_t *)element) + sizeof(tHTableOElement));
}


/**
 * The function returns a pointer to the data of the specific key
 * in the passed hash table.
 *
 * @param[in] ht - a hash table instance
 * @param[in] key - pointer to the key value of the desired element
 * @return pointer to the element data, NULL on error or nonexistent key
 */
void * hto_getKey(tHTableO * ht, const void * key) {
	tHTableOElement * nextElement;
	size_t tablePosition;
	if (ht == NULL || key == NULL) return NULL;
	tablePosition = ht->hash(key, ht->tableSize);
	nextElement = ht->table[tablePosition];
	if (nextElement != NULL) {
		while (nextElement->after != NULL && ht->cmp(nextElement->key, key) != 0) {
			nextElement = nextElement->after;
		}
		if (ht->cmp(nextElement->key, key) == 0) {
			return (void *)(((uint8_t *)nextElement) + sizeof(tHTableOElement));
		}
	}
	return NULL;
}


#if defined(__clang_major__) && (__clang_major__ >= 17)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuse-after-free"
#elif defined(__GNUC__) && (__GNUC__ >= 15)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
#endif /* GCC >= 15 */
/**
 * The function deleted the element with the specific key
 * in the passed hash table.
 *
 * @param[in,out] ht - a hash table instance
 * @param[in] key - pointer to the key value of the desired element
 * @return pointer to the old element data, NULL on error or nonexistent key
 * @remarks The return value is only to check if the option was successful.
 * @remarks The returned pointer points to an invalid memory address on success.
 */
void * hto_delKey(tHTableO * ht, const void * key) {
	tHTableOElement * lastElement, * nextElement;
	size_t tablePosition;
	if (ht == NULL || key == NULL) return NULL;
	tablePosition = ht->hash(key, ht->tableSize);
	nextElement = ht->table[tablePosition];
	lastElement = NULL;
	if (nextElement != NULL) {
		if (ht->cmp(nextElement->key, key) == 0) {
			ht->table[tablePosition] = nextElement->after;
			ht->del(nextElement->key);
			free(nextElement);
			ht->size--;
			return (void *)(((uint8_t *)nextElement) + sizeof(tHTableOElement));
		} else {
			while (nextElement->after != NULL && ht->cmp(nextElement->key, key) != 0) {
				lastElement = nextElement;
				nextElement = nextElement->after;
			}
			if (ht->cmp(nextElement->key, key) == 0) {
				if (lastElement != NULL) {
					lastElement->after = nextElement->after;
				}
				ht->del(nextElement->key);
				free(nextElement);
				ht->size--;
				return (void *)(((uint8_t *)nextElement) + sizeof(tHTableOElement));
			}
		}
	}
	return NULL;
}
#if defined(__clang_major__) && (__clang_major__ >= 17)
#pragma clang diagnostic pop
#elif defined(__GNUC__) && (__GNUC__ >= 15)
#pragma GCC diagnostic pop
#endif


/**
 * The function returns the number of element in the passed
 * hash table.
 *
 * @param[in] ht - a hash table instance
 * @return number of elements in table, 0 on error
 */
size_t hto_size(tHTableO * ht) {
	if (ht == NULL) return 0;
	return ht->size;
}


/**
 * The function removes all elements from the passed
 * hash table.
 *
 * @param[in,out] ht - a hash table instance
 * @return 1 on success, 0 on error
 */
int hto_clear(tHTableO * ht) {
	tHTableOElement ** entry;
	tHTableOElement * element, * nextElement;
	size_t i;
	if (ht == NULL) return 0;
	entry = ht->table;
	for (i = 0; i < ht->tableSize; i++) {
		nextElement = *entry;
		if (nextElement != NULL) {
			do {
				element = nextElement;
				nextElement = nextElement->after;
				ht->del(element->key);
				free(element);
			} while (nextElement != NULL);
		}
		*entry = NULL;
		entry++;
	}
	ht->size = 0;
	return 1;
}


/**
 * The function traverses through all elements from the passed
 * hash table and calls the specific callback function.
 *
 * @param[in] ht - a hash table instance
 * @param[in] v - callback function which is called for every element
 * @param[in] param - user defined parameter
 * @return 1 on success, 0 on error
 * @see HashVisitorO
 */
int hto_traverse(tHTableO * ht, HashVisitorO v, void * param) {
	tHTableOElement ** entry;
	tHTableOElement * element;
	size_t i;
	if (ht == NULL || v == NULL) return 0;
	entry = ht->table;
	for (i = 0; i < ht->tableSize; i++) {
		element = *entry;
		if (element != NULL) {
			do {
				if ((* v)(element->key, (void *)(((uint8_t *)element) + sizeof(tHTableOElement)), param) == 0) return 0;
				element = element->after;
			} while (element != NULL);
		}
		entry++;
	}
	return 1;
}


/**
 * The function deleted every element of the passed hash
 * table and its instance.
 *
 * @param[in] ht - a hash table instance
 */
void hto_delete(tHTableO * ht) {
	if (ht == NULL) return;
	hto_clear(ht);
	free(ht->table);
	free(ht);
}


/**
 * Returns the key pointer from the key value pointer.
 *
 * @param[in] key - a key value pointer
 * @return key pointer
 */
const void * hto_toKeyPtr(const void * key) {
	if (key == NULL) return NULL;
	return ((const tHTableOElement *)(((uint8_t *)key) - sizeof(tHTableOElement)))->key;
}
