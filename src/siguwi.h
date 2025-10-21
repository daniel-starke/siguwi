/**
 * @file siguwi.h
 * @author Daniel Starke
 * @date 2025-06-14
 * @version 2025-10-21
 */
#ifndef __SIGUWI_H__
#define __SIGUWI_H__

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <ntsecapi.h>
#include <objbase.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincred.h>
#include <winnls.h>
#include <winscard.h>
#include "getopt.h"
#include "htableo.h"
#include "rcwstr.h"
#include "resource.h"
#include "target.h"
#include "ustrbuf.h"
#include "utf8.h"
#include "vector.h"


#if ! (defined(UNICODE) && defined(_UNICODE))
#error "Please define UNICODE and _UNICODE."
#endif /* not (UNICODE and _UNICODE) */


extern int __wgetmainargs(int *, wchar_t ***, wchar_t ***, int, int *);


/**
 * Returns the number of array elements for compile time known size arrays.
 *
 * @param[in] x - array to count
 * @return number of array elements
 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))


/**
 * Default configuration group string.
 */
#define DEFAULT_CONFIG_GROUP L"siguwi"


/**
 * Default registry context menu entry text string.
 */
#define DEFAULT_REG_TEXT L"Sign Code"


/**
 * Inter-process communication pipe path.
 * This uses a UUIDv4 which changes whenever the interface changes.
 */
#define IPC_PIPE_PATH L"\\\\.\\pipe\\9f018697-0779-46c5-b562-cef59fef78ff"


/**
 * Maximum number of inter-process communication clients.
 *
 * @remarks Only tested with `1`.
 */
#define IPC_MAX_CLIENTS 1


/**
 * Maximum number of characters for a Windows registry key name.
 *
 * @see https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-element-size-limits
 */
#define MAX_REG_KEY_NAME 255


/**
 * Maximum number of characters per configuration string value.
 * @see `tConfig`
 */
#define MAX_CONFIG_STR_LEN (4*1024)


/**
 * Maximum number of characters per configuration file.
 * @see `iniConfigParse()`
 */
#define MAX_CONFIG_FILE_LEN (4*1024*1024)


/**
 * Certificate service provider name.
 */
#define PROVIDER_NAME L"Microsoft Base Smart Card Crypto Provider"


/**
 * Certificate service provider type.
 */
#define PROV_TYPE PROV_RSA_FULL


/**
 * Maximum number of characters in the process application output log.
 */
#define PROCESS_MAX_OUTPUT (1024*1024)


/**
 * Returns the container base point of the given member pointer.
 *
 * @param[in] ptr - member pointer
 * @param[in] type - container type
 * @param[in] member - member field
 * @return container base pointer
 */
#define CONTAINER_OF(ptr, type, member) ((type *)((uint8_t *)(ptr) - offsetof(type, member)))


/**
 * Number of pixel for the widget separator.
 */
#define SEP_WIDTH 6


#ifndef CRED_PACK_PROTECTED_CREDENTIALS
#define CRED_PACK_PROTECTED_CREDENTIALS 0x1
#endif /* CRED_PACK_PROTECTED_CREDENTIALS */


/**
 * Possible internal error codes.
 *
 * @remarks Synchronize with `errStr`!
 */
typedef enum {
	ERR_SUCCESS,
	ERR_UNKNOWN,
	ERR_INVALID_ARG,
	ERR_OUT_OF_MEMORY,
	ERR_OPT_NO_ARG,
	ERR_OPT_AMB_C,
	ERR_OPT_AMB_S,
	ERR_OPT_AMB_X,
	ERR_PRINTF_FMT,
	ERR_CREATEFONT,
	ERR_NO_SMARTCARD,
	ERR_GET_STATUS,
	ERR_GET_CSP,
	ERR_CREATE_FILE,
	ERR_READ_FILE,
	ERR_GET_EXE_PATH,
	ERR_REL_CONFIG_PATH,
	ERR_SYNTAX_ERROR,
	ERR_LARGE_CONFIG,
	ERR_MISSING_FIELD,
	ERR_CREATE_PIPE,
	ERR_OPEN_NAMED_PIPE,
	ERR_WRITE_NAMED_PIPE,
	ERR_ASYNC_LISTEN,
	ERR_ASYNC_READ,
	ERR_CREATE_EVENT,
	ERR_GET_STD_HANDLE,
	ERR_INVALID_REG_VERB,
	ERR_INIT_COM,
	ERR_FILE_NOT_FOUND
} tErrCode;


