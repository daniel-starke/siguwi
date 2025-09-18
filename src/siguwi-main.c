/**
 * @file siguwi-main.c
 * @author Daniel Starke
 * @date 2025-06-14
 * @version 2025-09-17
 */
#include "siguwi.h"


/**
 * Global application instance handle.
 */
HINSTANCE gInst = NULL;


/**
 * File path of this executable.
 */
wchar_t * exePath = NULL;


/**
 * Directory path of this executable with trailing backslash.
 */
wchar_t * exeDir = NULL;


/**
 * Last internal error code.
 *
 * @remarks This is only for the main thread.
 */
tErrCode lastErr = ERR_SUCCESS;


/**
 * Maps error codes to strings.
 */
const wchar_t * const errStr[] = {
	/* ERR_SUCCESS */          L"The operation completed successfully.",
	/* ERR_UNKNOWN */          L"An unknown error occurred.",
	/* ERR_INVALID_ARG */      L"An invalid argument has been given.",
	/* ERR_OUT_OF_MEMORY */    L"Failed to allocate memory.",
	/* ERR_OPT_NO_ARG */       L"Option argument is missing for '%s'.",
	/* ERR_OPT_AMB_C */        L"Unknown or ambiguous option '-%c'.",
	/* ERR_OPT_AMB_S */        L"Unknown or ambiguous option '%s'.",
	/* ERR_OPT_AMB_X */        L"Unknown option character '0x%02X'.",
	/* ERR_PRINTF_FMT */       L"Message format string syntax error.",
	/* ERR_CREATEFONT */       L"CreateFont failed.",
	/* ERR_NO_SMARTCARD */     L"No SmartCard was found.",
	/* ERR_GET_STATUS */       L"Failed to get SmartCard status.",
	/* ERR_GET_CSP */          L"Failed to get the CSP name from SmartCard name.",
	/* ERR_CREATE_FILE */      L"Failed to create file.",
	/* ERR_READ_FILE */        L"Failed to read file.",
	/* ERR_GET_EXE_PATH */     L"Failed to retrieve own executable path.",
	/* ERR_REL_CONFIG_PATH */  L"Configuration and executable file need to share the same\nparent directory for security considerations.\n\n%s\n%s",
	/* ERR_SYNTAX_ERROR */     L"Invalid syntax.",
	/* ERR_LARGE_CONFIG */     L"Given configuration file is too large.",
	/* ERR_MISSING_FIELD */    L"%s: Missing configuration field \"%s\" in section \"%s\".",
	/* ERR_CREATE_PIPE */      L"Failed to create pipe (0x%08X).",
	/* ERR_OPEN_NAMED_PIPE */  L"Failed to open named pipe (0x%08X).",
	/* ERR_WRITE_NAMED_PIPE */ L"Failed to write to named pipe (0x%08X).",
	/* ERR_ASYNC_LISTEN */     L"Failed to asynchronously listen for clients (0x%08X).",
	/* ERR_ASYNC_READ */       L"Failed to asynchronously read data (0x%08X).",
	/* ERR_CREATE_EVENT */     L"Failed to create asynchronous event (0x%08X).",
	/* ERR_GET_STD_HANDLE */   L"Failed to get standard I/O handle.",
	/* ERR_INVALID_REG_VERB */ L"Invalid static shell context menu item verb string \"%s\" given."
};


/**
 * Maps process states to strings.
 */
wchar_t * const procStateStr[] = {
	/* PST_IDLE */              L"pending",
	/* PST_RUNNING */           L"running",
	/* PST_OK */                L"success",
	/* PST_FAIL */              L"failed",
	/* PST_FILE_NOT_FOUND */    L"file not found",
	/* PST_BROKEN_PIPE */       L"broken pipe",
	/* PST_APP_NOT_FOUND */     L"app not found",
	/* PST_PIN_MISSING */       L"pin missing",
	/* PST_PIN_WRONG */         L"pin wrong"
};


/**
 * Look-up table for CRC32 hashing.
 */
const uint32_t crc32Table[] = {
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
	0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
	0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
	0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
	0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};


