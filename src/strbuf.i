/**
 * @file strbuf.i
 * @author Daniel Starke
 * @see ustrbuf.h
 * @date 2017-05-25
 * @version 2025-07-18
 * @internal This file is never used or compiled directly but only included.
 * @remarks Define STRBUF_UNICODE for the Unicode before including this file.
 * Defaults to ASCII.
 */
#ifdef STRBUF_UNICODE
#include <wchar.h>
#endif
#include <windows.h>


#ifdef STRBUF_UNICODE
#define CHAR_T wchar_t
#define _T(x) L##x
#ifndef TCHAR_STLEN
#define TCHAR_STLEN wcslen
#endif /* TCHAR_STLEN */
#ifndef TCHAR_STNCPY
#define TCHAR_STNCPY wcsncpy
#endif /* TCHAR_STNCPY */
#ifndef TCHAR_STPRINTF
#define TCHAR_STPRINTF fwprintf
#endif /* TCHAR_STPRINTF */
#ifndef TCHAR_STVSNPRINTF
#define TCHAR_STVSNPRINTF vsnwprintf
#endif /* TCHAR_STVSNPRINTF */
#ifndef TCHAR_FUNC
#define TCHAR_FUNC(x) usb_##x
#endif /* TCHAR_FUNC */
#ifndef TCHAR_BUF
#define TCHAR_BUF tUStrBuf
#endif /* TCHAR_BUF */
#ifndef TCHAR_BUF_ELEMENT
#define TCHAR_BUF_ELEMENT tUStrBufElement
#endif /* TCHAR_BUF_ELEMENT */
#else /* ! STRBUF_UNICODE */
#define CHAR_T char
#define _T(x) x
#ifndef TCHAR_STLEN
#define TCHAR_STLEN strlen
#endif /* TCHAR_STLEN */
#ifndef TCHAR_STNCPY
#define TCHAR_STNCPY strncpy
#endif /* TCHAR_STNCPY */
#ifndef TCHAR_STPRINTF
#define TCHAR_STPRINTF fprintf
#endif /* TCHAR_STPRINTF */
#ifndef TCHAR_STVSNPRINTF
#define TCHAR_STVSNPRINTF vsnprintf
#endif /* TCHAR_STVSNPRINTF */
#ifndef TCHAR_FUNC
#define TCHAR_FUNC(x) sb_##x
#endif /* TCHAR_FUNC */
#ifndef TCHAR_BUF
#define TCHAR_BUF tStrBuf
#endif /* TCHAR_BUF */
#ifndef TCHAR_BUF_ELEMENT
#define TCHAR_BUF_ELEMENT tStrBufElement
#endif /* TCHAR_BUF_ELEMENT */
#endif /* STRBUF_UNICODE */


/**
 * The function creates a new instance of a string buffer.
 *
 * @param[in] bufSize - size of the string buffer chunks in characters
 * @return returns the created string buffer instance or NULL
 */
TCHAR_BUF * TCHAR_FUNC(create)(const size_t bufSize) {
	TCHAR_BUF * obj;
	if (bufSize < 1) return NULL;
	obj = (TCHAR_BUF *)malloc(sizeof(TCHAR_BUF));
	if (obj == NULL) return NULL;
	obj->bufSize = bufSize;
	obj->bufOffset = 0;
	obj->bufList = (TCHAR_BUF_ELEMENT *)malloc(sizeof(TCHAR_BUF_ELEMENT));
	if (obj->bufList == NULL) {
		free(obj);
		return NULL;
	}
	obj->bufList->next = NULL;
	obj->bufList->buffer = (CHAR_T *)malloc(sizeof(CHAR_T) * bufSize);
	if (obj->bufList->buffer == NULL) {
		free(obj->bufList);
		free(obj);
		return NULL;
	}
	obj->lastBuf = obj->bufList;
	obj->len = 0;
	return obj;
}


/**
 * The function adds a specific null-terminated string to the
 * passed string buffer.
 *
 * @param[in,out] sb - a string buffer instance
 * @param[in] string - the string to add
 * @return 1 on success, 0 on error
 */