/**
 * Possible context menu registration modes.
 */
typedef enum {
	RM_NONE,
	RM_REGISTER,
	RM_UNREGISTER
} tRegMode;


/**
 * Possible internal processing states.
 *
 * @remarks Synchronize with `procStateStr`!
 */
typedef enum {
	PST_IDLE,
	PST_RUNNING,
	PST_OK,
	PST_FAIL,
	PST_FILE_NOT_FOUND,
	PST_BROKEN_PIPE,
	PST_APP_NOT_FOUND,
	PST_PIN_MISSING,
	PST_PIN_WRONG
} tProcState;


/**
 * Possible IPC server states.
 */
typedef enum {
	IST_CERT_ID,
	IST_CARD_NAME,
	IST_CARD_READER,
	IST_SIGN_APP,
	IST_FILE
} tIpcState;


/**
 * Possible internal processing list column indices.
 */
typedef enum {
	PCI_FILE,
	PCI_RESULT,
	PCI_PATH
} tProcColumnIndex;


/**
 * Single certificate configuration.
 */
typedef struct {
	wchar_t * certProv; /**< dynamically set from `cardName` via `getCspFromCardNameW()` */
	wchar_t * certId;
	wchar_t * certName;
	wchar_t * certSubj;
	wchar_t * cardName;
	wchar_t * cardReader;
} tConfig;


/**
 * Configuration list window context data.
 */
typedef struct {
	HFONT hFont;
	HWND hWnd;
	HWND hCombo;
	HWND hEdit;
	HWND hButton;
	tVector * v;
	tUStrBuf * sb;
} tConfigWndCtx;


/**
 * Single INI file configuration part related to a certificate.
 */
typedef struct {
	wchar_t * certProv; /**< dynamically set from `cardName` via `getCspFromCardNameW()` */
	wchar_t * certId;
	wchar_t * cardName;
	wchar_t * cardReader;
} tIniConfigBase;


/**
 * Reference counted single INI file configuration part related to a certificate.
 */
typedef struct {
	LONG refCount;
	tIniConfigBase cert[1];
} tRcIniConfigBase;


/**
 * Single INI file configuration.
 */
typedef struct {
	tIniConfigBase cert[1];
	tRcWStr * signApp;
} tIniConfig;


/**
 * Single file position.
 */
typedef struct {
	size_t row; /**< Line number starting at 1. */
	size_t col; /**< Column within the line starting at 1. */
} tFilePos;


/**
 * Single file position.
 */
typedef struct {
	wchar_t * ptr;
	size_t len;
} tToken;


/**
 * Single signing process context.
 */
typedef struct {
	tProcState state;
	tRcIniConfigBase * config;
	tRcWStr * signApp;
	wchar_t * path;
	tUStrBuf * output;
	bool pinValid;
} tProcCtx;


/**
 * Process window IPC context and associated handles.
 */
