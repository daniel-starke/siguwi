/**
 * @file siguwi-translate.c
 * @author Daniel Starke
 * @date 2025-08-24
 * @version 2025-08-25
 */
#include "siguwi.h"


/**
 * Flushes the translation buffers buffers.
 *
 * @param[in,out] ctx - standard I/O translation context
 */
static void transFlushBuffers(tTransIoCtx * ctx) {
	if (ctx == NULL || ctx->inLen == 0) {
		return;
	}
	size_t consume = ctx->inLen;
	DWORD bytesWritten;
	/* find longest convertable prefix */
	while (consume > 0) {
		const int len = MultiByteToWideChar(ctx->acp, MB_ERR_INVALID_CHARS, ctx->inBuf, (int)consume, ctx->wBuf, ARRAY_SIZE(ctx->wBuf));
		if (len > 0) {
			int utf8Len = WideCharToMultiByte(CP_UTF8, 0, ctx->wBuf, len, ctx->outBuf, ARRAY_SIZE(ctx->outBuf), NULL, NULL);
			if (utf8Len > 0) {
				WriteFile(ctx->hOut, ctx->outBuf, (DWORD)utf8Len, &bytesWritten, NULL);
			}
			break;
		}
		/* re-try with one byte less */
		--consume;
	}
	/* relocate remaining input bytes */
	ctx->inLen = (size_t)(ctx->inLen - consume);
	if (ctx->inLen > 0) {
		memmove(ctx->inBuf, ctx->inBuf + consume, ctx->inLen);
	}
}


/**
 * Translates the input data stream from ACP to UTF-8 in
 * the standard output stream.
 *
 * @return program exit code
 */
int translateIo() {
	int res = 1;
	HANDLE hEvent = NULL;
	tTransIoCtx ctx[1];
	ZeroMemory(ctx, sizeof(ctx));
	/* get current ACP */
	ctx->acp = GetACP();
	/* get standard input/output handles */
	ctx->hIn = GetStdHandle(STD_INPUT_HANDLE);
	ctx->hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (ctx->hIn == INVALID_HANDLE_VALUE || ctx->hOut == INVALID_HANDLE_VALUE) {
		showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (translateIo)", errStr[ERR_GET_STD_HANDLE]);
		goto onError;
	}
	ctx->isSeekable = (GetFileType(ctx->hIn) == FILE_TYPE_DISK);
	/* prepare overlapped read */
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if ( ! hEvent ) {
		showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (translateIo)", errStr[ERR_CREATE_EVENT], GetLastError());
		goto onError;
	}
	ctx->lastFlush = GetTickCount();
	/* translation loop */
	for (;;) {
		ZeroMemory(&(ctx->ovRead), sizeof(ctx->ovRead));
		ResetEvent(hEvent);
		ctx->ovRead.hEvent = hEvent;
		if ( ctx->isSeekable ) {
			ctx->ovRead.Offset = (DWORD)(ctx->received & 0xFFFFFFFF);
			ctx->ovRead.OffsetHigh = (DWORD)(ctx->received >> 32);
		}
		DWORD got = 0;
		const BOOL started = ReadFile(ctx->hIn, ctx->inBuf + ctx->inLen, (DWORD)(ARRAY_SIZE(ctx->inBuf) - ctx->inLen), &got, &(ctx->ovRead));
		if ( ! started ) {
			if (GetLastError() == ERROR_IO_PENDING) {
				if (WaitForSingleObject(ctx->ovRead.hEvent, 100) == WAIT_OBJECT_0) {
					if ( ! GetOverlappedResult(ctx->hIn, &(ctx->ovRead), &got, FALSE) ) {
						transFlushBuffers(ctx);
						break;
					}
				} else {
					got = 0;
				}
			} else {
				/* EOF or error */
				transFlushBuffers(ctx);
				break;
			}
		}
		const DWORD now = GetTickCount();
		if (got > 0) {
			if ( ctx->isSeekable ) {
				ctx->received += (DWORD64)got;
			}
			ctx->inLen += (size_t)got;
			if (ctx->inLen >= ARRAY_SIZE(ctx->inBuf)) {
				/* flush immediately on full input buffer */
				transFlushBuffers(ctx);
				ctx->lastFlush = now;
			} else {
				/* flush immediately on newline */
				for (size_t i = 0; i < ctx->inLen; ++i) {
					if (ctx->inBuf[i] == '\n') {
						transFlushBuffers(ctx);
						ctx->lastFlush = now;
						break;
					}
				}
			}
		}
		/* force flush once every second */
		if (ctx->inLen > 0 && (now - ctx->lastFlush) >= 1000) {
			transFlushBuffers(ctx);
			ctx->lastFlush = now;
		}
	}
	res = 0;
onError:
	closeHandlePtr(&hEvent, NULL);
	return res;
}
