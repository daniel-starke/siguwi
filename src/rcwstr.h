/**
 * @file rcwstr.h
 * @author Daniel Starke
 * @see rcwstr.c
 * @date 2025-07-23
 * @version 2025-07-23
 */
#ifndef __RCWSTR_H__
#define __RCWSTR_H__

#include <wchar.h>
#include <windows.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Reference counted wide-character string.
 */
typedef struct {
	LONG refCount;
	wchar_t ptr[];
} tRcWStr;


tRcWStr * rws_create(const wchar_t * str);
tRcWStr * rws_aquire(tRcWStr * s);
void rws_release(tRcWStr ** s);


#ifdef __cplusplus
}
#endif


#endif /* __RCWSTR_H__ */