/**
 * Main entry point.
 *
 * @param[in] hInst - handle to the current application instance
 * @param[in] hInstPrev - always `NULL`
 * @param[in] cmdline - command line for the application excluding the program name
 * @param[in] cmdshow - `ShowWindow` parameter
 * @return exit code
 */
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE hInstPrev, LPWSTR cmdline, int cmdshow) {
	PCF_UNUSED(hInstPrev);
	PCF_UNUSED(cmdline);
	wchar_t ** argv, ** enpv, * configUrl, * oldConfigUrl, * regEntry;
	wchar_t configPath[MAX_PATH];
	wchar_t * configGroup;
	tRegMode regMode;
	int argc, si = 0;
	if (__wgetmainargs(&argc, &argv, &enpv, 1 /* enable globbing */, &si) != 0) {
		return EXIT_FAILURE;
	}
	static const struct option longOptions[] = {
		{L"config",     required_argument, NULL, L'c'},
		{L"help",       no_argument,       NULL, L'h'},
		{L"list",       no_argument,       NULL, L'l'},
		{L"register",   required_argument, NULL, L'r'},
		{L"translate",  no_argument,       NULL, L't'},
		{L"unregister", required_argument, NULL, L'u'},
		{L"version",    no_argument,       NULL, L'v'},
		{NULL, 0, NULL, 0}
	};
	gInst = hInst;

	/* ensure that the environment does not change the argument parser behavior */
	_wputenv(L"POSIXLY_CORRECT=");
	/* ensure use of the correct locale */
	_wputenv(L"LANG=en_US.UTF-8");
	_wputenv(L"LC_ALL=en_US.UTF-8");
	_wputenv(L"LC_CTYPE=en_US.UTF-8");
	_wputenv(L"PYTHONIOENCODING=utf-8");
	_wputenv(L"PYTHONUTF8=1");
	_wputenv(L"DOTNET_CLI_UI_LANGUAGE=en");
	_wputenv(L"DOTNET_CLI_FORCE_UTF8_ENCODING=1");
	_wputenv(L"VSLANG=1033");
	_wputenv(L"RUBYOPT=-EUTF-8");
	_wputenv(L"JAVA_TOOL_OPTIONS=-Dfile.encoding=UTF-8 -Dsun.jnu.encoding=UTF-8");
	SetProcessPreferredUILanguages(MUI_LANGUAGE_NAME, L"en-US\0", NULL);

	if (argc <= 1) {
		return showConfigs(cmdshow);
	}

	configUrl = NULL;
	configGroup = NULL;
	oldConfigUrl = NULL;
	regEntry = NULL;
	regMode = RM_NONE;
	while (1) {
		const int res = getopt_long(argc, argv, L":c:hlvr:tu:", longOptions, NULL);
		if (res == -1) break;
		switch (res) {
		case L'c':
			configUrl = optarg;
			break;
		case L'h':
			showHelp();
			return EXIT_SUCCESS;
		case L'l':
			return showConfigs(cmdshow);
		case L'r':
			regMode = RM_REGISTER;
			regEntry = optarg;
			break;
		case L't':
			return translateIo();
			break;
		case L'u':
			regMode = RM_UNREGISTER;
			regEntry = optarg;
			break;
		case L'v':
			showVersion();
			return EXIT_SUCCESS;
		case L':':
			showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (command-line)", errStr[ERR_OPT_NO_ARG], argv[optind - 1]);
			return EXIT_FAILURE;
		case L'?':
			if (iswprint((wint_t)optopt) != 0) {
				showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (command-line)", errStr[ERR_OPT_AMB_C], optopt);
			} else if (optopt == 0) {
				showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (command-line)", errStr[ERR_OPT_AMB_S], argv[optind - 1]);
			} else {
				showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (command-line)", errStr[ERR_OPT_AMB_X], (int)optopt);
			}
			return EXIT_FAILURE;
		default:
			abort();
		}
	}
	int res = EXIT_FAILURE;

	/* get executable directory */
	{
		const DWORD exePathLen = GetModuleFileNameW(gInst, configPath, ARRAY_SIZE(configPath));
		if (exePathLen == 0) {
			MessageBoxW(NULL, errStr[ERR_GET_EXE_PATH], L"Error (command-line)", MB_OK | MB_ICONERROR);
			return EXIT_FAILURE;
		}
		wToBackslash(configPath);
		exePath = wcsdup(configPath);
		if (exePath == NULL) {
			MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (command-line)", MB_OK | MB_ICONERROR);
			goto onError;
		}
		/* strip file name */
		wchar_t * tail = wcsrchr(configPath, L'\\');
		if (tail == NULL) {
			tail = wcsrchr(configPath, L'/');
		}
		if (tail == NULL) {
			tail = configPath;
			*tail = 0;
		} else {
			tail[1] = 0;
		}
		exeDir = wcsdup(configPath);
		if (exeDir == NULL) {
			MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (command-line)", MB_OK | MB_ICONERROR);
			goto onError;
		}
	}

	/* resolve configuration file / section */
	if (configUrl == NULL) {
		/* assume default configuration URL to be `siguwi.ini` next to this executable */
		configPath[0] = 0;
		wcscat_s(configPath, ARRAY_SIZE(configPath), exeDir);
		wcscat_s(configPath, ARRAY_SIZE(configPath), L"siguwi.ini");
		configUrl = configPath;
		/* default config group */
		configGroup = DEFAULT_CONFIG_GROUP;
	} else {
		configGroup = wcsrchr(configUrl, L':');
		if (configGroup == NULL) {
			configGroup = DEFAULT_CONFIG_GROUP;
		} else {
			*configGroup++ = 0;
		}
	}
	oldConfigUrl = configUrl;
	if ( ! wToFullPath(&configUrl, false) ) {
		MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (command-line)", MB_OK | MB_ICONERROR);
		goto onError;
	}
	if (_wcsnicmp(configUrl, exeDir, wcslen(exeDir)) != 0) {
		showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (command-line)", errStr[ERR_REL_CONFIG_PATH], exeDir, configUrl);
		goto onError;
	}

	/* un-/register context menu entry in registry */
	switch (regMode) {
	case RM_NONE:
		break;
	case RM_REGISTER:
	case RM_UNREGISTER:
		res = modRegistry(regMode == RM_REGISTER, configUrl, configGroup, regEntry);
		goto onError;
	}

	/* load configuration file */
	tIniConfig config;
	tFilePos errPos;
	ZeroMemory(&config, sizeof(config));
	ZeroMemory(&errPos, sizeof(errPos));
	if ( ! iniConfigParse(configUrl, configGroup, &config, &errPos) ) {
		if (lastErr == ERR_SYNTAX_ERROR) {
			showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (INI file)", L"%s:%zu:%zu: %s", configUrl, errPos.row, errPos.col, errStr[lastErr]);
		} else {
			showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (INI file)", L"%s: %s", configUrl, errStr[lastErr]);
		}
		goto onError;
	}
	/* check configuration file consistency */
	const struct {
		const wchar_t * name;
		const wchar_t * ptr;
	} fields[] = {
		{L"certId",     config.cert->certId},
		{L"cardName",   config.cert->cardName},
		{L"cardReader", config.cert->cardReader},
		{L"signApp",    config.signApp->ptr}
	};
	for (size_t i = 0; i < ARRAY_SIZE(fields); ++i) {
		if (fields[i].ptr == NULL) {
			showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (INI file)", errStr[ERR_MISSING_FIELD], configUrl, fields[i].name, configGroup);
			goto onError;
		}
	}
	/* deduce cryptographic service provider */
	config.cert->certProv = getCspFromCardNameW(config.cert->cardName);
	if (config.cert->certProv == NULL) {
		MessageBoxW(NULL, errStr[ERR_GET_CSP], L"Error (showProcess)", MB_OK | MB_ICONERROR);
		goto onError;
	}
	/* process given file list */
	res = showProcess(&config, cmdshow, argc - optind, argv + optind);
onError:
	wStrDelete(&(config.cert->certProv));
	wStrDelete(&(config.cert->certId));
	wStrDelete(&(config.cert->cardName));
	wStrDelete(&(config.cert->cardReader));
	rws_release(&(config.signApp));
	if (oldConfigUrl != configUrl) {
		wStrDelete(&configUrl);
	}
	wStrDelete(&exeDir);
	wStrDelete(&exePath);
	return res;
}