int TCHAR_FUNC(add)(TCHAR_BUF * sb, const CHAR_T * string) {
	if (sb == NULL || string == NULL) return 0;
	for (; *string != 0; string++) {
		if (TCHAR_FUNC(addC)(sb, *string) == 0) return 0;
	}
	return 1;
}


/**
 * The function adds a specific character to the passed
 * string buffer.
 *
 * @param[in,out] sb - a string buffer instance
 * @param[in] c - the character to add
 * @return 1 on success, 0 on error
 */
int TCHAR_FUNC(addC)(TCHAR_BUF * sb, const CHAR_T c) {
	TCHAR_BUF_ELEMENT * buf, * newBufE;
	CHAR_T * newBuf;
	if (sb == NULL) return 0;
	buf = sb->lastBuf;
	if (sb->bufOffset >= sb->bufSize) {
		newBuf = (CHAR_T *)malloc(sizeof(CHAR_T) * sb->bufSize);
		if (newBuf == NULL) return 0;
		newBufE = (TCHAR_BUF_ELEMENT *)malloc(sizeof(TCHAR_BUF_ELEMENT));
		if (newBufE == NULL) {
			free(newBuf);
			return 0;
		}
		newBufE->next = NULL;
		newBufE->buffer = newBuf;
		sb->bufOffset = 0;
		buf->next = newBufE;
		buf = buf->next;
		sb->lastBuf = buf;
	}
	buf->buffer[sb->bufOffset] = c;
	sb->bufOffset++;
	sb->len++;
	return 1;
}


/**
 * The function adds a formatted string to the passed
 * string buffer.
 *
 * @param[in,out] sb - a string buffer instance
 * @param[in] fmt - the format string
 * @return 1 on success, 0 on error
 */
int TCHAR_FUNC(addFmt)(TCHAR_BUF * sb, const CHAR_T * fmt, ...) {
	va_list ap;
	int result;
	va_start(ap, fmt);
	result = TCHAR_FUNC(addFmtVar)(sb, fmt, ap);
	va_end(ap);
	return result;
}


/**
 * The function adds a formatted string to the passed
 * string buffer.
 *
 * @param[in,out] sb - a string buffer instance
 * @param[in] fmt - the format string
 * @param[in] ap - variable argument list
 * @return 1 on success, 0 on error
 */
int TCHAR_FUNC(addFmtVar)(TCHAR_BUF * sb, const CHAR_T * fmt, va_list ap) {
	CHAR_T * mem;
	int strLen, resLen;
	if (sb == NULL || fmt == NULL) return 0;
	{
		va_list ap2;
		va_copy(ap2, ap);
		strLen = TCHAR_STVSNPRINTF(NULL, 0, fmt, ap2);;
		va_end(ap2);
	}
	if (strLen <= 0) return 0;
	strLen++;
	mem = (CHAR_T *)malloc(sizeof(CHAR_T) * (size_t)strLen);
	if (mem == NULL) return 0;
	resLen = TCHAR_STVSNPRINTF(mem, (size_t)strLen, fmt, ap);
	if (resLen != (strLen - 1) || TCHAR_FUNC(add)(sb, mem) == 0) {
		free(mem);
		return 0;
	}
	free(mem);
	return 1;
}


/**
 * The function clears the string buffer.
 *
 * @param[in,out] sb - a string buffer instance
 * @return 1 on success, 0 on error
 */
int TCHAR_FUNC(clear)(TCHAR_BUF * sb) {
	TCHAR_BUF_ELEMENT * buf, * nextBuf;
	if (sb == NULL) return 0;
	sb->bufOffset = 0;
	sb->lastBuf = sb->bufList;
	buf = sb->bufList;
	if (buf->next) {
		buf = buf->next;
		do {
			nextBuf = buf->next;
			free(buf->buffer);
			free(buf);
			buf = nextBuf;
		} while (buf != NULL);
	}
	sb->bufList->next = NULL;
	sb->len = 0;
	return 1;
}


/**
 * The function securely wipes the string buffer.
 *
 * @param[in,out] sb - a string buffer instance
 * @return 1 on success, 0 on error
 */
