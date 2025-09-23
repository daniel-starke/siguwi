/**
 * @file siguwi-config.c
 * @author Daniel Starke
 * @date 2025-06-25
 * @version 2025-09-23
 */
#include "siguwi.h"


/**
 * Fills the certificate details for the given siguwi configuration.
 *
 * @param[in,out] c - siguwi configuration to fill
 * @param[in] hKey - `HCRYPTKEY` handle of the retrieved keys
 * @return `true` on success, else `false`
 */
bool fillCertInfo(tConfig * c, HCRYPTKEY hKey) {
	/* get the X.509 container */
	DWORD cbCert = 0;
	if ( ! CryptGetKeyParam(hKey, KP_CERTIFICATE, NULL, &cbCert, 0) ) {
		return false;
	}
	BYTE * pbCert = malloc(cbCert);
	if (pbCert == NULL) {
		return false;
	}
	if ( ! CryptGetKeyParam(hKey, KP_CERTIFICATE, pbCert, &cbCert, 0) ) {
		free(pbCert);
		return false;
	}
	/* create certificate context from DER blob */
	PCCERT_CONTEXT pCert = CertCreateCertificateContext(X509_ASN_ENCODING, pbCert, cbCert);
	free(pbCert);
	if ( ! pCert ) {
		return false;
	}
	/* get certificate name */
	wchar_t name[MAX_CONFIG_STR_LEN];
	const DWORD nameLen = CertGetNameStringW(pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name, ARRAY_SIZE(name));
	/* get subject */
	wchar_t subj[MAX_CONFIG_STR_LEN];
	const DWORD subjLen = CertNameToStrW(X509_ASN_ENCODING, &pCert->pCertInfo->Subject, CERT_X500_NAME_STR, subj, ARRAY_SIZE(subj));
	CertFreeCertificateContext(pCert);
	if (nameLen > 1 && subjLen > 1) {
		wStrDelete(&(c->certName));
		wStrDelete(&(c->certSubj));
		c->certProv = getCspFromCardNameW(name);
		c->certName = wcsdup(name);
		c->certSubj = wcsdup(subj);
		return c->certProv != NULL && c->certName != NULL && c->certSubj != NULL;
	}
	return false;
}


/**
 * Fills the container details for the given siguwi configuration.
 *
 * @param[in,out] c - siguwi configuration to fill with `certId` set
 * @return `true` on success, else `false`
 */
bool fillContainerInfo(tConfig * c) {
	if (c->certId == NULL || c->certProv == NULL) {
		return false;
	}
	HCRYPTPROV hProv = 0;
	bool res = false;
	if ( CryptAcquireContextW(&hProv, c->certId, c->certProv, PROV_TYPE, CRYPT_SILENT | CRYPT_VERIFYCONTEXT) ) {
		HCRYPTKEY hKey = 0;
		if ( CryptGetUserKey(hProv, AT_SIGNATURE, &hKey) ) {
			res = res || fillCertInfo(c, hKey);
			CryptDestroyKey(hKey);
		}
		if ( CryptGetUserKey(hProv, AT_KEYEXCHANGE, &hKey) ) {
			res = res || fillCertInfo(c, hKey);
			CryptDestroyKey(hKey);
		}
		CryptReleaseContext(hProv, 0);
	}
	return res;
}


/**
 * Adds the given certificate configuration to the passed configuration vector.
 *
 * @param[in,out] v - add to this vector
 * @param[in] c - configuration to add
 */
void configAdd(tVector * v, tConfig * c) {
	if (v == NULL || c == NULL) {
		return;
	}
	tConfig newC;
	ZeroMemory(&newC, sizeof(newC));
	newC.certProv = wcsdup(c->certProv);
	newC.certId = wcsdup(c->certId);
	newC.certName = wcsdup(c->certName);
	newC.certSubj = wcsdup(c->certSubj);
	newC.cardName = wcsdup(c->cardName);
	newC.cardReader = wcsdup(c->cardReader);
	if (newC.certProv != NULL && newC.certId != NULL && newC.certName != NULL && newC.certSubj != NULL && newC.cardName != NULL && newC.cardReader != NULL) {
		tConfig * pushedC = (tConfig *)vec_pushBack(v);
		if (pushedC != NULL) {
			*pushedC = newC;
			return;
		}
	}
	configDelete(0, &newC, NULL);
}