/**
 * Returns the wide-character string from the given ANSI character string.
 *
 * @param[in] str - character string
 * @return wide-character string or `NULL` on error
 */
wchar_t * wFromStr(const char * str) {
	if (str == NULL) {
		return NULL;
	}
	const size_t len = strlen(str) + 1;
	wchar_t * res = malloc(len * sizeof(wchar_t));
	if (res == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < len; ++i) {
		res[i] = (wchar_t)(str[i]);
	}
	return res;
}


/**
 * Returns the UTF-8 string from the given wide-character string.
 *
 * @param[in] str - wide-character string
 * @return UTF-8 string
 */
char * wToUtf8(const wchar_t * str) {
	if (str == NULL) {
		return NULL;
	}
	char * res = NULL;
	int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	if (len <= 0) {
		goto onError;
	}
	res = malloc((size_t)len);
	if (res == NULL) {
		goto onError;
	}
	len = WideCharToMultiByte(CP_UTF8, 0, str, -1, res, len, NULL, NULL);
	if (len <= 0) {
		goto onError;
	}
	return res;
onError:
	if (res != NULL) {
		/* ensure that possibly sensitive data is handled properly */
		SecureZeroMemory(res, strlen(res));
		free(res);
	}
	return NULL;
}


/**
 * Removes carrier-returns in the given string in-place.
 *
 * @param[in,out] str - wide-character string
 */
