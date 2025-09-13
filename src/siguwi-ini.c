/**
 * @file siguwi-ini.c
 * @author Daniel Starke
 * @date 2025-07-04
 * @version 2025-09-13
 */
#include "siguwi.h"


#ifdef _MSC_VER
typedef struct _KERB_SMARTCARD_CSP_INFO {
	DWORD dwCspInfoLen;
	DWORD MessageType;
	union {
		PVOID ContextInformation;
		ULONG64 SpaceHolderForWow64;
	};
	DWORD flags;
	DWORD KeySpec;
	ULONG nCardNameOffset;
	ULONG nReaderNameOffset;
	ULONG nContainerNameOffset;
	ULONG nCSPNameOffset;
	TCHAR bBuffer;
} KERB_SMARTCARD_CSP_INFO, *PKERB_SMARTCARD_CSP_INFO;
#endif /* _MSC_VER */


/**
 * Parses the passed INI file with the given section and fills the configuration
 * structure.
 *
 * @param[in] file - INI file path
 * @param[in] section - INI section name
 * @param[out] c - INI configuration
 * @param[out] p - optional file position of an parsing error
 * @return `true` on success, else `false`
 * @remarks Sets `lastErr` and `p` on error accordingly.
 */
bool iniConfigParse(const wchar_t * file, const wchar_t * section, tIniConfig * c, tFilePos * p) {
	enum {
		ST_IDLE,
		ST_COMMENT,
		ST_GROUP_START,
		ST_GROUP,
		ST_GROUP_END,
		ST_KEY,
		ST_ASSIGN,
		ST_VALUE_START,
		ST_VALUE,
		ST_VALUE_END
	} state;
	if (file == NULL || section == NULL || c == NULL) {
		lastErr = ERR_INVALID_ARG;
		return false;
	}
	FILE * fp = NULL;
	wchar_t * content = NULL;
	bool res = false;
	tFilePos pos = {1, 1};
	/* open the INI file */
	fp = _wfopen(file, L"rt, ccs=UTF-8");
	if (fp == NULL) {
		lastErr = ERR_READ_FILE;
		goto onError;
	}
	/* calculate the content size */
	if (_fseeki64(fp, 0, SEEK_END) != 0) {
		lastErr = ERR_INVALID_ARG;
		goto onError;
	}
	int64_t size = _ftelli64(fp) + 1;
	rewind(fp);
	if (size < 0) {
		lastErr = ERR_INVALID_ARG;
		goto onError;
	} else if (size > MAX_CONFIG_FILE_LEN) {
		lastErr = ERR_LARGE_CONFIG;
		goto onError;
	}
	/* read the whole file to memory and ensure trailing carrier return */
	content = malloc((size_t)size * sizeof(wchar_t));
	if (content == NULL) {
		lastErr = ERR_OUT_OF_MEMORY;
		goto onError;
	}
	size_t len = 0;
	wint_t wc;
	do {
#ifdef _UCRT
		wc = _getwc_nolock(fp);
#else
		wc = getwc(fp);
#endif
		if (len >= (size_t)size) {
			size *= 2;
			wchar_t * buf = realloc(content, (size_t)size * sizeof(wchar_t));
			if ( ! buf ) {
				lastErr = ERR_OUT_OF_MEMORY;
				goto onError;
			}
			content = buf;
		}
		content[len++] = (wchar_t)((wc != WEOF) ? wc : L'\r');
	} while (wc != WEOF);
	/* parse the content */
	const wchar_t * const ptrEnd = content + len;
	tToken group = {L"", 0};
	tToken key = {NULL, 0};
	tToken value = {NULL, 0};
	wchar_t quote = 0;
	state = ST_IDLE;
#ifdef DEBUG_INI
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
#endif /* DEBUG_INI */
	for (wchar_t * ptr = content; ptr != ptrEnd; ++ptr) {
		const wint_t ch = *ptr;
#ifdef DEBUG_INI
		static const wchar_t * const st[] = {L"ST_IDLE", L"ST_COMMENT", L"ST_GROUP_START", L"ST_GROUP", L"ST_GROUP_END", L"ST_KEY", L"ST_ASSIGN", L"ST_VALUE_START", L"ST_VALUE", L"ST_VALUE_END"};
		fwprintf(stdout, L"%s - %c (%u)\n", st[state], iswprint(ch) ? ch : L' ', ch);
#endif /* DEBUG_INI */
		switch (state) {
		case ST_IDLE:
			switch (ch) {
			case L'#':
			case L';':
				state = ST_COMMENT;
				break;
			case L'[':
				state = ST_GROUP_START;
				break;
			default:
				if ( iswalpha(ch) ) {
					state = ST_KEY;
					key = (tToken){ptr, 1};
				} else if ( ! iswspace(ch) ) {
					lastErr = ERR_SYNTAX_ERROR;
					goto onError;
				}
				break;
			}
			break;
		case ST_COMMENT:
			switch (ch) {
			case L'\n':
			case L'\r':
				state = ST_IDLE;
				break;
			default:
				break;
			}
			break;
		case ST_GROUP_START:
			if (ch == L']') {
				state = ST_IDLE;
				group = (tToken){L"", 0};
			} else if ( iswalpha(ch) ) {
				state = ST_GROUP;
				group = (tToken){ptr, 1};
			} else if ( ! iswblank(ch) ) {
				lastErr = ERR_SYNTAX_ERROR;
				goto onError;
			}
			break;
		case ST_GROUP:
			if (ch == L']') {
				state = ST_IDLE;
			} else if ( iswalnum(ch) ) {
				++(group.len);
			} else if ( iswblank(ch) ) {
				state = ST_GROUP_END;
			} else {
				lastErr = ERR_SYNTAX_ERROR;
				goto onError;
			}
			break;
		case ST_GROUP_END:
			if (ch == L']') {
				state = ST_IDLE;
			} else if ( ! iswblank(ch) ) {
				lastErr = ERR_SYNTAX_ERROR;
				goto onError;
			}
			break;
		case ST_KEY:
			if (ch == L'=') {
				state = ST_VALUE_START;
				quote = 0;
			} else if ( iswalnum(ch) ) {
				++(key.len);
			} else if ( iswblank(ch) ) {
				state = ST_ASSIGN;
			} else {
				lastErr = ERR_SYNTAX_ERROR;
				goto onError;
			}
			break;
		case ST_ASSIGN:
			if (ch == L'=') {
				state = ST_VALUE_START;
				quote = 0;
			} else if ( ! iswblank(ch) ) {
				lastErr = ERR_SYNTAX_ERROR;
				goto onError;
			}
			break;
		case ST_VALUE_START:
			switch (ch) {
			case L'"':
			case L'\'':
				state = ST_VALUE;
				quote = ch;
				value = (tToken){ptr + 1, 0};
				break;
			default:
				if ( ! iswblank(ch) ) {
					state = ST_VALUE;
					value = (tToken){ptr, 1};
				}
				break;
			}
			break;
		case ST_VALUE:
			if (quote != 0) {
				if (ch == quote) {
					state = ST_VALUE_END;
					*ptr = 0; /* make value a null-terminated string */
				} else {
					++(value.len);
				}
			} else {
				switch (ch) {
				case L'\n':
				case L'\r':
					state = ST_VALUE_END;
					break;
				default:
					++(value.len);
					break;
				}
			}
			break;
		case ST_VALUE_END:
			break;
		}
		if (state == ST_VALUE_END) {
			/* completely parsed {key, value} pair */
			state = ST_IDLE;
			if (quote == 0) {
				/* trim trailing blanks */
				wchar_t * it = value.ptr + value.len;
				while (it != value.ptr) {
					--it;
					if ( ! iswblank(*it) ) {
						*(++it) = 0; /* make value a null-terminated string */
						break;
					}
				}
				if (it == value.ptr) {
					*it = 0; /* make value a null-terminated string */
				}
			}
			/* assign configuration string */
			if (cmpToken(&group, section) == 0) {
				wchar_t ** k = NULL;
				if (cmpToken(&key, L"certId") == 0) {
					k = &(c->cert->certId);
				} else if (cmpToken(&key, L"cardName") == 0) {
					k = &(c->cert->cardName);
				} else if (cmpToken(&key, L"cardReader") == 0) {
					k = &(c->cert->cardReader);
				} else if (cmpToken(&key, L"signApp") == 0) {
					rws_release(&(c->signApp));
					c->signApp = rws_create(value.ptr);
					if (c->signApp == NULL) {
						lastErr = ERR_OUT_OF_MEMORY;
						goto onError;
					}
				} /* else: ignore other keys */
				if (k != NULL) {
					wStrDelete(k);
					*k = wcsdup(value.ptr);
					if (*k == NULL) {
						lastErr = ERR_OUT_OF_MEMORY;
						goto onError;
					}
				}
			}
		}
		/* error position handling */
		switch (ch) {
		case L'\n':
			++(pos.row);
			pos.col = 1;
			break;
		case L'\r':
			break; /* ignore */
		default:
			++(pos.col);
			break;
		}
	}
	lastErr = ERR_SUCCESS;
	res = true;
onError:
	if (fp != NULL) {
		fclose(fp);
	}
	wStrDelete(&content);
	if (lastErr == ERR_SYNTAX_ERROR && p != NULL) {
		*p = pos;
	}
	return res;
}