typedef struct {
	/* IPC context */
	HANDLE hPipe; /**< named pipe handle for IPC clients */
	OVERLAPPED ovClient; /**< asynchronous IPC client connection structure */
	OVERLAPPED ovRead; /**< asynchronous IPC read structure */
	bool waitForClient; /**< wait for IPC client? */
	char buf[MAX_CONFIG_STR_LEN * sizeof(wchar_t)];
	size_t bufLen; /**< bytes in `buf` */
	tIniConfig cfg; /**< used for reading IPC data from the remote application */
	tRcIniConfigBase * cfgBase; /**< created from `cfg` to assign it to the process items */
	tIpcState state; /**< current IPC reading state */
	/* processing context */
	tVector * v; /**< item (`tProcCtx`) list */
	tHTableO * h; /**< config (`tRcIniConfigBase`) to pin (`DATA_BLOB`) map */
	tProcCtx * proc; /**< points into `vec_at(v, vi)` */
	size_t vi; /**< current item index in `v` */
	HANDLE hProc; /**< current signing process handle or `NULL` */
	HANDLE hProcRead; /**< pipe handle to read the signing process output */
	OVERLAPPED ovProcRead; /**< overlapped structure to read from the signing process */
	uint8_t procBuf[MAX_CONFIG_STR_LEN]; /**< read buffer for signing process output */
	tUtf8Ctx utf8; /**< parsing context for UTF-8 data from signing process */
	size_t outputLen; /**< current length in `proc->output` in number of Unicode code points */
	uint32_t lastChar; /**< most recent Unicode code point added to `proc->output` */
	/* window context */
	HFONT hFont;
	HWND hWnd;
	HWND hList;
	HWND hSep;
	HWND hInfo;
	float sepPos;
	bool sepActive;
	int selList;
	tRcIniConfigBase * cmdlCfg; /**< parsed INI file content passed on command-line */
	tRcWStr * cmdlSignApp; /**< signing application command-line from command-line INI file */
} tIpcWndCtx;


/**
 * Standard I/O translation context.
 */
typedef struct {
	UINT acp;
	HANDLE hIn;
	HANDLE hOut;
	bool isSeekable;
	OVERLAPPED ovRead;
	DWORD lastFlush;
	DWORD64 received;
	size_t inLen;
	char inBuf[MAX_CONFIG_STR_LEN];
	wchar_t wBuf[MAX_CONFIG_STR_LEN];
	char outBuf[4 * MAX_CONFIG_STR_LEN];
} tTransIoCtx;


/* global variables (`siguwi-main.c`) */
extern HINSTANCE gInst;
extern wchar_t * exePath;
extern wchar_t * exeDir;
extern tErrCode lastErr;
extern const wchar_t * const errStr[];
extern wchar_t * const procStateStr[];
extern const uint32_t crc32Table[];


/* string handling (`siguwi-main.c`) */
wchar_t * wFromStr(const char * str);
char * wToUtf8(const wchar_t * str);
void wRemoveCr(wchar_t * str);
wchar_t * wFileName(wchar_t * path);
void wToBackslash(wchar_t * path);
bool wToFullPath(wchar_t ** path, const bool freeOld);
bool wFileExists(const wchar_t * path);
void wStrDelete(wchar_t ** str);
#if !defined(_WSTRING_S_DEFINED) && !defined(_MSC_VER)
errno_t __cdecl wcscat_s(wchar_t * dst, size_t dstSize, const wchar_t * src);
errno_t __cdecl wcscpy_s(wchar_t * dst, size_t dstSize, const wchar_t * src);
#endif /* not _WSTRING_S_DEFINED and not _MSC_VER */
#ifndef NDEBUG
#undef wcsdup
#define wcsdup siguwi_wcsdup
wchar_t * siguwi_wcsdup(const wchar_t * str);
#endif /* not NDEBUG */

/* general utility functions (`siguwi-main.c`) */
uint32_t crc32Update(uint32_t seed, const void * data, const size_t len);
int cmpToken(const tToken * const token, const wchar_t * str);

/* GUI utility functions (`siguwi-main.c`) */
int getDpi(void);
int calcPixels(const int px);
float calcPixelsF(const float px);
int calcFontSize(const int px);
void showFmtMsg(HWND parent, UINT type, const wchar_t * title, const wchar_t * fmt, ...);
void showFmtMsgVar(HWND parent, UINT type, const wchar_t * title, const wchar_t * fmt, va_list ap);
void closeHandlePtr(HANDLE * h, const HANDLE r);

