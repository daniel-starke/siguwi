/**
 * @file utf8.h
 * @author Daniel Starke
 * @see utf8.c
 * @date 2025-07-17
 * @version 2025-07-17
 */
#ifndef __UTF8_H__
#define __UTF8_H__

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/** Replacement character returned in case of an parsing error. */
#define UTF8_ERROR 0xFFFD
/** Invalid code point. Signals that more data is needed. */
#define UTF8_MORE 0xFFFFFFFF


/**
 * UTF-8 parser context.
 */
typedef struct {
	uint32_t cp; /**< code point */
	int_fast8_t rem; /**< remaining number of bytes bytes */
} tUtf8Ctx;


uint32_t utf8_parse(tUtf8Ctx * ctx, uint8_t b);


#ifdef __cplusplus
}
#endif


#endif /* __UTF8_H__ */