/**
 * Retrieves the current smart card status.
 * 
 * @param[in] c - INI configuration base
 * @param[out] cardStatus - set to the card status on success
 * @return `true` on success, else `false`
 */
bool iniConfigGetCardStatus(const tIniConfigBase * c, DWORD * cardStatus) {
	if (c == NULL || c->cardReader == NULL || cardStatus == NULL) {
		return false;
	}
	bool res = false;
	SCARDCONTEXT hContext = 0;
	SCARDHANDLE hCard = 0;
	DWORD activeProtocol = 0;
	LONG lReturn = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hContext);
	if (lReturn != SCARD_S_SUCCESS) {
		lastErr = ERR_UNKNOWN;
		goto onError;
	}
	lReturn = SCardConnectW(hContext, c->cardReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &activeProtocol);
	if (lReturn != SCARD_S_SUCCESS) {
		lastErr = ERR_UNKNOWN;
		goto onError;
	}
	lReturn = SCardStatusW(hCard, NULL, NULL, cardStatus, NULL, NULL, NULL);
	if (lReturn != SCARD_S_SUCCESS) {
		lastErr = ERR_UNKNOWN;
		goto onError;
	}
	res = true;
onError:
	if (hContext != 0) {
		if (hCard != 0) {
			SCardDisconnect(hCard, SCARD_LEAVE_CARD);
		}
		SCardReleaseContext(hContext);
	}
	return res;
}


