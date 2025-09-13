/**
 * @file siguwi-process.c
 * @author Daniel Starke
 * @date 2025-06-25
 * @version 2025-09-13
 */
#include "siguwi.h"


/**
 * Deletes the given pin data blob memory.
 * 
 * @param[in] key - pointer to the `tRcIniConfigBase` value (unused)
 * @param[in] data - pin data blob
 * @param[in] param - user parameter (unused)
 * @return 0 to abort
 * @return 1 to continue
 */
int pinBlobDelete(const tRcIniConfigBase * key, DATA_BLOB * data, void * param) {
	PCF_UNUSED(key);
	PCF_UNUSED(param);
	if (data == NULL || data->pbData == NULL) {
		return 1;
	}
	if (data->cbData > 0) {
		SecureZeroMemory(data->pbData, data->cbData);
	}
	LocalFree(data->pbData);
	ZeroMemory(data, sizeof(*data));
	return 1;
}


/**
 * Deletes a process context.
 *
 * @param[in] index - process context vector index (unused)
 * @param[in,out] data - process context
 * @param[in] param - user parameter (unused)
 * @return 0 to abort
 * @return 1 to continue
 */
int procCtxDelete(const size_t index, tProcCtx * data, void * param) {
	PCF_UNUSED(index);
	PCF_UNUSED(param);
	if (data == NULL) {
		return 1;
	}
	rcIniConfigBaseDelete(data->config);
	data->config = NULL;
	rws_release(&(data->signApp));
	wStrDelete(&(data->path));
	if (data->output != NULL) {
		usb_delete(data->output);
		data->output = NULL;
	}
	return 1;
}


/**
 * Sends the signing request to an connected IPC server via named pipe.
 * 
 * @param[in] hPipe - piper handle
 * @param[in] c - INI configuration
 * @param[in] argc - number of files to sign
 * @param[in] argv - list of files to sign
 * @return `true` on success, else `false`
 */
bool ipcSendReqToServer(HANDLE hPipe, const tIniConfig * c, int argc, wchar_t ** argv) {
	if (hPipe == INVALID_HANDLE_VALUE || c == NULL || argc == 0 || argv == 0 || argv[0] == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return false;
	}
	DWORD bytesWritten;
	DWORD bytesToWrite;
	bool res = true;
	bytesToWrite = (DWORD)((wcslen(c->cert->certId) + 1) * sizeof(wchar_t));
	res = res && WriteFile(hPipe, c->cert->certId, bytesToWrite, &bytesWritten, NULL) && bytesWritten >= bytesToWrite;
	bytesToWrite = (DWORD)((wcslen(c->cert->cardName) + 1) * sizeof(wchar_t));
	res = res && WriteFile(hPipe, c->cert->cardName, bytesToWrite, &bytesWritten, NULL) && bytesWritten >= bytesToWrite;
	bytesToWrite = (DWORD)((wcslen(c->cert->cardReader) + 1) * sizeof(wchar_t));
	res = res && WriteFile(hPipe, c->cert->cardReader, bytesToWrite, &bytesWritten, NULL) && bytesWritten >= bytesToWrite;
	bytesToWrite = (DWORD)((wcslen(c->signApp->ptr) + 1) * sizeof(wchar_t));
	res = res && WriteFile(hPipe, c->signApp->ptr, bytesToWrite, &bytesWritten, NULL) && bytesWritten >= bytesToWrite;
	/* transmit file list */
	for (int i = 0; i < argc; ++i) {
		wchar_t * path = argv[i];
		wToFullPath(&path, false);
		bytesToWrite = (DWORD)((wcslen(path) + 1) * sizeof(wchar_t));
		res = res && WriteFile(hPipe, path, bytesToWrite, &bytesWritten, NULL) && bytesWritten >= bytesToWrite;
		if (path != argv[i]) {
			free(path);
		}
	}
	return res;
}


/**
 * Starts an asynchronous listening for new clients on the open named pipe.
 * 
 * @param[in,out] ctx - IPC context
 * @return `true` on success, else `false`
 * @remarks Shows an message box on error.
 */
bool ipcListen(tIpcWndCtx * ctx) {
	if (ctx == NULL) {
		return false;
	}
	/* reset context */
	ctx->bufLen = 0;
	ctx->state = IST_CERT_ID;
	wStrDelete(&(ctx->cfg.cert->certId));
	wStrDelete(&(ctx->cfg.cert->cardName));
	wStrDelete(&(ctx->cfg.cert->cardReader));
	rws_release(&(ctx->cfg.signApp));
	rcIniConfigBaseDelete(ctx->cfgBase);
	ctx->cfgBase = NULL;
	/* start listening for clients */
	BOOL res = ConnectNamedPipe(ctx->hPipe, &(ctx->ovClient));
	DWORD err = GetLastError();
	if (( ! res ) && err != ERROR_IO_PENDING && err != ERROR_PIPE_CONNECTED) {
		showFmtMsg(ctx->hWnd, MB_OK | MB_ICONERROR, L"Error (ipcListen)", errStr[ERR_ASYNC_LISTEN], err);
		return false;
	} else if (err == ERROR_PIPE_CONNECTED) {
		return ipcReadAsync(ctx);
	}
	ctx->waitForClient = true;
	return true;
}


/**
 * Checks whether the connected peer of the given named pipe has the same image
 * path as this application.
 * 
 * @param[in] hPipe - named pipe handle
 * @return `true` if same image path, else `false`
 */