/* configuration window utility functions (`siguwi-config.c`) */
bool fillCertInfo(tConfig * c, HCRYPTKEY hKey);
bool fillContainerInfo(tConfig * c);
void configAdd(tVector * v, tConfig * c);
wchar_t * getCardNameW(SCARDCONTEXT hContext, LPCBYTE atr, const CHAR * ref);
wchar_t * getCspFromCardNameW(const wchar_t * cardName);
tVector * configsGet(void);
int comboConfigIdAdd(const size_t index, tConfig * data, HWND * hCombo);
int configPrint(const size_t index, tConfig * data, tUStrBuf * sb);
int configDelete(const size_t index, tConfig * data, void * param);
void configsDelete(tVector * v);
void configsWndResize(const tConfigWndCtx * ctx);
LRESULT CALLBACK configsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* INI configuration utility functions (`siguwi-ini.c`) */
bool iniConfigParse(const wchar_t * file, const wchar_t * section, tIniConfig * c, tFilePos * p);
bool iniConfigGetCardStatus(const tIniConfigBase * c, DWORD * cardStatus);
bool iniConfigValidatePin(const wchar_t * certProv, const wchar_t * certId, const wchar_t * pin, DWORD len);
bool iniConfigGetPin(const tIniConfigBase * c, HWND parent, DATA_BLOB * pin);
tRcIniConfigBase * rcIniConfigBaseCreate(const tIniConfigBase * c);
tRcIniConfigBase * rcIniConfigBaseClone(tRcIniConfigBase * c);
int rcIniConfigBaseCmp(const tRcIniConfigBase * lhs, const tRcIniConfigBase * rhs);
size_t rcIniConfigBaseHash(const tRcIniConfigBase * key, const size_t limit);
void rcIniConfigBaseDelete(tRcIniConfigBase * c);

/* shell context menu integration via registry utility functions (`siguwi-registry.c`) */
bool regRunningAsAdmin();
bool regIsValidVerb(const wchar_t * str);
void regCloseKeyPtr(HKEY * hKey);
bool regRegister(const wchar_t * configUrl, const wchar_t * configGroup, const wchar_t * ext, const wchar_t * verb, const wchar_t * text, const bool useHklm);
bool regUnregister(const wchar_t * ext, const wchar_t * verb, const bool useHklm);

/* process window utility functions (`siguwi-process.c`) */
int pinBlobDelete(const tRcIniConfigBase * key, DATA_BLOB * data, void * param);
int procCtxDelete(const size_t index, tProcCtx * data, void * param);
bool ipcSendReqToServer(HANDLE hPipe, const tIniConfig * c, int argc, wchar_t ** argv);
bool ipcListen(tIpcWndCtx * ctx);
bool ipcIsValidProcess(HANDLE hPipe);
bool ipcReadAsync(tIpcWndCtx * ctx);
void CALLBACK ipcHandleReadComplete(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
bool processStart(tIpcWndCtx * ctx);
bool processNext(tIpcWndCtx * ctx);
bool processReadAsync(tIpcWndCtx * ctx);
void CALLBACK processHandleReadComplete(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
bool processFinish(tIpcWndCtx * ctx);
bool processAddFile(tIpcWndCtx * ctx, tRcIniConfigBase * c, tRcWStr * signApp, const wchar_t * path);
bool processAddItem(const tIpcWndCtx * ctx, const tProcCtx * item);
void processDragFile(tIpcWndCtx * ctx, HDROP hDrop, UINT i, wchar_t * buf, size_t len);
bool processUpdateItem(const tIpcWndCtx * ctx, const size_t i);
void processWndResize(const tIpcWndCtx * ctx);
LRESULT CALLBACK processSepWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK processWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* command-line option handlers (`siguwi-main.c`) */
void showHelp(void);
void showVersion(void);
/* `siguwi-config.c` */
int showConfigs(int cmdshow);
/* `siguwi-process.c` */
int showProcess(const tIniConfig * c, int cmdshow, int argc, wchar_t ** argv);
/* `siguwi-registry.c` */
int modRegistry(const bool reg, const wchar_t * configUrl, const wchar_t * configGroup, wchar_t * regEntry);
/* `siguwi-translate.c` */
int translateIo();


#endif /* __SIGUWI_H__ */