/**
 * Validates the given pin.
 * 
 * @param[in] certId - related certificate ID
 * @param[in] pin - wide-character pin to validate
 * @param[in] len - pin length in number of characters
 * @return `true` if  the pin is valid, else `false`
 */
bool iniConfigValidatePin(const wchar_t * certId, const wchar_t * pin, DWORD len) {
	bool res = false;
	HCRYPTPROV hProv = 0;
	BYTE bPin[257];
	if ( ! CryptAcquireContextW(&hProv, certId, PROVIDER_NAME, PROV_RSA_FULL, 0) ) {
		goto onError;
	}
	ZeroMemory(bPin, sizeof(bPin));
	for (DWORD i = 0; i < len && i < sizeof(bPin); ++i) {
		bPin[i] = (BYTE)(pin[i]);
	}
	if ( ! CryptSetProvParam(hProv, PP_SIGNATURE_PIN, bPin, 0) ) {
		goto onError;
	}
	res = true;
onError:
	SecureZeroMemory(bPin, sizeof(bPin));
	if (hProv != 0) {
		CryptReleaseContext(hProv, 0);
	}
	return res;
}


/**
 * Returns the pin for the given `tIniConfigBase` value.
 * 
 * @param[in] c - pointer to the `tIniConfigBase` value
 * @param[in] parent - parent window or `NULL`
 * @param[out] pin - filled with the encrypted pin on success
 * @return `true` if a pin has been given, else `false`
 */