bool ipcIsValidProcess(HANDLE hPipe) {
	ULONG peerPid;
	wchar_t peerPath[MAX_PATH];
	bool res = false;
	if (hPipe == INVALID_HANDLE_VALUE) {
		return false;
	}
	if ( ! GetNamedPipeClientProcessId(hPipe, &peerPid) ) {
		return false;
	}
	const HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)peerPid);
	if (hProc == NULL) {
		return false;
	}
	if ( ! GetModuleFileNameExW(hProc, NULL, peerPath, (DWORD)sizeof(peerPath)) ) {
		goto onError;
	}
	res = (_wcsicmp(exePath, peerPath) == 0);
onError:
	CloseHandle(hProc);
	return res;
}


/**
 * Starts an asynchronous read operation on the open named pipe.
 * 
 * @param[in,out] ctx - IPC context
 * @return `true` on success, else `false`
 * @remarks Shows an message box on error.
 */
bool ipcReadAsync(tIpcWndCtx * ctx) {
	if (ctx == NULL) {
		return false;
	}
	ctx->waitForClient = false;
	ZeroMemory(&(ctx->ovRead), sizeof(ctx->ovRead));
	if ( ! ReadFileEx(ctx->hPipe, ctx->buf + ctx->bufLen, (DWORD)(sizeof(ctx->buf) - ctx->bufLen), &(ctx->ovRead), ipcHandleReadComplete) ) {
		const DWORD err = GetLastError();
		if (err != ERROR_BROKEN_PIPE) {
			showFmtMsg(ctx->hWnd, MB_OK | MB_ICONERROR, L"Error (ipcReadAsync)", errStr[ERR_ASYNC_READ], err);
		}
		return false;
	}
	return true;
}


/**
 * Handles the read complete event from IPC client connection.
 * 
 * @param[in] dwErrorCode - I/O completion status
 * @param[in] dwNumberOfBytesTransfered - number of bytes transferred or zero on error
 * @param[in] lpOverlapped - pointer to the OVERLAPPED structure specified by the asynchronous I/O function
 */
void CALLBACK ipcHandleReadComplete(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
	if (lpOverlapped == NULL) {
		MessageBoxW(NULL, errStr[ERR_INVALID_ARG], L"Error (ipcHandleReadComplete)", MB_OK | MB_ICONERROR);
		return;
	}
	tIpcWndCtx * ctx = CONTAINER_OF(lpOverlapped, tIpcWndCtx, ovRead);
	wchar_t * file = NULL;
	if (dwErrorCode == 0 && dwNumberOfBytesTransfered > 0) {
		ctx->bufLen += (size_t)dwNumberOfBytesTransfered;
		/* handle data received in `ctx->buf` */
		while (ctx->bufLen > 0) {
			const wchar_t * start = (const wchar_t *)(ctx->buf);
			const wchar_t * it = start;
			const wchar_t * endIt = start + (ctx->bufLen >> 1);
			for (; it < endIt && *it != 0; ++it);
			if ( ! (it < endIt && *it == 0) ) {
				break; /* need more data */
			}
			/* found string */
			wchar_t ** field = NULL;
			switch (ctx->state) {
			case IST_CERT_ID:
				ctx->state = IST_CARD_NAME;
				field = &(ctx->cfg.cert->certId);
				break;
			case IST_CARD_NAME:
				ctx->state = IST_CARD_READER;
				field = &(ctx->cfg.cert->cardName);
				break;
			case IST_CARD_READER:
				ctx->state = IST_SIGN_APP;
				field = &(ctx->cfg.cert->cardReader);
				break;
			case IST_SIGN_APP:
				ctx->state = IST_FILE;
				ctx->cfgBase = rcIniConfigBaseCreate(ctx->cfg.cert);
				ctx->cfg.signApp = rws_create(start);
				if (ctx->cfgBase == NULL || ctx->cfg.signApp == NULL) {
					MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (ipcHandleReadComplete)", MB_OK | MB_ICONERROR);
					goto onError;
				}
				break;
			case IST_FILE:
				wStrDelete(&file);
				field = &file;
				break;
			}
			if (field != NULL) {
				*field = wcsdup(start);
			}
			const size_t len = (size_t)(it - start + 1);
			const size_t rem = (size_t)(ctx->bufLen - (len * sizeof(wchar_t)));
			if (rem > 0) {
				memmove(ctx->buf, ctx->buf + (len * sizeof(wchar_t)), rem);
			}
			ctx->bufLen = rem;
			if (field == NULL) {
				continue;
			}
			if (*field == NULL) {
				MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (ipcHandleReadComplete)", MB_OK | MB_ICONERROR);
				goto onError;
			}
			if (file != NULL) {
				/* add file */
				if ( ! processAddFile(ctx, ctx->cfgBase, ctx->cfg.signApp, file) ) {
					goto onError;
				}
			}
		}
		/* read next chunk */
		if ( ! ipcReadAsync(ctx) ) {
			goto onProtocolError;
		}
	} else {
		/* client connection lost -> wait for next client */
		goto onProtocolError;
	}
	wStrDelete(&file);
	return;
onProtocolError:
	wStrDelete(&file);
	DisconnectNamedPipe(ctx->hPipe);
	if ( ! ipcListen(ctx) ) {
		goto onError;
	}
	return;
onError:
	wStrDelete(&file);
	closeHandlePtr(&(ctx->hPipe), INVALID_HANDLE_VALUE);
}


/**
 * Starts processing the currently selected item.
 * 
 * @param[in,out] ctx - process context
 * @return `true` if started successfully, else `false`
 */