/**
 * Converts the given ANSI card name to a wide-character card name.
 *
 * @param[in] hContext - resource manager context
 * @param[in] atr - ATR string
 * @param[in] ref - reference ANSI card name
 * @return allocated wide-character card name on success, else `NULL`
 * @remarks Use `free()` on the result.
 */
wchar_t * getCardNameW(SCARDCONTEXT hContext, LPCBYTE atr, const CHAR * ref) {
	if (hContext == 0 || atr == NULL || ref == NULL) {
		return NULL;
	}
	LPWSTR mszCards = NULL;
	DWORD dwCardsSize = SCARD_AUTOALLOCATE;
	const LONG lReturn = SCardListCardsW(hContext, atr, NULL, 0, (LPWSTR)&mszCards, &dwCardsSize);
	if (lReturn != SCARD_S_SUCCESS) {
		return wFromStr(ref);
	}
	if (*mszCards == 0) {
		SCardFreeMemory(hContext, mszCards);
		return wFromStr(ref);
	}
	const wchar_t * bestFit = mszCards;
	size_t bestFitCount = 0;
	for (LPWSTR card = mszCards; *card != 0; card += (wcslen(card) + 1)) {
		size_t i = 0;
		LPCWSTR s1 = card;
		const CHAR * s2 = ref;
		for (; *s1 != 0 && *s2 != 0; ++s1, ++s2) {
			char ansi;
			if (WideCharToMultiByte(CP_ACP, 0, s1, 1, &ansi, 1, NULL, NULL) && ansi == *s2) {
				++i;
			}
		}
		if (i > bestFitCount) {
			bestFit = card;
			bestFitCount = i;
		}
	}
	wchar_t * res = wcsdup(bestFit);
	SCardFreeMemory(hContext, mszCards);
	return res;
}


/**
 * Retrieves the cryptographic service provider (CSP) name from the given smart card name.
 *
 * @param[in] cardName - smart card name
 * @return cryptographic service provider name or `NULL` on allocation error
 * @remarks Use `free()` on the result.
 */
wchar_t * getCspFromCardNameW(const wchar_t * cardName) {
	if (cardName == NULL) {
		return NULL;
	}
	wchar_t * res = PROVIDER_NAME; /* fallback to default if not found in registry */
	HKEY hKey = NULL;
	wchar_t path[MAX_REG_KEY_NAME + 1];
	wchar_t cspName[257];
	DWORD cspLen = (DWORD)(sizeof(cspName) - sizeof(wchar_t));
	snwprintf(path, ARRAY_SIZE(path), L"SOFTWARE\\Microsoft\\Cryptography\\Calais\\SmartCards\\%s", cardName);
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
		goto onError;
	}
	DWORD type;
	if (RegQueryValueExW(hKey, L"Crypto Provider", NULL, &type, (LPBYTE)cspName, &cspLen) != ERROR_SUCCESS || type != REG_SZ) {
		goto onError;
	}
	/* found valid CSP -> return it */
	cspName[cspLen / sizeof(wchar_t)] = 0;
	res = cspName;
onError:
	regCloseKeyPtr(&hKey);
	return wcsdup(res);
}


/**
 * Gets a list of possible siguwi configurations.
 *
 * @return configuration list or `NULL` on error
 */