bool iniConfigGetPin(const tIniConfigBase * c, HWND parent, DATA_BLOB * pin) {
	bool res = false;
	if (c->cardName == NULL || c->cardReader == NULL || c->certId == NULL || pin == NULL) {
		return res;
	}
	ZeroMemory(pin, sizeof(*pin));
	DWORD cardStatus = 0;
	if ( ! iniConfigGetCardStatus(c, &cardStatus) ) {
		return res;
	}
	wchar_t userName[256];
	wchar_t domainName[256];
	wchar_t rawPin[256];
	DWORD userNameLen = 256, domainNameLen = 256, rawPinLen = 256;
	const size_t cardNameLen = wcslen(c->cardName) + 1;
	const size_t cardReaderLen = wcslen(c->cardReader) + 1;
	const size_t certIdLen = wcslen(c->certId) + 1;
	const size_t cspNameLen = wcslen(PROVIDER_NAME) + 1;
	/* size of KERB_CERTIFICATE_LOGON, KERB_SMARTCARD_CSP_INFO and wchar_t are a multiple of 2 on 32 and 64 bit system -> no need for alignment */
	const size_t certLogonSize = sizeof(KERB_CERTIFICATE_LOGON);
	const size_t cspInfoSize = sizeof(KERB_SMARTCARD_CSP_INFO) + ((cardNameLen + cardReaderLen + certIdLen + cspNameLen) * sizeof(wchar_t));
	const size_t certLogonBufferSize = certLogonSize + cspInfoSize;
	KERB_CERTIFICATE_LOGON * pCertLogon = CoTaskMemAlloc(certLogonBufferSize);
	if (pCertLogon == NULL) {
		return res;
	}
	ZeroMemory(pCertLogon, certLogonBufferSize);
	pCertLogon->MessageType = KerbCertificateLogon;
	pCertLogon->CspDataLength = (ULONG)cspInfoSize;
	pCertLogon->CspData = (PUCHAR)(ptrdiff_t)certLogonSize;
	KERB_SMARTCARD_CSP_INFO * pCspData = (KERB_SMARTCARD_CSP_INFO *)((ptrdiff_t)pCertLogon + pCertLogon->CspData);
	pCspData->dwCspInfoLen = pCertLogon->CspDataLength;
	pCspData->MessageType = 1;
	pCspData->flags = (DWORD)((cardStatus & 0xFFFF0000) | 1);
	pCspData->KeySpec = AT_KEYEXCHANGE;
	wchar_t * pBuffer = &(pCspData->bBuffer);
	size_t offset = 0;
	/* card name */
	pCspData->nCardNameOffset = (ULONG)offset;
	memcpy(pBuffer + offset, c->cardName, cardNameLen * sizeof(wchar_t));
	offset += cardNameLen;
	/* container name */
	pCspData->nContainerNameOffset = (ULONG)offset;
	memcpy(pBuffer + offset, c->certId, certIdLen * sizeof(wchar_t));
	offset += certIdLen;
	/* CSP name */
	pCspData->nCSPNameOffset = (ULONG)offset;
	memcpy(pBuffer + offset, PROVIDER_NAME, cspNameLen * sizeof(wchar_t));
	offset += cspNameLen;
	/* reader name */
	pCspData->nReaderNameOffset = (ULONG)offset;
	memcpy(pBuffer + offset, c->cardReader, cardReaderLen * sizeof(wchar_t));
	offset += cardReaderLen;
	/* configure and show credential UI */
	CREDUI_INFOW credUI;
	ZeroMemory(&credUI, sizeof(credUI));
	credUI.cbSize = (DWORD)sizeof(credUI);
	credUI.hwndParent = parent;
	credUI.pszCaptionText = L"Code Sign";
	credUI.pszMessageText = &(pCspData->bBuffer) + pCspData->nCardNameOffset;
	credUI.hbmBanner = NULL;
	ULONG authPackage = 0;
	const LPCVOID inAuthBuffer = pCertLogon;
	const ULONG inAuthBufferSize = (ULONG)certLogonBufferSize;
	LPVOID outCredBuffer = NULL;
	ULONG outCredBufferSize = 0;
	BOOL fSave = FALSE;
	const DWORD dwFlags = CREDUIWIN_IN_CRED_ONLY;
	if (CredUIPromptForWindowsCredentials(
		&credUI,
		0,
		&authPackage,
		inAuthBuffer,
		inAuthBufferSize,
		&outCredBuffer,
		&outCredBufferSize,
		&fSave,
		dwFlags
	) != ERROR_SUCCESS) {
		goto onError;
	}
	res = true;
	/* unpack PIN */
	ZeroMemory(userName, sizeof(userName));
	ZeroMemory(domainName, sizeof(domainName));
	ZeroMemory(rawPin, sizeof(rawPin));
	if ( ! CredUnPackAuthenticationBuffer(
		CRED_PACK_PROTECTED_CREDENTIALS,
		outCredBuffer,
		outCredBufferSize,
		userName,
		&userNameLen,
		domainName,
		&domainNameLen,
		rawPin,
		&rawPinLen
	) ) {
		goto onError;
	}
	if ( ! iniConfigValidatePin(c->certId, rawPin, rawPinLen) ) {
		goto onError;
	}
	DATA_BLOB pinBlob;
	pinBlob.pbData = (BYTE *)rawPin;
	pinBlob.cbData = rawPinLen * 2; /* including null-termination character */
	if ( ! CryptProtectData(&pinBlob, NULL, NULL, NULL, NULL, 0, pin) ) {
		goto onError;
	}
	ZeroMemory(userName, sizeof(userName));
	ZeroMemory(domainName, sizeof(domainName));
	SecureZeroMemory(rawPin, sizeof(rawPin));
onError:
	if (outCredBuffer != NULL) {
		CoTaskMemFree(outCredBuffer);
	}
	if (pCertLogon != NULL) {
		CoTaskMemFree(pCertLogon);
	}
	return res;
}