bool processStart(tIpcWndCtx * ctx) {
	if (ctx == NULL  || ctx->proc == NULL || ctx->proc->config == NULL || ctx->proc->signApp == NULL || ctx->proc->state != PST_IDLE) {
		return false;
	}
	tProcState newState = PST_FAIL;
	/* build read pipe name */
	GUID guid;
	CoCreateGuid(&guid);
	wchar_t pipeName[128];
	RPC_WSTR guidStr;
	if (UuidToStringW(&guid, &guidStr) != RPC_S_OK) {
		goto onEarlyError;
	}
	newState = PST_BROKEN_PIPE;
	snwprintf(pipeName, ARRAY_SIZE(pipeName), L"\\\\.\\pipe\\siguwi-read-%s", guidStr);
	RpcStringFreeW(&guidStr);
	/* create named pipe to reading process output */
	ULONG pid = 0;
	OVERLAPPED ovConn;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
	HANDLE hPipeWrite = INVALID_HANDLE_VALUE;
	HANDLE hPipeInRead = INVALID_HANDLE_VALUE;
	HANDLE hPipeInWrite = INVALID_HANDLE_VALUE;
	tUStrBuf * cmdBuf = NULL;
	wchar_t * cmd = NULL;
	ctx->hProc = NULL;
	ctx->hProcRead = INVALID_HANDLE_VALUE;
	DATA_BLOB * pin = NULL;
	DATA_BLOB rawPin;
	char * utf8Pin = NULL;
	ZeroMemory(&ovConn, sizeof(ovConn));
	ZeroMemory(&rawPin, sizeof(rawPin));
	ctx->hProcRead = CreateNamedPipeW(pipeName, PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, MAX_CONFIG_STR_LEN, MAX_CONFIG_STR_LEN, 0, NULL);
	if (ctx->hProcRead == INVALID_HANDLE_VALUE) {
		goto onEarlyError;
	}
	/* start connecting our end */
	ovConn.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if ( ! ConnectNamedPipe(ctx->hProcRead, &ovConn) ) {
		const DWORD err = GetLastError();
		if (err == ERROR_PIPE_CONNECTED) {
			SetEvent(ovConn.hEvent);
		} else if (err != ERROR_IO_PENDING) {
			goto onError;
		}
	}
	/* create child process end of the pipe */
	hPipeWrite = CreateFile(pipeName, GENERIC_WRITE, 0, &sa, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (hPipeWrite == INVALID_HANDLE_VALUE) {
		goto onError;
	}
	/* wait for both ends to connect */
	if (WaitForSingleObject(ovConn.hEvent, INFINITE) == WAIT_FAILED) {
		goto onError;
	}
	closeHandlePtr(&(ovConn.hEvent), INVALID_HANDLE_VALUE);
	/* check if we are really connected with ourself */
	if ( ! GetNamedPipeClientProcessId(ctx->hProcRead, &pid) ) {
		goto onError;
	}
	if ((ULONG)GetCurrentProcessId() != pid) {
		goto onError; /* we got hijacked */
	}
	if ( ! GetNamedPipeClientProcessId(hPipeWrite, &pid) ) {
		goto onError;
	}
	if ((ULONG)GetCurrentProcessId() != pid) {
		goto onError; /* we got hijacked */
	}
	/* create anonymous pipe to write to the process input */
	if ( ! CreatePipe(&hPipeInRead, &hPipeInWrite, &sa, 0) ) {
		goto onError;
	}
	SetHandleInformation(ctx->hProcRead, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hPipeInWrite, HANDLE_FLAG_INHERIT, 0);
	/* get and cache pin */
	newState = PST_PIN_WRONG;
	pin = hto_addKey(ctx->h, ctx->proc->config);
	if (pin == NULL) {
		goto onError;
	}
	if (pin->pbData == NULL) {
		if ( ! iniConfigGetPin(ctx->proc->config->cert, ctx->hWnd, pin) ) {
			newState = PST_PIN_MISSING;
			goto onError;
		}
		if (pin->pbData == NULL) {
			goto onError;
		}
	}
	/* decode pin */
	if ( ! CryptUnprotectData(pin, NULL, NULL, NULL, NULL, 0, &rawPin) ) {
		goto onError;
	}
	if (rawPin.pbData == NULL || rawPin.cbData < 2 || *(const wchar_t *)(rawPin.pbData + rawPin.cbData - 2) != 0) {
		goto onError;
	}
	/* build complete command-line */
	newState = PST_FAIL;
	cmdBuf = usb_create(1024);
	if (cmdBuf == NULL) {
		goto onError;
	}
	bool esc = false;
	bool hasPinArg = false;
	for (const wchar_t * ptr = ctx->proc->signApp->ptr; *ptr != 0; ++ptr) {
		if ( esc ) {
			esc = false;
			switch (*ptr) {
			case L'1':
				if (usb_add(cmdBuf, ctx->proc->path) == 0) {
					goto onError;
				}
				continue;
			case L'2':
				if (usb_add(cmdBuf, (const wchar_t *)(rawPin.pbData)) <= 0) {
					goto onError;
				}
				hasPinArg = true;
				continue;
			default:
				break;
			}
		} else if (*ptr == L'%') {
			esc = true;
			continue;
		}
		if (usb_addC(cmdBuf, *ptr) == 0) {
			goto onError;
		}
	}
	if ( hasPinArg ) {
		/* free pin */
		SecureZeroMemory(rawPin.pbData, rawPin.cbData);
		LocalFree(rawPin.pbData);
		ZeroMemory(&rawPin, sizeof(rawPin));
	}
	/* get complete command-line as string */
	cmd = usb_get(cmdBuf);
	if (cmd == NULL) {
		goto onError;
	}
	usb_wipe(cmdBuf);
	usb_delete(cmdBuf);
	cmdBuf = NULL;
	/* create child process */
	ZeroMemory(&si, sizeof(si));
	si.cb          = sizeof(si);
	si.dwFlags     = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdInput   = hPipeInRead;
	si.hStdOutput  = hPipeWrite;
	si.hStdError   = hPipeWrite;
	if ( ! CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, exeDir, &si, &pi) ) {
		newState = PST_APP_NOT_FOUND;
		goto onError;
	}
	/* free used command-line string */
	SecureZeroMemory(cmd, wcslen(cmd) * sizeof(wchar_t));
	free(cmd);
	cmd = NULL;
	/* close handles that got passed to the child process */
	closeHandlePtr(&hPipeInRead, INVALID_HANDLE_VALUE);
	closeHandlePtr(&hPipeWrite, INVALID_HANDLE_VALUE);
	closeHandlePtr(&(pi.hThread), NULL);
	ctx->hProc = pi.hProcess;
	/* pass UTF-8 pin */
	if ( ! hasPinArg ) {
		newState = PST_PIN_MISSING;
		utf8Pin = wToUtf8((const wchar_t *)(rawPin.pbData));
		if (utf8Pin == NULL) {
			goto onError;
		}
		const DWORD utf8Len = (DWORD)strlen(utf8Pin);
		DWORD bytesWritten;
		if (WriteFile(hPipeInWrite, utf8Pin, utf8Len, &bytesWritten, NULL) == 0 || bytesWritten != utf8Len) {
			goto onError;
		}
		FlushFileBuffers(hPipeInWrite);
		/* free UTF-8 pin */
		SecureZeroMemory(utf8Pin, strlen(utf8Pin));
		free(utf8Pin);
		utf8Pin = NULL;
		/* free pin */
		SecureZeroMemory(rawPin.pbData, rawPin.cbData);
		LocalFree(rawPin.pbData);
		ZeroMemory(&rawPin, sizeof(rawPin));
	}
	closeHandlePtr(&hPipeInWrite, INVALID_HANDLE_VALUE);
	ZeroMemory(&(ctx->utf8), sizeof(ctx->utf8));
	ctx->outputLen = 0;
	ctx->lastChar = 0;
	ctx->proc->state = PST_RUNNING;
	if ( ! processReadAsync(ctx) ) {
		processFinish(ctx);
		return processNext(ctx);
	}
	return true;
onError:
	if (utf8Pin != NULL) {
		SecureZeroMemory(utf8Pin, strlen(utf8Pin));
		free(utf8Pin);
	}
	closeHandlePtr(&(ctx->hProc), NULL);
	if (cmd != NULL) {
		SecureZeroMemory(cmd, wcslen(cmd) * sizeof(wchar_t));
		free(cmd);
	}
	if (cmdBuf != NULL) {
		usb_wipe(cmdBuf);
		usb_delete(cmdBuf);
	}
	if (rawPin.pbData != NULL) {
		SecureZeroMemory(rawPin.pbData, rawPin.cbData);
		LocalFree(rawPin.pbData);
	}
	closeHandlePtr(&hPipeInWrite, INVALID_HANDLE_VALUE);
	closeHandlePtr(&hPipeInRead, INVALID_HANDLE_VALUE);
	closeHandlePtr(&(ovConn.hEvent), INVALID_HANDLE_VALUE);
	closeHandlePtr(&(ctx->hProcRead), INVALID_HANDLE_VALUE);
	closeHandlePtr(&hPipeWrite, INVALID_HANDLE_VALUE);
onEarlyError:
	ctx->proc->state = newState;
	return false;
}