void wRemoveCr(wchar_t * str) {
	if (str == NULL) {
		return;
	}
	wchar_t * ptr = str;
	for (; *str; ++str) {
		if (*str != L'\r') {
			*ptr++ = *str;
		}
	}
	*ptr = 0;
}


/**
 * Returns the file name part of the path. The returned value point into the
 * given path string.
 *
 * @param[in] path - input path
 * @return file name part including file extension
 */
wchar_t * wFileName(wchar_t * path) {
	if (path == NULL) {
		return NULL;
	}
	for (wchar_t * ptr = path; *ptr != 0; ++ptr) {
		switch (*ptr) {
		case L'/':
		case L'\\':
			path = ptr + 1;
			break;
		default:
			break;
		}
	}
	return path;
}


/**
 * Converts all slashes to backslashes in the given path.
 *
 * @param[in,out] path - pointer to path string
 */
void wToBackslash(wchar_t * path) {
	if (path == NULL) {
		return;
	}
	while ( *path ) {
		if (*path == L'/') {
			*path = L'\\';
		}
		++path;
	}
}


/**
 * Replaces the given path string with an absolute path if possible.
 *
 * @param[in,out] path - pointer to path string
 * @param[in] freeOld - `true` to free old path before setting it, else `false`
 * @return `true` on success, else `false`
 */
bool wToFullPath(wchar_t ** path, const bool freeOld) {
	if (path == NULL || *path == NULL) {
		return false;
	}
	wchar_t buf[MAX_PATH + 1];
	ZeroMemory(buf, sizeof(buf));
	const DWORD res = GetFullPathNameW(*path, MAX_PATH + 1, buf, NULL);
	if (res == 0) {
		return false;
	}
	if (res <= MAX_PATH) {
		buf[res] = 0;
		wchar_t * str = wcsdup(buf);
		if (str == NULL) {
			return false;
		}
		if ( freeOld ) {
			free(*path);
		}
		*path = str;
	} else {
		wchar_t * str = malloc(((size_t)res + 1) * sizeof(wchar_t));
		if (str == NULL) {
			return false;
		}
		ZeroMemory(str, ((size_t)res + 1) * sizeof(wchar_t));
		const DWORD res2 = GetFullPathNameW(*path, res + 1, str, NULL);
		if (res2 == 0 || res2 > res) {
			free(str);
			return false;
		}
		if ( freeOld ) {
			free(*path);
		}
		str[res2] = 0;
		*path = str;
	}
	wToBackslash(*path);
	return true;
}


/**
 * Checks if the given path is an existing file.
 *
 * @param[in] path - path to check
 * @return `true` if existing file path, else `false`
 */