tVector * configsGet(void) {
	bool res = false;
	SCARDCONTEXT hContext = 0;
	LPWSTR mszReaders = NULL;
	tVector * v = vec_create(sizeof(tConfig));
	if (v == NULL) {
		lastErr = ERR_OUT_OF_MEMORY;
		return NULL;
	}
	tConfig c;
	ZeroMemory(&c, sizeof(c));
	/* iterate smart card readers */
	LONG lReturn = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hContext);
	if (lReturn != SCARD_S_SUCCESS) {
		lastErr = ERR_UNKNOWN;
		goto onError;
	}
	DWORD dwReadersSize = SCARD_AUTOALLOCATE;
	lReturn = SCardListReadersW(hContext, NULL, (LPWSTR)&mszReaders, &dwReadersSize);
	if (lReturn != SCARD_S_SUCCESS) {
		if (lReturn == SCARD_E_NO_READERS_AVAILABLE) {
			lastErr = ERR_NO_SMARTCARD;
		} else {
			lastErr = ERR_UNKNOWN;
		}
		goto onError;
	}
	if (mszReaders == NULL || *mszReaders == 0) {
		lastErr = ERR_NO_SMARTCARD;
		goto onError;
	}
	for (LPWSTR reader = mszReaders; *reader != 0; reader += (wcslen(reader) + 1)) {
		/* for each reader */
		SCARDHANDLE hCard = 0;
		DWORD activeProtocol = 0;
		wchar_t readerStr[MAX_CONFIG_STR_LEN];
		ZeroMemory(readerStr, sizeof(readerStr));
		wcscpy_s(readerStr, ARRAY_SIZE(readerStr), reader);
		lReturn = SCardConnectW(hContext, readerStr, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &activeProtocol);
		if (lReturn == SCARD_S_SUCCESS) {
			CHAR cardName[MAX_CONFIG_STR_LEN];
			DWORD cardNameLen = ARRAY_SIZE(cardName);
			lReturn = SCardGetAttrib(hCard, SCARD_ATTR_VENDOR_IFD_TYPE, (LPBYTE)cardName, &cardNameLen);
			if (lReturn != SCARD_S_SUCCESS) {
				SCardDisconnect(hCard, SCARD_LEAVE_CARD);
				continue;
			}
			DWORD readerLen = (DWORD)ARRAY_SIZE(readerStr);
			DWORD cardProtocol = 0;
			DWORD cardStatus = 0;
			BYTE atr[36];
			ZeroMemory(atr, sizeof(atr));
			DWORD atrLen = sizeof(atr);
			lReturn = SCardStatusW(hCard, readerStr, &readerLen, &cardStatus, &cardProtocol, atr, &atrLen);
			if (lReturn == SCARD_S_SUCCESS) {
				/* try to get the wide-character string for the smart card */
				c.cardName = getCardNameW(hContext, atr, cardName);
				if (c.cardName == NULL) {
					lastErr = ERR_OUT_OF_MEMORY;
					SCardDisconnect(hCard, SCARD_LEAVE_CARD);
					continue;
				}
				c.certProv = getCspFromCardNameW(c.cardName);
				if (c.certProv == NULL) {
					lastErr = ERR_OUT_OF_MEMORY;
					wStrDelete(&(c.cardName));
					SCardDisconnect(hCard, SCARD_LEAVE_CARD);
					continue;
				}
				/* get the cryptographic service provider */
				wchar_t path[MAX_CONFIG_STR_LEN + 6];
				snwprintf(path, ARRAY_SIZE(path), L"\\\\.\\%s\\", readerStr);
				HCRYPTPROV hProv;
				if ( CryptAcquireContextW(&hProv, path, c.certProv, PROV_TYPE, CRYPT_SILENT | CRYPT_VERIFYCONTEXT) ) {
					/* for each container */
					CHAR containerName[MAX_CONFIG_STR_LEN];
					DWORD cnLen = MAX_CONFIG_STR_LEN;
					DWORD dwFlags = CRYPT_FIRST;
					while ( CryptGetProvParam(hProv, PP_ENUMCONTAINERS, (BYTE *)containerName, &cnLen, dwFlags) ) {
						/* fill details */
						c.certId = wFromStr(containerName);
						if (c.certId != NULL) {
							if ( fillContainerInfo(&c) ) {
								c.cardReader = wcsdup(readerStr);
								if (c.cardReader != NULL) {
									/* add to result vector */
									configAdd(v, &c);
									wStrDelete(&(c.cardReader));
								}
							}
							wStrDelete(&(c.certId));
						}
						dwFlags = CRYPT_NEXT;
						cnLen = sizeof(containerName);
					}
					CryptReleaseContext(hProv, 0);
				}
				wStrDelete(&(c.certProv));
				wStrDelete(&(c.cardName));
			}
			SCardDisconnect(hCard, SCARD_LEAVE_CARD);
		}
	}
	res = true;
onError:
	if (hContext != 0) {
		if (mszReaders != NULL) {
			SCardFreeMemory(hContext, mszReaders);
		}
		SCardReleaseContext(hContext);
	}
	if ( ! res ) {
		configsDelete(v);
		v = NULL;
	}
	configDelete(0, &c, NULL);
	return v;
}


/**
 * Adds the configuration ID to the given combobox at the end.
 *
 * @param[in] index - configuration vector index
 * @param[in,out] data - configuration
 * @param[in] hCombo - pointer to the window handle of the combobox
 * @return 0 to abort
 * @return 1 to continue
 */