/**
 * Select next item in queue and start processing it.
 * 
 * @param[in,out] ctx - process context
 * @return `true` if started successfully, else `false`
 */
bool processNext(tIpcWndCtx * ctx) {
	if (ctx == NULL || ctx->v == NULL || (ctx->proc != NULL && ctx->proc->state == PST_RUNNING)) {
		return false;
	}
	const size_t count = vec_size(ctx->v);
	for (size_t i = ctx->vi; i < count; ++i) {
		tProcCtx * proc = vec_at(ctx->v, i);
		if (proc == NULL) {
			return false;
		}
		if (proc->state != PST_IDLE) {
			continue;
		}
		ctx->proc = proc;
		ctx->vi = i;
		break;
	}
	const bool res = processStart(ctx);
	processUpdateItem(ctx, ctx->vi);
	return res;
}


/**
 * Starts an asynchronous read operation on the open named pipe from the started process.
 * 
 * @param[in,out] ctx - process context
 * @return `true` on success, else `false`
 */
bool processReadAsync(tIpcWndCtx * ctx) {
	if (ctx == NULL || ctx->proc == NULL) {
		return false;
	}
	ZeroMemory(&(ctx->ovProcRead), sizeof(ctx->ovProcRead));
	if ( ! ReadFileEx(ctx->hProcRead, ctx->procBuf, (DWORD)sizeof(ctx->procBuf), &(ctx->ovProcRead), processHandleReadComplete) ) {
		return false;
	}
	return true;
}


/**
 * Handles the read complete event from process output pipe.
 * 
 * @param[in] dwErrorCode - I/O completion status
 * @param[in] dwNumberOfBytesTransfered - number of bytes transferred or zero on error
 * @param[in] lpOverlapped - pointer to the OVERLAPPED structure specified by the asynchronous I/O function
 */