int TCHAR_FUNC(wipe)(TCHAR_BUF * sb) {
	TCHAR_BUF_ELEMENT * buf, * nextBuf;
	if (sb == NULL) return 0;
	sb->bufOffset = 0;
	sb->lastBuf = sb->bufList;
	buf = sb->bufList;
	if (buf->next) {
		buf = buf->next;
		do {
			nextBuf = buf->next;
			SecureZeroMemory(buf->buffer, sizeof(CHAR_T) * sb->bufSize);
			free(buf->buffer);
			free(buf);
			buf = nextBuf;
		} while (buf != NULL);
	}
	sb->bufList->next = NULL;
	sb->len = 0;
	return 1;
}


/**
 * The function returns a new allocated string with the
 * whole content of the string buffer.
 *
 * @param[in] sb - a string buffer instance
 * @return the string pointer on success, NULL on error
 */
CHAR_T * TCHAR_FUNC(get)(TCHAR_BUF * sb) {
	CHAR_T * result;
	if (sb == NULL) return NULL;
	result = (CHAR_T *)malloc(sizeof(CHAR_T) * (sb->len + 1));
	if (result == NULL) return NULL;
	if (TCHAR_FUNC(copyToStr)(sb, result) == 0) {
		free(result);
		return NULL;
	}
	return result;
}


/**
 * The function copies the whole content of the string buffer
 * to the passed string. The string needs to be large enough
 * to hold the content and the additional null-pointer at the end.
 *
 * @param[in] sb - a string buffer instance
 * @param[out] string - the target string
 * @return number of characters copied on success (without null-terminator)
 * @return 0 on error
 */
size_t TCHAR_FUNC(copyToStr)(TCHAR_BUF * sb, CHAR_T * string) {
	TCHAR_BUF_ELEMENT * buf;
	if (sb == NULL || string == NULL) return 0;
	buf = sb->bufList;
	while (buf->next != NULL) {
		TCHAR_STNCPY(string, buf->buffer, sb->bufSize);
		string += sb->bufSize;
		buf = buf->next;
	}
	if (sb->bufOffset > 0) {
		TCHAR_STNCPY(string, buf->buffer, sb->bufOffset);
	}
	string[sb->bufOffset] = 0;
	return sb->len;
}


/**
 * The function writes the whole content of the string buffer
 * to the passed file descriptor without the null-termination
 * character.
 *
 * @param[in] sb - a string buffer instance
 * @param[in,out] fd - the target file descriptor
 * @return number of characters copied or 0 on error
 * @remarks This function uses fprintf/fwprintf internally to
 * allow local transformations.
 */
size_t TCHAR_FUNC(write)(TCHAR_BUF * sb, FILE * fd) {
	TCHAR_BUF_ELEMENT * buf;
	size_t result;
	int tmp;
	if (sb == NULL || fd == NULL) return 0;
	buf = sb->bufList;
	result = 0;
	while (buf->next != NULL) {
		tmp = TCHAR_STPRINTF(fd, _T("%.*s"), (int)(sb->bufSize), buf->buffer);
		if (tmp < 0) return result;
		result += (size_t)tmp;
		buf = buf->next;
	}
	tmp = TCHAR_STPRINTF(fd, _T("%.*s"), (int)(sb->bufOffset), buf->buffer);
	if (tmp < 0) return result;
	return result + (size_t)tmp;
}


/**
 * The function returns the length of the string buffer in
 * characters without the null-termination character.
 *
 * @param[in] sb - a string buffer instance
 * @return number of characters in the string buffer
 */
size_t TCHAR_FUNC(len)(TCHAR_BUF * sb) {
	if (sb == NULL) return 0;
	return sb->len;
}


/**
 * The function deletes the specific string buffer instance.
 *
 * @param[in] sb - a string buffer instance
 * @return 1 on success, 0 on error
 */
int TCHAR_FUNC(delete)(TCHAR_BUF * sb) {
	if (sb == NULL) return 0;
	TCHAR_FUNC(clear)(sb);
	free(sb->bufList->buffer);
	free(sb->bufList);
	free(sb);
	return 1;
}