int comboConfigIdAdd(const size_t index, tConfig * data, HWND * hCombo) {
	PCF_UNUSED(index);
	if (data == NULL || hCombo == NULL) {
		return 0;
	}
	SendMessageW(*hCombo, CB_ADDSTRING, 0, (LPARAM)(data->certId));
	return 1;
}


/**
 * Prints the `siguwi` configuration using the given string buffer.
 *
 * @param[in] index - configuration vector index (unused)
 * @param[in,out] data - configuration
 * @param[in] sb - string buffer handle
 * @return 0 to abort
 * @return 1 to continue
 */
int configPrint(const size_t index, tConfig * data, tUStrBuf * sb) {
	PCF_UNUSED(index);
	if (data == NULL || sb == NULL) {
		return 0;
	}
	usb_addC(sb, L'[');
	usb_add(sb, DEFAULT_CONFIG_GROUP);
	usb_add(sb, L"]\r\n");
	usb_addFmt(sb, L"# Name: %s\r\n", data->certName);
	usb_addFmt(sb, L"# Subject: %s\r\n", data->certSubj);
	usb_addFmt(sb, L"# CSP: %s\r\n", data->certProv);
	usb_addFmt(sb, L"certId = \"%s\"\r\n", data->certId);
	usb_addFmt(sb, L"cardName = \"%s\"\r\n", data->cardName);
	usb_addFmt(sb, L"cardReader = \"%s\"\r\n", data->cardReader);
	usb_addFmt(sb, L"# signing application parameters:\r\n");
	usb_addFmt(sb, L"# %%1 - input file\r\n");
	usb_addFmt(sb, L"# %%2 - pin (or via standard input if not set)\r\n");
	usb_addFmt(sb, L"signApp = \"<enter your application command-line here>\"\r\n");
	return 1;
}


/**
 * Deletes a single configuration.
 *
 * @param[in] index - configuration vector index (unused)
 * @param[in,out] data - configuration
 * @param[in] param - user parameter (unused)
 * @return 0 to abort
 * @return 1 to continue
 */
int configDelete(const size_t index, tConfig * data, void * param) {
	PCF_UNUSED(index);
	PCF_UNUSED(param);
	if (data == NULL) {
		return 0;
	}
	wStrDelete(&(data->certProv));
	wStrDelete(&(data->certId));
	wStrDelete(&(data->certName));
	wStrDelete(&(data->certSubj));
	wStrDelete(&(data->cardName));
	wStrDelete(&(data->cardReader));
	return 1;
}


/**
 * Deletes the given configuration vector.
 *
 * @param[in,out] v - configuration vector to delete
 */
void configsDelete(tVector * v) {
	vec_traverse(v, (VectorVisitor)configDelete, NULL);
	vec_delete(v);
}


/**
 * Updates the configuration window controls after a change in the window size.
 *
 * @param[in] ctx - configuration window context
 */
