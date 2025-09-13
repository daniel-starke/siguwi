/**
 * @file siguwi-registry.c
 * @author Daniel Starke
 * @date 2025-08-21
 * @version 2025-09-12
 */
#include "siguwi.h"


/**
 * Checks whether the current process runs with admin rights.
 *
 * @return `true` if running with admin rights, else `false`
 */
bool regRunningAsAdmin() {
	bool res = false;
	HANDLE hToken = NULL;
	if ( OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) ) {
		TOKEN_ELEVATION e;
		DWORD size;
		if ( GetTokenInformation(hToken, TokenElevation, &e, sizeof(e), &size) ) {
			res = (bool)(e.TokenIsElevated);
		}
		CloseHandle(hToken);
	}
	return res;
}


/**
 * Checks the syntax of the given static shell context menu item verb string.
 *
 * @param[in] str - string to check
 * @return `true` if valid, else `false`
 */
bool regIsValidVerb(const wchar_t * str) {
	static const wchar_t * reserved[] = {
		L"0",
		L"open",
		L"edit",
		L"explore",
		L"find",
		L"new",
		L"play",
		L"preview",
		L"print",
		L"printto",
		L"properties",
		L"runas",
		L"runasuser"
	};
	if (str == NULL || ( ! iswalpha(*str) )) {
		return false;
	}
	for (size_t i = 0; i < ARRAY_SIZE(reserved); ++i) {
		if (_wcsicmp(str, reserved[i]) == 0) {
			return false;
		}
	}
	const wchar_t * start = str;
	++str;
	while (*str) {
		if ( ! iswalnum(*str) ) {
			return false;
		}
		++str;
	}
	return (str - start) <= MAX_REG_KEY_NAME;
}


/**
 * Closes the given registry key and resets the variable to `NULL`.
 * 
 * @param[in,out] hKey - pointer to registry key
 */
void regCloseKeyPtr(HKEY * hKey) {
	if (hKey != NULL && *hKey != NULL) {
		RegCloseKey(*hKey);
		*hKey = NULL;
	}
}


/**
 * Registers the verb for the given file extension in the registry.
 *
 * @param[in] configUrl - INI file path
 * @param[in] configGroup - INI section name
 * @param[in] ext - file extension
 * @param[in] verb - static context menu verb
 * @param[in] text - display string for the context menu entry
 * @param[in] useHklm - `true` for local machine, else `false` for current user
 * @return `true` on success, else `false`
 */