void CALLBACK processHandleReadComplete(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
	if (lpOverlapped == NULL) {
		return;
	}
	tIpcWndCtx * ctx = CONTAINER_OF(lpOverlapped, tIpcWndCtx, ovProcRead);
	if (dwErrorCode == 0 && dwNumberOfBytesTransfered > 0 && ctx->proc && ctx->proc->output) {
		/* handle data received in `ctx->procBuf` */
		const uint8_t * ptr = ctx->procBuf;
		tUtf8Ctx * utf8 = &(ctx->utf8);
		tUStrBuf * output = ctx->proc->output;
		size_t * outputLen = &(ctx->outputLen);
		uint32_t * lastChr = &(ctx->lastChar);
		bool added = false;
		for (DWORD i = 0; i < dwNumberOfBytesTransfered && *outputLen < PROCESS_MAX_OUTPUT; ++i) {
			uint32_t cp = utf8_parse(utf8, ptr[i]);
			if (cp == UTF8_MORE) {
				continue;
			}
			if (cp > 0x10FFFF) {
				cp = UTF8_ERROR;
			}
			if (cp < 0x10000) {
				/* direct encoding */
				if (cp == L'\n' && *lastChr != L'\r') {
					/* fix line ending */
					usb_addC(output, L'\r');
				}
				if (cp != 0) {
					usb_addC(output, (wchar_t)cp);
				}
			} else {
				/* surrogate pair encoding */
				const uint32_t sp = (uint32_t)(cp - 0x10000);
				usb_addC(output, (wchar_t)((sp >> 10) + 0xD800));
				usb_addC(output, (wchar_t)((sp & 0x3FF) + 0xDC00));
			}
			*lastChr = cp;
			++(*outputLen);
			added = true;
		}
		/* update process list and output widget */
		if ( added ) {
			if (*outputLen >= PROCESS_MAX_OUTPUT) {
				usb_add(output,
					L"\r\n--------------------------------------------------------------------------------"
					"\r\nThe output has been truncated here."
				);
			}
			processUpdateItem(ctx, ctx->vi);
		}
		/* read next chunk */
		if ( ! processReadAsync(ctx) ) {
			goto onError;
		}
	} else {
		/* child process terminated */
		goto onError;
	}
	return;
onError:
	processFinish(ctx);
	processNext(ctx);
}


/**
 * Waits for the child process termination and updates the process item status.
 * 
 * @param[in,out] ctx - process context
 * @return `true` on success, else `false`
 */
bool processFinish(tIpcWndCtx * ctx) {
	if (ctx == NULL || ctx->proc == NULL) {
		return false;
	}
	if (WaitForSingleObject(ctx->hProc, INFINITE) == WAIT_FAILED) {
		goto onError;
	}
	DWORD dwExitCode;
	if ( ! GetExitCodeProcess(ctx->hProc, &dwExitCode) ) {
		goto onError;
	}
	if (dwExitCode != 0) {
		usb_addFmt(
			ctx->proc->output,
			L"\r\n--------------------------------------------------------------------------------"
			"\r\nCommand failed with exit code %" PRIu32 ".",
			(uint32_t)dwExitCode
		);
		goto onError;
	}
	ctx->proc->state = PST_OK;
	ctx->proc = NULL;
	processUpdateItem(ctx, ctx->vi);
	return true;
onError:
	ctx->proc->state = PST_FAIL;
	processUpdateItem(ctx, ctx->vi);
	return false;
}


/**
 * Adds a single file with the given configuration to the internal process list
 * to process it.
 * 
 * @param[in] ctx - Window/IPC context
 * @param[in] c - INI configuration base
 * @param[in] signApp - code signing application command-line
 * @param[in] path - path to the file to add (can be relative)
 * @return `true` on success, else `false` after showing an error message
 */
bool processAddFile(tIpcWndCtx * ctx, tRcIniConfigBase * c, tRcWStr * signApp, const wchar_t * path) {
	if (ctx == NULL || c == NULL || signApp == NULL || path == NULL) {
		showFmtMsg(ctx->hWnd, MB_OK | MB_ICONERROR, L"Error (processAddFile)", L"%s", errStr[ERR_INVALID_ARG]);
		return false;
	}
	tProcCtx * item = vec_pushBack(ctx->v);
	if (item == NULL) {
		showFmtMsg(ctx->hWnd, MB_OK | MB_ICONERROR, L"Error (processAddFile)", L"%s", errStr[ERR_OUT_OF_MEMORY]);
		return false;
	}
	item->state = PST_IDLE;
	item->config = rcIniConfigBaseClone(c);
	item->signApp = rws_aquire(signApp);
	item->path = wcsdup(path);
	item->output = usb_create(4096);
	wToFullPath(&(item->path), true);
	if (item->path == NULL) {
		showFmtMsg(ctx->hWnd, MB_OK | MB_ICONERROR, L"Error (processAddFile)", L"%s", errStr[ERR_OUT_OF_MEMORY]);
		return false;
	}
	if ( ! wFileExists(item->path) ) {
		item->state = PST_FILE_NOT_FOUND;
	}
	if ( ! processAddItem(ctx, item) ) {
		showFmtMsg(ctx->hWnd, MB_OK | MB_ICONERROR, L"Error (processAddFile)", L"%s", errStr[ERR_OUT_OF_MEMORY]);
		return false;
	}
	processNext(ctx);
	return true;
}


/**
 * Adds a new item to the process list widget.
 * 
 * @param[in] ctx - Window/IPC context
 * @param[in] item - item to add
 * @return `true` on success, else `false`
 */
bool processAddItem(const tIpcWndCtx * ctx, const tProcCtx * item) {
	if (ctx == NULL || item == NULL) {
		return false;
	}
	const int count = ListView_GetItemCount(ctx->hList);
	LVITEMW lvi;
	ZeroMemory(&lvi, sizeof(lvi));
	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	lvi.iItem = count;
	lvi.iSubItem = PCI_FILE;
	lvi.pszText = wFileName(item->path);
	lvi.lParam = (LPARAM)item;
	if (ListView_InsertItem(ctx->hList, &lvi) < 0) {
		return false;
	}
	ListView_SetItemText(ctx->hList, count, PCI_RESULT, procStateStr[item->state]);
	if (lvi.pszText != item->path) {
		/* set only containing folder path */
		wchar_t oldCh = *(lvi.pszText);
		*(lvi.pszText) = 0;
		ListView_SetItemText(ctx->hList, count, PCI_PATH, item->path);
		*(lvi.pszText) = oldCh;
	} /* else: leave empty */
	return true;
}


