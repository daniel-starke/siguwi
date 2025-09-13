/**
 * @file ustrbuf.h
 * @author Daniel Starke
 * @see ustrbuf.c
 * @date 2010-03-25
 * @version 2025-07-18
 */
#ifndef __LIBPCF_USTRBUF_H__
#define __LIBPCF_USTRBUF_H__

#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>
#include "target.h"


#ifdef __cplusplus
extern "C" {
#endif


/**
 * The defined structure is for internal usage only.
 * It defines an buffer element for the string buffer.
 */
typedef struct tUStrBufElement {
	struct tUStrBufElement * next; /**< pointer to next buffer element */
	wchar_t * buffer; /**< pointer to buffer */
} tUStrBufElement;


/**
 * The defined structure is for internal usage only.
 * It defines the context information for the string buffer.
 */
typedef struct tUStrBuf {
	size_t bufSize; /**< default buffer size in bytes */
	size_t bufOffset; /**< current position inside the buffer */
	tUStrBufElement * bufList; /**< pointer to the first buffer element */
	tUStrBufElement * lastBuf; /**< pointer to the last buffer element */
	size_t len; /**< number of characters inside the string buffer */
} tUStrBuf;


tUStrBuf * usb_create(const size_t bufSize);
int usb_add(tUStrBuf * sb, const wchar_t * string);
int usb_addC(tUStrBuf * sb, const wchar_t c);
int usb_addFmt(tUStrBuf * sb, const wchar_t * fmt, ...);
int usb_addFmtVar(tUStrBuf * sb, const wchar_t * fmt, va_list ap);
int usb_wipe(tUStrBuf * sb);
int usb_clear(tUStrBuf * sb);
wchar_t * usb_get(tUStrBuf * sb);
size_t usb_copyToStr(tUStrBuf * sb, wchar_t * string);
size_t usb_write(tUStrBuf * sb, FILE * fd);
size_t usb_len(tUStrBuf * sb);
int usb_delete(tUStrBuf * sb);


#ifdef __cplusplus
}
#endif


#endif /* __LIBPCF_USTRBUF_H__ */