bool regRegister(const wchar_t * configUrl, const wchar_t * configGroup, const wchar_t * ext, const wchar_t * verb, const wchar_t * text, const bool useHklm) {
	const HKEY hRoot = useHklm ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	wchar_t path[2 * MAX_REG_KEY_NAME];
	wchar_t val[MAX_REG_KEY_NAME + 1];
	wchar_t * cmd = NULL;
	tUStrBuf * sb = NULL;
	DWORD valLen;
	HKEY hKey = NULL;
	bool res = false;
	/* ensure the extension is present in the registry */
	snwprintf(path, ARRAY_SIZE(path), L"SOFTWARE\\Classes\\%s", ext);
	if (RegCreateKeyExW(hRoot, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
		goto onError;
	}
	/* check the default value for a possible ProgID redirection */
	ZeroMemory(val, sizeof(val));
	valLen = (DWORD)sizeof(val);
	switch (RegGetValueW(HKEY_CLASSES_ROOT, ext, NULL, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, val, &valLen)) {
	case ERROR_SUCCESS:
		if (*val != 0) {
			/* use redirected path */
			regCloseKeyPtr(&hKey);
			snwprintf(path, ARRAY_SIZE(path), L"SOFTWARE\\Classes\\%s", val);
			if (RegCreateKeyExW(hRoot, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
				goto onError;
			}
		}
		break;
	case ERROR_FILE_NOT_FOUND:
		break;
	default:
		goto onError;
	}
	/* add/replace verb */
	regCloseKeyPtr(&hKey);
	wcscat_s(path, ARRAY_SIZE(path), L"\\shell\\");
	wcscat_s(path, ARRAY_SIZE(path), verb);
	RegDeleteTreeW(hRoot, path);
	if (RegCreateKeyExW(hRoot, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
		goto onError;
	}
	/* set meta data */
	if (RegSetValueExW(hKey, L"NeverDefault", 0, REG_SZ, NULL, 0) != ERROR_SUCCESS) {
		goto onError;
	}
	if (RegSetValueExW(hKey, L"Icon", 0, REG_SZ, (LPCBYTE)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
		goto onError;
	}
	if (RegSetValueExW(hKey, L"MUIVerb", 0, REG_SZ, (LPCBYTE)text, (DWORD)((wcslen(text) + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
		goto onError;
	}
	/* add command */
	regCloseKeyPtr(&hKey);
	wcscat_s(path, ARRAY_SIZE(path), L"\\command");
	if (RegCreateKeyExW(hRoot, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
		goto onError;
	}
	/* build and set command-line */
	sb = usb_create(MAX_CONFIG_STR_LEN);
	if (sb == NULL) {
		goto onError;
	}
	if (usb_addFmt(sb, L"\"%s\" -c \"%s:%s\" \"%%1\"", exePath, configUrl, configGroup) <= 0) {
		goto onError;
	}
	cmd = usb_get(sb);
	if (cmd == NULL) {
		goto onError;
	}
	if (RegSetValueExW(hKey, NULL, 0, REG_SZ, (LPCBYTE)cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
		goto onError;
	}
	res = true;
onError:
	if (cmd != NULL) {
		free(cmd);
	}
	if (sb != NULL) {
		usb_delete(sb);
	}
	regCloseKeyPtr(&hKey);
	return res;
}


/**
 * Unregisters the verb from the given context menu entry in the registry.
 *
 * @param[in] ext - file extension
 * @param[in] verb - static context menu verb
 * @param[in] useHklm - `true` for local machine, else `false` for current user
 * @return `true` on success, else `false`
 */
bool regUnregister(const wchar_t * ext, const wchar_t * verb, const bool useHklm) {
	const HKEY hRoot = useHklm ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	wchar_t path[2 * MAX_REG_KEY_NAME];
	wchar_t val[MAX_REG_KEY_NAME + 1];
	DWORD valLen;
	HKEY hKey = NULL;
	bool res = false;
	/* check if extension path exists */
	snwprintf(path, ARRAY_SIZE(path), L"SOFTWARE\\Classes\\%s", ext);
	if (RegCreateKeyExW(hRoot, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, NULL) != ERROR_SUCCESS) {
		goto onSuccess;
	}
	/* check the default value for a possible ProgID redirection */
	ZeroMemory(val, sizeof(val));
	valLen = (DWORD)sizeof(val);
	switch (RegGetValueW(HKEY_CLASSES_ROOT, ext, NULL, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, val, &valLen)) {
	case ERROR_SUCCESS:
		if (*val != 0) {
			/* use redirected path (do not create one) */
			regCloseKeyPtr(&hKey);
			snwprintf(path, ARRAY_SIZE(path), L"SOFTWARE\\Classes\\%s", val);
			if (RegCreateKeyExW(hRoot, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, NULL) != ERROR_SUCCESS) {
				goto onError;
			}
		}
		break;
	case ERROR_FILE_NOT_FOUND:
		break;
	default:
		goto onError;
	}
	/* delete verb */
	regCloseKeyPtr(&hKey);
	wcscat_s(path, ARRAY_SIZE(path), L"\\shell\\");
	wcscat_s(path, ARRAY_SIZE(path), verb);
	switch (RegDeleteTreeW(hRoot, path)) {
	case ERROR_SUCCESS:
	case ERROR_FILE_NOT_FOUND:
		break;
	default:
		goto onError;
	}
onSuccess:
	res = true;
onError:
	regCloseKeyPtr(&hKey);
	return res;
}


/**
 * Modifies the context menu entry in the Windows registry according to the
 * given parameters.
 *
 * @param[in] reg - `true` to register, `false` to unregister
 * @param[in] configUrl - INI file path
 * @param[in] configGroup - INI section name
 * @param[in] regEntry - registry with static verb with optional text
 * @return program exit code
 */
int modRegistry(const bool reg, const wchar_t * configUrl, const wchar_t * configGroup, wchar_t * regEntry) {
	static const wchar_t * exts[] = {
		L".exe",
		L".dll",
		L".ps1"
	};
	if (configUrl == NULL || regEntry == NULL) {
		MessageBoxW(NULL, errStr[ERR_INVALID_ARG], L"Error (modRegistry)", MB_OK | MB_ICONERROR);
		return 1;
	}
	const wchar_t * regVerb = regEntry;
	bool res = true;
	bool useHklm = regRunningAsAdmin();
	if ( reg ) {
		/* register */
		/* split registry entry string */
		wchar_t * regText = wcsrchr(regEntry, L':');
		if (regText != NULL) {
			*regText++ = 0;
		} else {
			regText = DEFAULT_REG_TEXT;
		}
		if ( ! regIsValidVerb(regVerb) ) {
			showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (modRegistry)", errStr[ERR_INVALID_REG_VERB], regVerb);
			return 1;
		}
		for (size_t i = 0; i < ARRAY_SIZE(exts); ++i) {
			/* register until error */
			res = res && regRegister(configUrl, configGroup, exts[i], regVerb, regText, useHklm);
		}
		if ( ! res ) {
			/* roll back on error */
			for (size_t i = 0; i < ARRAY_SIZE(exts); ++i) {
				regUnregister(exts[i], regVerb, useHklm);
			}
		}
	} else {
		/* unregister */
		if ( ! regIsValidVerb(regVerb) ) {
			showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (modRegistry)", errStr[ERR_INVALID_REG_VERB], regVerb);
			return 1;
		}
		for (size_t i = 0; i < ARRAY_SIZE(exts); ++i) {
			/* unregister all even on single error */
			res = regUnregister(exts[i], regVerb, useHklm) && res;
		}
	}
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
	if ( ! res ) {
		MessageBoxW(NULL, L"Failed to perform the requested operation.", L"Error (modRegistry)", MB_OK | MB_ICONERROR);
	}
	return res ? 0 : 1;
}