/**
 * Updates the result column in the process list widget for the item with the
 * given index.
 * 
 * @param[in] ctx - Window/IPC context
 * @param[in] i - item index
 * @return `true` on success, else `false`
 */
bool processUpdateItem(const tIpcWndCtx * ctx, const size_t i) {
	if (ctx == NULL || ctx->v == NULL) {
		return false;
	}
	const tProcCtx * item = vec_at(ctx->v, i);
	if (item == NULL) {
		return false;
	}
	ListView_SetItemText(ctx->hList, (int)i, PCI_RESULT, procStateStr[item->state]);
	if (ctx->selList == (int)i) {
		/* update output */
		wchar_t * str = usb_get(item->output);
		if (str != NULL) {
			const int oldLen = GetWindowTextLengthW(ctx->hInfo);
			DWORD oldStart, oldEnd;
			SendMessageW(ctx->hInfo, EM_GETSEL, (WPARAM)(&oldStart), (LPARAM)(&oldEnd));
			const bool wasAtEnd = (oldStart == oldEnd && oldStart == (DWORD)oldLen);
			SendMessageW(ctx->hInfo, WM_SETREDRAW, FALSE, 0);
			SetWindowTextW(ctx->hInfo, str);
			SendMessageW(ctx->hInfo, WM_SETREDRAW, TRUE, 0);
			if (wasAtEnd && ctx->proc != NULL && i == ctx->vi) {
				const int newLen = GetWindowTextLengthW(ctx->hInfo);
				SendMessageW(ctx->hInfo, EM_SETSEL, (WPARAM)newLen, (LPARAM)newLen);
				SendMessageW(ctx->hInfo, EM_SCROLLCARET, 0, 0);
			} else {
				SendMessageW(ctx->hInfo, EM_SETSEL, (WPARAM)oldStart, (LPARAM)oldEnd);
			}
			InvalidateRect(ctx->hInfo, NULL, TRUE);
			free(str);
		}
	}
	return true;
}


/**
 * Updates the process window controls after a change in the window size.
 * 
 * @param[in] ctx - Window/IPC context
 */