bool wFileExists(const wchar_t * path) {
	const DWORD attr = GetFileAttributesW(path);
	return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}


/**
 * Deletes the given wide-character string if set and resets it.
 *
 * @param[in,out] str - pointer to wide-character string
 */
void wStrDelete(wchar_t ** str) {
	if (str != NULL && *str != NULL) {
		free(*str);
		*str = NULL;
	}
}


#if !defined(_WSTRING_S_DEFINED) && !defined(_MSC_VER)
errno_t __cdecl wcscat_s(wchar_t * dst, size_t dstSize, const wchar_t * src) {
	if (dst == NULL || src == NULL || dstSize == 0) {
		return EINVAL;
	}
	const size_t dstLen = wcslen(dst);
	const size_t srcLen = wcslen(src);
	if ((dstLen + srcLen + 1) > dstSize) {
		if (dstLen < dstSize) {
			*dst = 0;
		}
		return ERANGE;
	}
	wcscpy(dst + dstLen, src);
	return 0;
}


errno_t __cdecl wcscpy_s(wchar_t * dst, size_t dstSize, const wchar_t * src) {
	if (dst == NULL || src == NULL || dstSize == 0) {
		return EINVAL;
	}
	size_t srcLen = wcslen(src);
	if ((srcLen + 1) > dstSize) {
		*dst = 0;
		return ERANGE;
	}
	wcscpy(dst, src);
	return 0;
}
#endif /* not _WSTRING_S_DEFINED and not _MSC_VER */


#ifndef NDEBUG
/**
 * Own implementation of `wcsdup()` to allow better debugging with `Dr.Memory`.
 *
 * @param[in] str - null-terminated wide-character string to duplicate
 * @return duplicated string or `NULL` on allocation error
 */
wchar_t * siguwi_wcsdup(const wchar_t * str) {
	size_t len = (wcslen(str) + 1) * sizeof(wchar_t);
	wchar_t * res = malloc(len);
	if (res == NULL) {
		return NULL;
	}
	memcpy(res, str, len);
	return res;
}
#endif /* not NDEBUG */


/**
 * Hashes the given data with the passed seed.
 * The updated seed is returned.
 *
 * @param[in] seed - seed to update
 * @param[in] data - data pointer
 * @param[in] len - bytes to hash
 * @return updated seed
 */
inline uint32_t crc32Update(uint32_t seed, const void * data, const size_t len) {
	const uint8_t * ptr = data;
	for (size_t i = 0; i < len; ++i) {
		seed = crc32Table[(*ptr ^ seed) & 0xFF] ^ (seed >> 8);
		++ptr;
	}
	return seed;
}


/**
 * Compares the given token with a passed string. Both are compared case sensitive. The token needs
 * to match the passed string exactly and completely to return 0.
 *
 * @param[in] token - token to compare
 * @param[in] str - compare with this string
 * @return same as strcmp
 */
int cmpToken(const tToken * const token, const wchar_t * str) {
	if (token == NULL || token->ptr == NULL || str == NULL) {
		lastErr = ERR_INVALID_ARG;
		return INT_MAX;
	}
	const unsigned short * left = (const unsigned short *)(token->ptr);
	const unsigned short * right = (const unsigned short *)str;
	size_t len = token->len;
	for (;len > 0 && *right != 0 && *left == *right; --len, ++left, ++right);
	if (len > 0 && *right == 0) {
		return *left;
	} else if (len == 0 && *right != 0) {
		return -(*right);
	} else if (len == 0 && *right == 0) {
		return 0;
	}
	return *left - *right;
}


/**
 * Returns the current screen DPI.
 *
 * @return screen DPI
 */
int getDpi(void) {
	HDC hDc = GetDC(NULL);
	const int dpi = GetDeviceCaps(hDc, LOGPIXELSY);
	ReleaseDC(NULL, hDc);
	return dpi;
}


/**
 * Calculates the font size for the given target font size in pixels.
 *
 * @param[in] px - target font size in pixel * 10
 * @param[in] dpi - dots per inch of the target surface
 * @return font size
 */