void configsWndResize(const tConfigWndCtx * ctx) {
	RECT rect;
	GetClientRect(ctx->hWnd, &rect);
	const int width = rect.right;
	const int height = rect.bottom;
	HDWP hDwp = BeginDeferWindowPos(3);
	hDwp = DeferWindowPos(hDwp, ctx->hCombo, NULL, calcPixels(10), calcPixels(10), width - calcPixels(20), calcPixels(300), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
	hDwp = DeferWindowPos(hDwp, ctx->hEdit, NULL, calcPixels(10), calcPixels(45), width - calcPixels(20), height - calcPixels(90), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
	hDwp = DeferWindowPos(hDwp, ctx->hButton, NULL, width - calcPixels(100), height - calcPixels(35), calcPixels(90), calcPixels(25), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
	EndDeferWindowPos(hDwp);
	InvalidateRect(ctx->hWnd, NULL, FALSE);
}


/**
 * Sub class for the `WC_EDITW` control to enable insertion via tab.
 *
 * @param[in] hWnd - window handle
 * @param[in] msg - event message
 * @param[in] wParam - associated wParam
 * @param[in] lParam - associated lParam
 * @param[in] uIdSubclass - sub class ID
 * @param[in] dwRefData - data provided via `SetWindowSubclass`
 * @return result value
 */
static LRESULT CALLBACK configsEditSubClassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	PCF_UNUSED(uIdSubclass);
	PCF_UNUSED(dwRefData);
	if (msg == WM_KEYDOWN && wParam == VK_TAB) {
		SendMessageW(hWnd, EM_REPLACESEL, TRUE, (LPARAM)L"\t");
		return 0;
	}
	return DefSubclassProc(hWnd, msg, wParam, lParam);
}


/**
 * Window callback handler for the configurations window.
 *
 * @param[in] hWnd - window handle
 * @param[in] msg - event message
 * @param[in] wParam - associated wParam
 * @param[in] lParam - associated lParam
 * @return result value
 */
LRESULT CALLBACK configsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	tConfigWndCtx * ctx = (msg == WM_CREATE) ? (tConfigWndCtx *)(((CREATESTRUCTW *)lParam)->lpCreateParams) : (tConfigWndCtx *)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
	if (ctx == NULL || ctx->v == NULL) {
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	switch (msg) {
	case WM_CREATE: {
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)ctx);
		ctx->hWnd = hWnd;
		ctx->hCombo = CreateWindowW(WC_COMBOBOXW, NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP, 0, 0, 0, 0, hWnd, (HMENU)IDC_CONFIG_CBOX, gInst, NULL);
		ctx->hEdit = CreateWindowW(WC_EDITW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 0, 0, 0, 0, hWnd, (HMENU)IDC_CONFIG_VIEW, gInst, NULL);
		ctx->hButton = CreateWindowW(WC_BUTTONW, L"Save As...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)IDC_SAVE_AS, gInst, NULL);
		if ( ! (ctx->hCombo && ctx->hEdit && ctx->hButton) ) {
			CloseWindow(hWnd);
			break;
		}
		SetWindowSubclass(ctx->hEdit, configsEditSubClassProc, 1, 0);
		/* set fonts */
		SendMessage(hWnd, WM_SETFONT, (WPARAM)(ctx->hFont), TRUE);
		SendMessage(ctx->hCombo, WM_SETFONT, (WPARAM)(ctx->hFont), TRUE);
		SendMessage(ctx->hEdit, WM_SETFONT, (WPARAM)(ctx->hFont), TRUE);
		SendMessage(ctx->hButton, WM_SETFONT, (WPARAM)(ctx->hFont), TRUE);
		/* add possible configuration IDs */
		vec_traverse(ctx->v, (VectorVisitor)comboConfigIdAdd, &(ctx->hCombo));
		SendMessageW(ctx->hCombo, CB_SETCURSEL, 0, 0);
		SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDC_CONFIG_CBOX, CBN_SELCHANGE), 0);
		configsWndResize(ctx);
		} break;
	case WM_GETMINMAXINFO: {
		MINMAXINFO * pmmi = (MINMAXINFO *)lParam;
		pmmi->ptMinTrackSize.x = calcPixels(500);
		pmmi->ptMinTrackSize.y = calcPixels(300);
		} return 0;
	case WM_SIZE:
		configsWndResize(ctx);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CONFIG_CBOX:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				const size_t index = (size_t)SendMessageW(ctx->hCombo, CB_GETCURSEL, 0, 0);
				if ( ctx->sb ) {
					usb_clear(ctx->sb);
					configPrint(index, vec_at(ctx->v, index), ctx->sb);
					wchar_t * str = usb_get(ctx->sb);
					if ( str ) {
						SetWindowTextW(ctx->hEdit, str);
						free(str);
					} else {
						SetWindowTextW(ctx->hEdit, L"");
					}
				} else {
					SetWindowTextW(ctx->hEdit, L"");
				}
			}
			break;
		case IDC_SAVE_AS:
			if (HIWORD(wParam) == BN_CLICKED) {
				wchar_t szFile[MAX_PATH];
				OPENFILENAMEW ofn;
				ZeroMemory(szFile, sizeof(szFile));
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = ARRAY_SIZE(szFile);
				ofn.lpstrFilter = L"INI File (*.ini)\0*.ini\0All Files (*.*)\0*.*\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFileTitle = NULL;
				ofn.nMaxFileTitle = 0;
				ofn.lpstrInitialDir = NULL;
				ofn.Flags = OFN_OVERWRITEPROMPT;
				if (GetSaveFileNameW(&ofn) == TRUE) {
					if (ofn.nFilterIndex == 1 && PathFindExtensionW(szFile)[0] == 0) {
						/* append missing file extension */
						wcscat_s(szFile, ARRAY_SIZE(szFile), L".ini");
					}
					FILE * fp = _wfopen(szFile, L"wb");
					if ( fp ) {
						const int len = GetWindowTextLengthW(ctx->hEdit);
						if (len > 0) {
							wchar_t * buf = malloc(((size_t)(len + 1)) * sizeof(wchar_t));
							if ( buf ) {
								const int realLen = GetWindowTextW(ctx->hEdit, buf, len + 1);
								if (realLen > 0 && realLen <= len) {
									buf[realLen] = 0;
									wRemoveCr(buf);
									/* convert to UTF-8 */
									char * utf8 = wToUtf8(buf);
									if (utf8 != NULL) {
										fwrite("\xEF\xBB\xBF", 3, 1, fp); /* BOM: UTF-8 */
										fwrite(utf8, strlen(utf8), 1, fp);
										free(utf8);
									}
								}
								free(buf);
							}
						}
						fclose(fp);
					} else {
						showFmtMsg(hWnd, MB_OK | MB_ICONERROR, L"Error (save as)", L"%s\n%s", errStr[ERR_CREATE_FILE], szFile);
					}
				}
			}
			break;
		default:
			break;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		ctx->hCombo = NULL;
		ctx->hEdit = NULL;
		ctx->hButton = NULL;
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
 * Shows a list of possible siguwi configurations.
 *
 * @param[in] cmdshow - `ShowWindow` parameter
 * @return program exit code
 */