void processWndResize(const tIpcWndCtx * ctx) {
	const int sepHeight = SEP_WIDTH;
	const int sepHalf = sepHeight / 2;
	RECT rect;
	GetClientRect(ctx->hWnd, &rect);
	const int width = rect.right;
	const int height = rect.bottom;
	const int sepMid = (int)lroundf((float)height * ctx->sepPos);
	HDWP hDwp = BeginDeferWindowPos(3);
	hDwp = DeferWindowPos(hDwp, ctx->hList, NULL, 10, 10, width - 20, sepMid - sepHalf - 10, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
	hDwp = DeferWindowPos(hDwp, ctx->hSep, NULL, 10, sepMid - sepHalf, width - 20, sepHeight, SWP_NOZORDER | SWP_NOACTIVATE);
	hDwp = DeferWindowPos(hDwp, ctx->hInfo, NULL, 10, sepMid + sepHalf, width - 20, height - (sepMid + sepHalf + 10), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
	EndDeferWindowPos(hDwp);
	InvalidateRect(ctx->hWnd, NULL, FALSE);
}


/**
 * Window callback handler for the signing process window separator.
 *
 * @param[in] hWnd - window handle
 * @param[in] msg - event message
 * @param[in] wParam - associated wParam
 * @param[in] lParam - associated lParam
 * @return result value
 */
LRESULT CALLBACK processSepWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	tIpcWndCtx * ctx = (tIpcWndCtx *)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
	switch (msg) {
	case WM_SETCURSOR:
		SetCursor(LoadCursor(NULL, IDC_SIZENS));
		return TRUE;
	case WM_LBUTTONDOWN:
		if (ctx && ( ! ctx->sepActive )) {
			ctx->sepActive = true;
			SetCapture(hWnd);
		}
		break;
	case WM_MOUSEMOVE:
		if (ctx && ctx->sepActive) {
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(ctx->hWnd, &pt);
			RECT rect;
			GetClientRect(ctx->hWnd, &rect);
			/* allow at least 70px at the top and bottom */
			const float minY = 70.f / (float)(rect.bottom);
			const float maxY = 1.0f - (70.f / (float)(rect.bottom));
			const float mid = (float)(pt.y) / (float)(rect.bottom);
			ctx->sepPos = fmaxf(minY, fminf(maxY, mid));
			processWndResize(ctx);
		}
		break;
	case WM_LBUTTONUP:
		if (ctx && ctx->sepActive) {
			ctx->sepActive = false;
			ReleaseCapture();
			InvalidateRect(ctx->hWnd, NULL, TRUE);
		}
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

/**
 * Sub class for the `WC_EDITW` control to enable tab navigation.
 * 
 * @param[in] hWnd - window handle
 * @param[in] msg - event message
 * @param[in] wParam - associated wParam
 * @param[in] lParam - associated lParam
 * @param[in] uIdSubclass - sub class ID
 * @param[in] dwRefData - data provided via `SetWindowSubclass`
 * @return result value
 */
static LRESULT CALLBACK processEditSubClassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	PCF_UNUSED(uIdSubclass);
	if (msg == WM_KEYDOWN && wParam == VK_TAB) {
		SetFocus((HWND)dwRefData);
		return 0;
	}
	return DefSubclassProc(hWnd, msg, wParam, lParam);
}


/**
 * Window callback handler for the signing process window.
 *
 * @param[in] hWnd - window handle
 * @param[in] msg - event message
 * @param[in] wParam - associated wParam
 * @param[in] lParam - associated lParam
 * @return result value
 */
LRESULT CALLBACK processWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	tIpcWndCtx * ctx = (msg == WM_CREATE) ? (tIpcWndCtx *)(((CREATESTRUCTW *)lParam)->lpCreateParams) : (tIpcWndCtx *)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
	if (ctx == NULL) {
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	switch (msg) {
	case WM_CREATE: {
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)ctx);
		LPCREATESTRUCTW init = (CREATESTRUCTW *)lParam;
		const int width = init->cx;
		ctx->hWnd = hWnd;
		ctx->hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, 0, 0, 0, hWnd, (HMENU)IDC_PROCESS_LIST, gInst, NULL);
		ctx->hSep = CreateWindowW(WC_STATICW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, gInst, NULL);
		ctx->hInfo = CreateWindowW(WC_EDITW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_HSCROLL | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0, 0, 0, 0, hWnd, (HMENU)IDC_PROCESS_INFO, gInst, NULL);
		ctx->selList = -1;
		ctx->sepPos = 0.5f;
		if ( ! (ctx->hList && ctx->hInfo && ctx->hSep) ) {
			CloseWindow(hWnd);
			break;
		}
		SetWindowSubclass(ctx->hInfo, processEditSubClassProc, 1, (DWORD_PTR)(ctx->hList));
		/* set fonts */
		SendMessageW(hWnd, WM_SETFONT, (WPARAM)(ctx->hFont), TRUE);
		SendMessageW(ctx->hList, WM_SETFONT, (WPARAM)(ctx->hFont), TRUE);
		SendMessageW(ctx->hInfo, WM_SETFONT, (WPARAM)(ctx->hFont), TRUE);
		/* set extended list view styles */
		SendMessageW(ctx->hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		/* add list view columns */
		static const struct {
			int width;
			wchar_t * const name;
		} columns[] = {
			{120, L"File"},   /* PCI_FILE */
			{100, L"Result"}, /* PCI_RESULT */
			{-1,  L"Path"}    /* PCI_PATH */
		};
		LVCOLUMNW lvc;
		ZeroMemory(&lvc, sizeof(lvc));
		lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		int colWidth = 0;
		for (size_t i = 0; i < ARRAY_SIZE(columns); ++i) {
			lvc.cx = columns[i].width;
			lvc.pszText = columns[i].name;
			lvc.iSubItem = (int)i;
			if (lvc.cx == -1) {
				lvc.cx = width - colWidth - 70;
			}
			colWidth += lvc.cx;
			ListView_InsertColumn(ctx->hList, (int)i, &lvc);
		}
		SetWindowLongPtr(ctx->hSep, GWLP_USERDATA, (LONG_PTR)ctx);
		SetWindowLongPtr(ctx->hSep, GWLP_WNDPROC, (LONG_PTR)processSepWndProc);
		processWndResize(ctx);
		} break;
	case WM_NOTIFY: {
		const NMHDR * nmhdr = (const NMHDR *)lParam;
		if (nmhdr->code == LVN_ITEMCHANGED && nmhdr->idFrom == IDC_PROCESS_LIST) {
			const int selList = ListView_GetNextItem(ctx->hList, -1, LVNI_SELECTED);
			if (selList != ctx->selList) {
				ctx->selList = selList;
				if (selList >= 0) {
					processUpdateItem(ctx, (size_t)(ctx->selList));
				} else {
					SetWindowTextW(ctx->hInfo, L"");
				}
			}
		}
		} break;
	case WM_GETMINMAXINFO: {
		MINMAXINFO * pmmi = (MINMAXINFO *)lParam;
		pmmi->ptMinTrackSize.x = 500;
		pmmi->ptMinTrackSize.y = 300;
		} return 0;
	case WM_SIZE:
		processWndResize(ctx);
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		ctx->hList = NULL;
		ctx->hSep = NULL;
		ctx->hInfo = NULL;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}


/**
 * Shows the process window or transmits the request to an existing one.
 * 
 * @param[in] c - INI configuration
 * @param[in] cmdshow - `ShowWindow` parameter
 * @param[in] argc - number of files to sign
 * @param[in] argv - list of files to sign
 * @return program exit code
 */
int showProcess(const tIniConfig * c, int cmdshow, int argc, wchar_t ** argv) {
	int res = EXIT_FAILURE;
	bool isServer = true;
	const int dpi = getDpi();
	tIpcWndCtx ctx;
	ZeroMemory(&ctx, sizeof(ctx));
	ctx.hPipe = INVALID_HANDLE_VALUE;
	ctx.waitForClient = true;
	/* input value check */
	if (c == NULL || (argc > 0 && argv == NULL && argv[0] == NULL)) {
		MessageBoxW(NULL, errStr[ERR_INVALID_ARG], L"Error (showProcess)", MB_OK | MB_ICONERROR);
		goto onError;
	}
	/* processing context initialization */
	ctx.v = vec_create(sizeof(tProcCtx));
	if (ctx.v == NULL) {
		MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (showProcess)", MB_OK | MB_ICONERROR);
		goto onError;
	}
	ctx.h = hto_create(
		sizeof(DATA_BLOB),
		64,
		(HashFunctionCloneO)rcIniConfigBaseClone,
		(HashFunctionDelO)rcIniConfigBaseDelete,
		(HashFunctionCmpO)rcIniConfigBaseCmp,
		(HashFunctionHashO)rcIniConfigBaseHash
	);
	if (ctx.h == NULL) {
		MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (showProcess)", MB_OK | MB_ICONERROR);
		goto onError;
	}
	/* IPC setup */
	for (size_t i = 0; i < 3; ++i) {
		/* try to act as IPC server */
		ctx.hPipe = CreateNamedPipeW(IPC_PIPE_PATH, PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, IPC_MAX_CLIENTS, 0, MAX_CONFIG_STR_LEN, 0, NULL);
		if (ctx.hPipe == INVALID_HANDLE_VALUE) {
			/* try to connect to an existing server */
			ctx.hPipe = CreateFileW(IPC_PIPE_PATH, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (ctx.hPipe == INVALID_HANDLE_VALUE) {
				/* wait and try again */
				Sleep(100);
				continue;
			}
			isServer = false;
			break;
		}
		if (ctx.ovClient.hEvent == NULL) {
			ctx.ovClient.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		}
		if (ctx.ovClient.hEvent == NULL) {
			continue;
		}
		/* run as IPC server */
		break;
	}
	if (ctx.hPipe == INVALID_HANDLE_VALUE) {
		showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (showProcess)", errStr[ERR_OPEN_NAMED_PIPE], GetLastError());
		goto onError;
	}
	if ( ! isServer ) {
		/* act as IPC client and transmit INI configuration to server */
		if (argc > 0) {
			if ( ! ipcSendReqToServer(ctx.hPipe, c, argc, argv) ) {
				showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (showProcess)", errStr[ERR_WRITE_NAMED_PIPE], GetLastError());
				goto onError;
			}
		}
		goto onSuccess;
	}
	/* load default window font */
	ctx.hFont = CreateFontW(calcFontSize(85, dpi), 0, 0, 0, 0, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"MS Shell Dlg");
	if (ctx.hFont == NULL) {
		MessageBoxW(NULL, errStr[ERR_CREATEFONT], L"Error (list)", MB_OK | MB_ICONERROR);
		goto onError;
	}
	/* register window class */
	WNDCLASSEXW wc = {
		/* cbSize        */ sizeof(WNDCLASSEXW),
		/* style         */ CS_HREDRAW | CS_VREDRAW,
		/* lpfnWndProc   */ processWndProc,
		/* cbClsExtra    */ 0,
		/* cbWndExtra    */ 0,
		/* hInstance     */ gInst,
		/* hIcon         */ LoadIconW(gInst, MAKEINTRESOURCEW(IDI_APP_ICON)),
		/* hCursor       */ LoadCursorW(NULL, IDC_ARROW),
		/* hbrBackground */ (HBRUSH)COLOR_3DSHADOW,
		/* lpszMenuName  */ NULL,
		/* lpszClassName */ L"ProcessClass",
		/* hIconSm       */ LoadIconW(gInst, MAKEINTRESOURCEW(IDI_APP_ICON))
	};
	RegisterClassExW(&wc);
	/* initialize common controls */
	INITCOMMONCONTROLSEX icex = {sizeof(icex), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icex);
	/* create and show window */
	HWND hWnd = CreateWindowW(wc.lpszClassName, L"Signing process", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, gInst, (LPVOID)&ctx);
	ShowWindow(hWnd, cmdshow);
	UpdateWindow(hWnd);
	if (argc > 0) {
		/* create configuration environment */
		ctx.cfgBase = rcIniConfigBaseCreate(c->cert);
		if (ctx.cfgBase == NULL) {
			MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (showProcess)", MB_OK | MB_ICONERROR);
			goto onError;
		}
		/* add files to process list */
		for (int i = 0; i < argc; ++i) {
			if ( ! processAddFile(&ctx, ctx.cfgBase, c->signApp, argv[i]) ) {
				goto onError;
			}
		}
	}
	/* run as IPC server and show process window */
	if ( ! ipcListen(&ctx) ) {
		goto onError;
	}
	DWORD waitResult;
	MSG msg;
	for (;;) {
		if ( ctx.waitForClient ) {
			waitResult = MsgWaitForMultipleObjectsEx(1, &(ctx.ovClient.hEvent), INFINITE, QS_ALLINPUT, MWMO_ALERTABLE);
			if (waitResult == WAIT_OBJECT_0) {
				/* handle new client */
				DWORD dummy;
				BOOL ok = GetOverlappedResult(ctx.hPipe, &(ctx.ovClient), &dummy, FALSE);
				if (ok || GetLastError() == ERROR_PIPE_CONNECTED) {
					if ( ! (ipcIsValidProcess(ctx.hPipe) && ipcReadAsync(&ctx)) ) {
						/* wait for next client */
						DisconnectNamedPipe(ctx.hPipe);
						if ( ! ipcListen(&ctx) ) {
							goto onError;
						}
					}
				} else {
					/* wait for next client */
					DisconnectNamedPipe(ctx.hPipe);
					if ( ! ipcListen(&ctx) ) {
						goto onError;
					}
				}
			}
		} else {
			waitResult = MsgWaitForMultipleObjectsEx(0, NULL, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE);
		}
		if (waitResult == WAIT_IO_COMPLETION) {
			continue;
		}
		while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ) {
			if (msg.message == WM_QUIT) {
				goto onSuccess;
			}
			if ( ! IsDialogMessage(hWnd, &msg) ) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
onSuccess:
	res = EXIT_SUCCESS;
onError:
	if (ctx.hFont != NULL) {
		DeleteObject(ctx.hFont);
	}
	closeHandlePtr(&(ctx.ovClient.hEvent), NULL);
	closeHandlePtr(&(ctx.hPipe), INVALID_HANDLE_VALUE);
	wStrDelete(&(ctx.cfg.cert->certId));
	wStrDelete(&(ctx.cfg.cert->cardName));
	wStrDelete(&(ctx.cfg.cert->cardReader));
	rws_release(&(ctx.cfg.signApp));
	rcIniConfigBaseDelete(ctx.cfgBase);
	if (ctx.h != NULL) {
		hto_traverse(ctx.h, (HashVisitorO)pinBlobDelete, NULL);
		hto_delete(ctx.h);
	}
	if (ctx.v != NULL) {
		vec_traverse(ctx.v, (VectorVisitor)procCtxDelete, NULL);
		vec_delete(ctx.v);
	}
	closeHandlePtr(&(ctx.hProc), INVALID_HANDLE_VALUE);
	return res;
}