int calcFontSize(const int px, const int dpi) {
	return -MulDiv(px, dpi, 720);
}


/**
 * Shows the given formated string as modal window.
 *
 * @param[in] parent - parent window handle
 * @param[in] type - display flags
 * @param[in] title - window title
 * @param[in] fmt - format string
 * @param[in] ... - format arguments
 */
void showFmtMsg(HWND parent, UINT type, const wchar_t * title, const wchar_t * fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	showFmtMsgVar(parent, type, title, fmt, ap);
	va_end(ap);
}


/**
 * Shows the given formated string as modal window.
 *
 * @param[in] parent - parent window handle
 * @param[in] type - display flags
 * @param[in] title - window title
 * @param[in] fmt - format string
 * @param[in] ap - variable argument list
 */
void showFmtMsgVar(HWND parent, UINT type, const wchar_t * title, const wchar_t * fmt, va_list ap) {
	wchar_t preBuf[256];
	wchar_t * buf = preBuf;
	const int len = vswprintf(preBuf, ARRAY_SIZE(preBuf), fmt, ap);
	if (len < 0) {
		MessageBoxW(parent, errStr[ERR_PRINTF_FMT], L"Error (showFmtMsgVar)", MB_OK | MB_ICONERROR);
		return;
	}
	if ((size_t)len >= ARRAY_SIZE(preBuf)) {
		buf = malloc((size_t)(len + 1) * sizeof(wchar_t));
		if (buf == NULL) {
			MessageBoxW(parent, errStr[ERR_OUT_OF_MEMORY], L"Error (showFmtMsgVar)", MB_OK | MB_ICONERROR);
			return;
		}
		vswprintf(buf, (size_t)(len + 1), fmt, ap);
	}
	MessageBoxW(parent, buf, title, type);
	if (buf != preBuf) {
		free(buf);
	}
}


/**
 * Closes the given handle and resets the variable to `r`.
 *
 * @param[in,out] h - pointer to Windows handle
 * @param[in] r - HANDLE value in unset state to reset to
 */
void closeHandlePtr(HANDLE * h, const HANDLE r) {
	if (h != NULL && *h != r) {
		CloseHandle(*h);
		*h = r;
	}
}


/**
 * Show the help for this application as modal window.
 */
void showHelp(void) {
	wchar_t buf[1024];
	snwprintf(buf, ARRAY_SIZE(buf),
		L"siguwi [-c file[:section]] [--] [files ...]\n"
		L"siguwi [-c file[:section]] -r verb[:text]\n"
		L"siguwi [-c file[:section]] -u verb\n"
		L"siguwi [-hltv]\n"
		"\n"
		"-c, --config file[:section]\n"
		"\tSpecify the configuration file. Can be following\n"
		"\tby a section name if separated by a colon (':').\n"
		"-l, --list\n"
		"\tList possible configurations.\n"
		"-h, --help\n"
		"\tShow short usage instruction.\n"
		"-r, --register verb[:text]\n"
		"\tAdd a shell context menu entry via registry for:\n"
		"\t- executable files (.exe)\n"
		"\t- shared libraries (.dll)\n"
		"\t- PowerShell scripts (.ps1)\n"
		"\tSpecify the unique registry verb and an optional menu\n"
		"\tstring separated by a colon (':').\n"
		"-t, --translate\n"
		"\tTranslate standard input data from ACP to UTF-8.\n"
		"-u, --unregister verb\n"
		"\tRemove the shell context menu entry with the given\n"
		"\tverb from the registry.\n"
		"-v, --version\n"
		"\tShow the program version.\n"
		"\n"
		"siguwi " SIGUWI_VERSION "\n"
		"https://github.com/daniel-starke/siguwi\n"
	);
	MessageBoxW(NULL, buf, L"Help", MB_OK | MB_ICONINFORMATION);
}


/**
 * Show the help for this application as modal window.
 */
void showVersion(void) {
	MessageBoxW(
		NULL,
		L"siguwi " SIGUWI_VERSION "\n"
		"\n"
		"Copyright (C) 2025 " SIGUWI_AUTHOR,
		L"siguwi",
		MB_OK | MB_ICONINFORMATION
	);
}