/**
 * Create the given INI base configuration.
 * 
 * @param[in] c - input configuration
 * @return created configuration or `NULL` on error
 */
tRcIniConfigBase * rcIniConfigBaseCreate(const tIniConfigBase * c) {
	if (c == NULL) {
		return NULL;
	}
	tRcIniConfigBase * res = calloc(1, sizeof(tRcIniConfigBase));
	if (res == NULL) {
		return NULL;
	}
	res->refCount = 1;
	res->cert->certId = wcsdup(c->certId);
	res->cert->cardName = wcsdup(c->cardName);
	res->cert->cardReader = wcsdup(c->cardReader);
	if (res->cert->certId == NULL || res->cert->cardName == NULL || res->cert->cardReader == NULL) {
		wStrDelete(&(res->cert->certId));
		wStrDelete(&(res->cert->cardName));
		wStrDelete(&(res->cert->cardReader));
		free(res);
		return NULL;
	}
	return res;
}


/**
 * Clones the given INI base configuration.
 * 
 * @param[in] c - configuration to clone
 * @return cloned configuration or `NULL` on error
 */
tRcIniConfigBase * rcIniConfigBaseClone(tRcIniConfigBase * c) {
	if (c == NULL) {
		return NULL;
	}
	InterlockedIncrement(&(c->refCount));
	return c;
}


/**
 * Compares two `tRcIniConfigBase` values. This is compatible with `HashFunctionCmpO`.
 * 
 * @param[in] lhs - pointer to the left-hand sided `tRcIniConfigBase` value
 * @param[in] rhs - pointer to the right-hand sided `tRcIniConfigBase` value
 * @return 0 if equal, not 0 in every other case
 */
#define _CMP_IGN(l, r) 0
#define _CMP_PTR(l, r, fn) \
	if (l == r) return 0; \
	if (l == NULL) return INT_MAX; \
	if (r == NULL) return INT_MIN; \
	res = fn(l, r); \
	if (res != 0) return res;
inline int rcIniConfigBaseCmp(const tRcIniConfigBase * lhs, const tRcIniConfigBase * rhs) {
	int res;
	_CMP_PTR(lhs, rhs, _CMP_IGN)
	_CMP_PTR(lhs->cert->certId, rhs->cert->certId, wcscmp)
	_CMP_PTR(lhs->cert->cardName, rhs->cert->cardName, wcscmp)
	_CMP_PTR(lhs->cert->cardReader, rhs->cert->cardReader, wcscmp)
	return 0;
	
}
#undef _CMP_IGN
#undef _CMP_PTR


/**
 * Hashes the given `tRcIniConfigBase` value. This is compatible with `HashFunctionHashO`.
 * 
 * @param[in] key - pointer to the `tRcIniConfig` value
 * @param[in] limit - size of hash table
 * @return hash value x with 0 <= x < limit
 */
inline size_t rcIniConfigBaseHash(const tRcIniConfigBase * key, const size_t limit) {
	wchar_t * strList[] = {
		key->cert->certId,
		key->cert->cardName,
		key->cert->cardReader,
	};
	uint32_t hash = 0xFFFFFFFF;
	for (size_t i = 0; i < ARRAY_SIZE(strList); ++i) {
		if (strList[i] != NULL) {
			hash = crc32Update(hash, strList[i], wcslen(strList[i]) * sizeof(wchar_t));
		}
	}
	return (hash ^ 0xFFFFFFFF) % limit;
}


/**
 * Deletes the strings within the given INI configuration base and the INI
 * configuration base itself.
 *
 * @param[in,out] c - INI configuration base to delete
 */
void rcIniConfigBaseDelete(tRcIniConfigBase * c) {
	if (c == NULL) {
		return;
	}
	if (InterlockedDecrement(&(c->refCount)) == 0) {
		wStrDelete(&(c->cert->certId));
		wStrDelete(&(c->cert->cardName));
		wStrDelete(&(c->cert->cardReader));
		free(c);
	}
}