int showConfigs(int cmdshow) {
	tConfigWndCtx ctx;
	int res = EXIT_FAILURE;
	ZeroMemory(&ctx, sizeof(ctx));
	/* load default window font */
	ctx.hFont = CreateFontW(calcFontSize(85), 0, 0, 0, 0, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"MS Shell Dlg");
	if (ctx.hFont == NULL) {
		MessageBoxW(NULL, errStr[ERR_CREATEFONT], L"Error (list)", MB_OK | MB_ICONERROR);
		goto onError;
	}
	/* allocate string buffer */
	ctx.sb = usb_create(512);
	if (ctx.sb == NULL) {
		MessageBoxW(NULL, errStr[ERR_OUT_OF_MEMORY], L"Error (list)", MB_OK | MB_ICONERROR);
		goto onError;
	}
	/* get configurations */
	ctx.v = configsGet();
	if (ctx.v == NULL) {
		showFmtMsg(NULL, MB_OK | MB_ICONERROR, L"Error (list)", L"Failed to list possible configurations.\n%s", errStr[lastErr]);
		goto onError;
	}
	/* register window class */
	WNDCLASSEXW wc = {
		/* cbSize        */ sizeof(WNDCLASSEXW),
		/* style         */ CS_HREDRAW | CS_VREDRAW,
		/* lpfnWndProc   */ configsWndProc,
		/* cbClsExtra    */ 0,
		/* cbWndExtra    */ 0,
		/* hInstance     */ gInst,
		/* hIcon         */ LoadIconW(gInst, MAKEINTRESOURCEW(IDI_APP_ICON)),
		/* hCursor       */ LoadCursorW(NULL, IDC_ARROW),
		/* hbrBackground */ (HBRUSH)COLOR_3DSHADOW,
		/* lpszMenuName  */ NULL,
		/* lpszClassName */ L"ConfigurationsClass",
		/* hIconSm       */ LoadIconW(gInst, MAKEINTRESOURCEW(IDI_APP_ICON))
	};
	RegisterClassExW(&wc);
	/* create and show window */
	HWND hWnd = CreateWindowW(wc.lpszClassName, L"Configurations", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, calcPixels(640), calcPixels(350), NULL, NULL, gInst, (LPVOID)&ctx);
	ShowWindow(hWnd, cmdshow);
	UpdateWindow(hWnd);
	MSG msg;
	while ( GetMessage(&msg, NULL, 0, 0) ) {
		if ( ! IsDialogMessage(hWnd, &msg) ) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	res = (int)msg.wParam;
onError:
	vec_delete(ctx.v);
	usb_delete(ctx.sb);
	if (ctx.hFont != NULL) {
		DeleteObject(ctx.hFont);
	}
	return res;
}
