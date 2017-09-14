#include <windows.h>
#include <winnt.h>
#include <WinTrust.h>
#include <delayimp.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <SDKDDKVer.h>
#include <shellapi.h>
#include <commctrl.h>
#include "resource.h"
#pragma comment (lib, "comctl32.lib")

#define TOOLBAR_ID 200
#define check_bit(data, loc) ((data) & (loc))

struct {
	PIMAGE_IMPORT_DESCRIPTOR idt;
	DWORD IMAGE_NAMEADDR_TABLE;
	PIMAGE_IMPORT_BY_NAME IBN;
	PVOID VA;
	PVOID P2RAW;
	PVOID VA2;
	PVOID P2RAW2;
}typedef IDT;

struct {
	PImgDelayDescr DELAY_IMPORT_DESCRIPTOR;
	DWORD IMAGE_NAMEADDR_TABLE;
	PIMAGE_IMPORT_BY_NAME IBN;
	PVOID VA;
	PVOID P2RAW;
	PVOID VA2;
	PVOID P2RAW2;
}typedef DELAY_IDT;

struct {
	PIMAGE_RESOURCE_DIRECTORY rsrc_direc;
	PIMAGE_RESOURCE_DIRECTORY_ENTRY * rsrc_direc_ents;
}typedef IMAGE_RESOURCE_DIRECTORY_LEVEL;

typedef struct _IMAGE_RESOURCE_LISTS { //free 필요
	IMAGE_RESOURCE_DIRECTORY_LEVEL rsrc_types;
	IMAGE_RESOURCE_DIRECTORY_LEVEL * rsrc_nameid;
	IMAGE_RESOURCE_DIRECTORY_LEVEL * rsrc_language;
	PIMAGE_RESOURCE_DATA_ENTRY * rsrc_data;
	PVOID VA;
	PVOID P2RAW;
	int numofnameid;
	int numofdata;
} IMAGE_RESOURCE_LISTS; //free 필요

typedef struct _CERTIFICATE_TABLE { //존재시 free 필요
	LPWIN_CERTIFICATE certi_table;
	struct _CERTIFICATE_TABLE * next_list;
}CERTIFICATE_TABLE;

typedef struct _RELOCATION { //존재시 free 필요
	PIMAGE_BASE_RELOCATION reloca;
	PWORD word;
	struct _RELOCATION * next_list;
} RELOCATION;

struct LISTVIEW {
	HWND hList;
	HTREEITEM treeitem;
}typedef LISTVIEW;

struct BODYLISTVIEW {
	HWND hList;
	HTREEITEM treeitem;
	PVOID Start;
	PVOID End;
}typedef BODYLISTVIEW; //free 필요

HINSTANCE hInst;
HWND hWnd;

HWND hTree;
HWND hToolbar;
HWND hTip;
HFONT hFont;
HDC hdc;

IDT import_table;
IMAGE_RESOURCE_LISTS rsrc_section;
PIMAGE_TLS_DIRECTORY tls_table;
PIMAGE_LOAD_CONFIG_DIRECTORY load_cfg_direc;
PIMAGE_DEBUG_DIRECTORY debug_direc;
PIMAGE_EXPORT_DIRECTORY export_table;
PIMAGE_COR20_HEADER cli_header;
CERTIFICATE_TABLE certificate_table;
PIMAGE_BOUND_IMPORT_DESCRIPTOR bound_import;
DELAY_IDT delay_idt;
RELOCATION reloc;

PIMAGE_DOS_HEADER dos_header;
PIMAGE_NT_HEADERS nt_header;
PIMAGE_FILE_HEADER file_header;
PIMAGE_OPTIONAL_HEADER optional_header;
PIMAGE_DATA_DIRECTORY data_directory[16];
PIMAGE_SECTION_HEADER section_header;

FILETIME file_time;
SYSTEMTIME sys_time;
ULARGE_INTEGER li;

HANDLE hMap, hFile;
PVOID backup, backup2;
PVOID temp_next = (PVOID)-1;
PVOID temp_prev = (PVOID)-1;
PVOID base_ptr;

DWORD global_RVA;
DWORD local_RVA;

LISTVIEW listview[50];
BODYLISTVIEW * body_listview;

BOOL data_directories_present[16] = {
	FALSE, FALSE, FALSE, FALSE, FALSE,
	FALSE, FALSE, FALSE, FALSE, FALSE,
	FALSE, FALSE, FALSE, FALSE, FALSE,
	FALSE
};

WCHAR * DATA_DIRECTORY_STR[16] = {
	L"EXPORT_TABLE", L"IMPORT_TABLE", L"RESOURCE_TABLE", L"EXCEPTION_TABLE", L"CERTIFICATE_TABLE",
	L"BASE_RELOCATION_TABLE", L"DEBUG_DIRECTORY", L"ARCHITECTURE_SPECIFIC_DATA", L"GLOBAL_POINTER_REGISTER", L"TLS_TABLE",
	L"LOAD_CONFIGURATION_TABLE", L"BOUND_IMPORT_TABLE", L"IMPORT_ADDRESS_TABLE", L"DELAY_IMPORT_DESCRIPTORS", L"CLI_HEADER",
	L"NULL_STRUCTURE"
};

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
VOID Initialize(HWND hWnd, WCHAR * StrOfFile);
VOID RemoveValues(HWND hWnd);
VOID InsertHexcode(PVOID Start, PVOID End, HWND hWndOfListview);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	hInst = hInstance;
	MSG msg;
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU1);
	wcex.lpszClassName = L"Main Window Class";
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON2));

	RegisterClassExW(&wcex);

	hWnd = CreateWindowW(L"Main Window Class", L"PE Viewer", WS_OVERLAPPEDWINDOW ^ (WS_THICKFRAME | WS_MAXIMIZEBOX),
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TBBUTTON ToolbarBtn[4] = {
		{ STD_FILEOPEN, 10, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0 },
		{ STD_DELETE, 11, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0 },
		{ STD_UNDO, 12, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0 },
		{ STD_REDOW, 13, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0 }
	};

	LPTOOLTIPTEXT lptooltip;
	TCHAR * pszText = NULL;

	switch (message) {
	case WM_CREATE:
		DragAcceptFiles(hWnd, TRUE);
		InitCommonControls();
		hToolbar = CreateToolbarEx(hWnd, WS_CHILD | WS_VISIBLE | WS_BORDER | TBSTYLE_FLAT | CCS_TOP | TBSTYLE_TOOLTIPS, TOOLBAR_ID, 1, HINST_COMMCTRL, IDB_STD_SMALL_COLOR, ToolbarBtn, 4, 16, 16, 16, 16, sizeof(TBBUTTON));
		hTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWnd, NULL, hInst, NULL);
		hFont = CreateFont(18, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Consolas");
		break;
	case WM_LBUTTONDOWN:
		SendMessage(hToolbar, TTM_ACTIVATE, (WPARAM)TRUE, 0);
		break;
	case WM_RBUTTONDOWN:
		SendMessage(hToolbar, TTM_ACTIVATE, (WPARAM)FALSE, 0);
		break;
	case WM_DROPFILES:
		WCHAR StrOfFile[260];
		DragQueryFile((HDROP)wParam, 0, StrOfFile, MAX_PATH);
		RemoveValues(hWnd);
		Initialize(hWnd, StrOfFile);
		SetWindowText(hWnd, StrOfFile);
		DragFinish((HDROP)wParam);
		break;
	case WM_NOTIFY:
		LPNMHDR hdr;
		hdr = (LPNMHDR)lParam;
		if (hdr->hwndFrom == hTree) {
			switch (hdr->code) {
			case TVN_SELCHANGED:
				HTREEITEM temp = TreeView_GetSelection(hTree);
				BOOL changed = FALSE;

				for (int i = 0; i < 50 && listview[i].hList != NULL; i++) {
					LONG Style = GetWindowLong(listview[i].hList, GWL_STYLE);
					if (check_bit(Style, WS_VISIBLE)) {
						SetWindowLong(listview[i].hList, GWL_STYLE, WS_CHILD | WS_BORDER | LVS_REPORT);
						changed = TRUE;
						break;
					}
				}

				if (changed != TRUE) {
					for (int i = 0; i < file_header->NumberOfSections; i++) {
						LONG Style = GetWindowLong(body_listview[i].hList, GWL_STYLE);
						if (check_bit(Style, WS_VISIBLE)) {
							SetWindowLong(body_listview[i].hList, GWL_STYLE, WS_CHILD | WS_BORDER | LVS_REPORT);
							break;
						}
					}
				}

				changed = FALSE;

				for (int i = 0; i < 50 && listview[i].hList != NULL; i++) {
					if (listview[i].treeitem == temp) {
						SetWindowLong(listview[i].hList, GWL_STYLE, WS_CHILD | WS_BORDER | WS_VISIBLE | LVS_REPORT);
						changed = TRUE;
						break;
					}
				}

				if (changed != TRUE) {
					for (int i = 0; i < file_header->NumberOfSections; i++) {
						if (body_listview[i].treeitem == temp) {
							SetWindowLong(body_listview[i].hList, GWL_STYLE, WS_CHILD | WS_BORDER | WS_VISIBLE | LVS_REPORT);
							break;
						}
					}
				}

				InvalidateRgn(hWnd, NULL, TRUE);
				break;
			}
		}
		else if (hdr->code == TTN_NEEDTEXT) {
			lptooltip = (LPTOOLTIPTEXT)lParam;
			switch (lptooltip->hdr.idFrom) {
			case 10:
				pszText = L"파일을 오픈합니다";
				break;
			case 11:
				pszText = L"프로그램을 종료합니다.";
				break;
			case 12:
				pszText = L"이전 항목으로 이동합니다.";
				break;
			case 13:
				pszText = L"다음 항목으로 이동합니다.";
				break;
			}
			wcscpy_s(lptooltip->szText, pszText);
			break;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case 12: {
			HTREEITEM temp = TreeView_GetSelection(hTree);
			temp = TreeView_GetPrevVisible(hTree, temp);
			TreeView_Select(hTree, temp, TVGN_CARET);
			break; }
		case 13: {
			HTREEITEM temp = TreeView_GetSelection(hTree);
			temp = TreeView_GetNextVisible(hTree, temp);
			TreeView_Select(hTree, temp, TVGN_CARET);
			break; }
		case ID_EXIT:
		case 11:
			RemoveValues(hWnd);
			DestroyWindow(hWnd);
			break;
		case ID_FILEOPEN:
		case 10: {
			OPENFILENAME OFN;
			WCHAR lpstrFile[100] = { 0, };

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hWnd;
			OFN.lpstrFilter = L"EXE File(*.exe)\0 * .exe\0DLL File(*.dll)\0 * .dll\0SYS File(*.sys)\0 * .sys\0";
			OFN.lpstrFile = lpstrFile;
			OFN.nMaxFile = 100;
			OFN.lpstrInitialDir = L".";

			if (GetOpenFileNameW(&OFN) != 0) {
				RemoveValues(hWnd);
				Initialize(hWnd, OFN.lpstrFile);
				SetWindowText(hWnd, OFN.lpstrFile);
			}
			break; }
		}
		break;
	case WM_DESTROY:
		RemoveValues(hWnd);
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

VOID Initialize(HWND hWnd, WCHAR * StrOfFile) {
	RECT rect;
	WCHAR temp[50] = { 0, };

	int NumOfTreeitems = 0;
	int itemnumber = 0;

	TVINSERTSTRUCT COL;

	LVCOLUMN LV;
	LVITEM IT;

	__try {

		hFile = CreateFile(StrOfFile, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

		base_ptr = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
		dos_header = (PIMAGE_DOS_HEADER)base_ptr;
		nt_header = (PIMAGE_NT_HEADERS)((DWORD)dos_header + (DWORD)dos_header->e_lfanew);

		file_header = (PIMAGE_FILE_HEADER)(&nt_header->FileHeader);

		if (file_header->Machine != IMAGE_FILE_MACHINE_I386) {
			MessageBox(hWnd, L"이 프로그램은 64비트 프로그램을 지원하지 않습니다.", L"경고", MB_OK);
			return;
		}

		optional_header = (PIMAGE_OPTIONAL_HEADER)(&nt_header->OptionalHeader);

		for (int i = 0; i < 16; i++) {
			data_directory[i] = optional_header->DataDirectory + i;
		}

		section_header = (PIMAGE_SECTION_HEADER)((DWORD)optional_header + (DWORD)file_header->SizeOfOptionalHeader);

		memset(&sys_time, 0, sizeof(SYSTEMTIME));

		sys_time.wYear = 1970;
		sys_time.wMonth = 1;
		sys_time.wDay = 1;
		sys_time.wHour = 9;

		SystemTimeToFileTime(&sys_time, &file_time);

		li.HighPart = file_time.dwHighDateTime;
		li.LowPart = file_time.dwLowDateTime;
		li.QuadPart += ((LONGLONG)file_header->TimeDateStamp * 10000000LL);
		file_time.dwHighDateTime = li.HighPart;
		file_time.dwLowDateTime = li.LowPart;
		FileTimeToSystemTime(&file_time, &sys_time);

		body_listview = (BODYLISTVIEW *)calloc(file_header->NumberOfSections, sizeof(BODYLISTVIEW));

		GetClientRect(hWnd, &rect);
		hTree = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_TREEVIEW, NULL, WS_CHILD | WS_BORDER | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT, rect.left + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);
		SendMessage(hTree, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);
		//-------------------------------------------------
		COL.hParent = 0;
		COL.hInsertAfter = TVI_LAST;
		COL.item.mask = TVIF_TEXT;
		COL.item.pszText = L"IMAGE_DOS_HEADER";
		listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

		listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
			rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

		SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

		LV.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		LV.fmt = LVCFMT_LEFT;
		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 0;
		LV.pszText = L"DATA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 1;
		LV.pszText = L"DESCRIPTION";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 2;
		LV.pszText = L"VALUE";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 3;
		LV.pszText = L"RVA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

		IT.mask = LVIF_TEXT;
		IT.iItem = 0;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", dos_header->e_magic);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"e_magic");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		char mzstr[10];
		sprintf(mzstr, "%s", &dos_header->e_magic);
		int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, mzstr, -1, NULL, NULL);
		MultiByteToWideChar(CP_ACP, 0, mzstr, -1, temp, len);
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 3;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x3C;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 1;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", dos_header->e_lfanew);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"e_lfanew");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 3;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		NumOfTreeitems++;
		//----------------------------------------------------
		COL.item.pszText = L"MS-DOS Stub Codes";
		listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

		listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
			rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

		SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

		LV.cx = (rect.right / 2 - 20) - 270;
		LV.iSubItem = 0;
		LV.pszText = L"HEX CODES";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

		LV.cx = 170;
		LV.iSubItem = 1;
		LV.pszText = L"STRINGS";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

		LV.cx = 100;
		LV.iSubItem = 1;
		LV.pszText = L"RVA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

		InsertHexcode(dos_header + 1, nt_header, listview[NumOfTreeitems].hList);

		NumOfTreeitems++;
		//-----------------------------------------------------
		COL.item.pszText = L"IMAGE_NT_HEADER";
		listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

		listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
			rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

		SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

		LV.cx = (rect.right / 2 - 20) - 270;
		LV.iSubItem = 0;
		LV.pszText = L"HEX CODES";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

		LV.cx = 170;
		LV.iSubItem = 1;
		LV.pszText = L"STRINGS";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

		LV.cx = 100;
		LV.iSubItem = 2;
		LV.pszText = L"RVA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

		InsertHexcode(nt_header, nt_header + 1, listview[NumOfTreeitems].hList);

		global_RVA = dos_header->e_lfanew;

		NumOfTreeitems++;
		//----------------------------------------------------------
		COL.hParent = listview[2].treeitem;
		COL.item.pszText = L"IMAGE_NT_SIGNITURE";
		listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

		listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
			rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

		SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 0;
		LV.pszText = L"DATA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 1;
		LV.pszText = L"DESCRIPTION";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 2;
		LV.pszText = L"VALUE";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 3;
		LV.pszText = L"RVA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

		IT.iItem = 0;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", nt_header->Signature);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"Signature");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		sprintf(mzstr, "%s", &nt_header->Signature);
		len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, mzstr, -1, NULL, NULL);
		MultiByteToWideChar(CP_ACP, 0, mzstr, -1, temp, len);
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 3;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		NumOfTreeitems++;
		//----------------------------------------------------------
		COL.item.pszText = L"IMAGE_FILE_HEADER";
		listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

		listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
			rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

		SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

		LV.cx = (rect.right / 2 - 20) / 3;
		LV.iSubItem = 0;
		LV.pszText = L"DATA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

		LV.cx = (rect.right / 2 - 20) / 3;
		LV.iSubItem = 1;
		LV.pszText = L"DESCRIPTION";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

		LV.cx = (rect.right / 2 - 20) / 3;
		LV.iSubItem = 2;
		LV.pszText = L"RVA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

		IT.iItem = 0;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", file_header->Machine);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"Machine");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 1;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", file_header->NumberOfSections);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"NumberOfSections");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 2;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", file_header->TimeDateStamp);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"TimeDateStamp");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 3;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", file_header->PointerToSymbolTable);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"PointerToSymbolTable");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 4;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", file_header->NumberOfSymbols);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"NumberOfSymbols");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 5;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", file_header->SizeOfOptionalHeader);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfOptionalHeader");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 6;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", file_header->Characteristics);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"Characteristics");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		NumOfTreeitems++;
		//----------------------------------------------------------
		COL.item.pszText = L"IMAGE_OPTIONAL_HEADER";
		listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

		listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
			rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

		SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

		LV.cx = (rect.right / 2 - 20) / 3;
		LV.iSubItem = 0;
		LV.pszText = L"DATA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

		LV.cx = (rect.right / 2 - 20) / 3;
		LV.iSubItem = 1;
		LV.pszText = L"DESCRIPTION";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

		LV.cx = (rect.right / 2 - 20) / 3;
		LV.iSubItem = 2;
		LV.pszText = L"RVA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

		IT.iItem = 0;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->Magic);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"Magic");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 1;
		IT.iSubItem = 0;
		wsprintf(temp, L"%02X", optional_header->MajorLinkerVersion);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"MajorLinkerVersion");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x1;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 2;
		IT.iSubItem = 0;
		wsprintf(temp, L"%02X", optional_header->MinorLinkerVersion);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"MinorLinkerVersion");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x1;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 3;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfCode);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfCode");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 4;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfInitializedData);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfInitializedData");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 5;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfUninitializedData);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfUninitializedData");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 6;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->AddressOfEntryPoint);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"AddressOfEntryPoint");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 7;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->BaseOfCode);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"BaseOfCode");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 8;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->BaseOfData);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"BaseOfData");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 9;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->ImageBase);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"ImageBase");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 10;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SectionAlignment);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SectionAlignment");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 11;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->FileAlignment);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"FileAlignment");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 12;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->MajorOperatingSystemVersion);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"MajorOperatingSystemVersion");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 13;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->MinorOperatingSystemVersion);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"MinorOperatingSystemVersion");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 14;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->MajorImageVersion);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"MajorImageVersion");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 15;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->MinorImageVersion);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"MinorImageVersion");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 16;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->MajorSubsystemVersion);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"MajorSubsystemVersion");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 17;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->MinorSubsystemVersion);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"MinorSubsystemVersion");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 18;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->Win32VersionValue);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"Win32VersionValue");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 19;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfImage);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfImage");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 20;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfHeaders);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfHeaders");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 21;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->CheckSum);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"CheckSum");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 22;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->Subsystem);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"Subsystem");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 23;
		IT.iSubItem = 0;
		wsprintf(temp, L"%04X", optional_header->DllCharacteristics);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"DllCharacteristics");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x2;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 24;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfStackReserve);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfStackReserve");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 25;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfStackCommit);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfStackCommit");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 26;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfHeapReserve);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfHeapReserve");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 27;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->SizeOfHeapCommit);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"SizeOfHeapCommit");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 28;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->LoaderFlags);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"LoaderFlags");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iItem = 29;
		IT.iSubItem = 0;
		wsprintf(temp, L"%08X", optional_header->NumberOfRvaAndSizes);
		IT.pszText = temp;
		ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 1;
		wsprintf(temp, L"NumberOfRvaAndSizes");
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		IT.iSubItem = 2;
		wsprintf(temp, L"%X", global_RVA);
		global_RVA += 0x4;
		IT.pszText = temp;
		ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

		NumOfTreeitems++;
		//----------------------------------------------------------
		COL.item.pszText = L"IMAGE_DATA_DIRECTORY";
		listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

		listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
			rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

		SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 0;
		LV.pszText = L"DATA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 1;
		LV.pszText = L"DESCRIPTION";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 2;
		LV.pszText = L"VALUE";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

		LV.cx = (rect.right / 2 - 20) / 4;
		LV.iSubItem = 3;
		LV.pszText = L"RVA";
		ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

		for (int i = 0, j = 0; i < 16; i++) {
			IT.iItem = j++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", data_directory[i]->Size);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Size");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			IT.pszText = DATA_DIRECTORY_STR[i];
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 3;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = j++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", data_directory[i]->VirtualAddress);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"VirtualAddress");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 3;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			if (i == IMAGE_DIRECTORY_ENTRY_EXPORT) {
				export_table = (PIMAGE_EXPORT_DIRECTORY)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_IMPORT) {
				import_table.idt = (PIMAGE_IMPORT_DESCRIPTOR)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_RESOURCE) {
				rsrc_section.rsrc_types.rsrc_direc = (PIMAGE_RESOURCE_DIRECTORY)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_SECURITY) {
				certificate_table.certi_table = (LPWIN_CERTIFICATE)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_BASERELOC) {
				reloc.reloca = (PIMAGE_BASE_RELOCATION)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_DEBUG) {
				debug_direc = (PIMAGE_DEBUG_DIRECTORY)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_TLS) {
				tls_table = (PIMAGE_TLS_DIRECTORY)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG) {
				load_cfg_direc = (PIMAGE_LOAD_CONFIG_DIRECTORY)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT && data_directory[11]->VirtualAddress != 0) {
				bound_import = (PIMAGE_BOUND_IMPORT_DESCRIPTOR)(data_directory[i]->VirtualAddress + (DWORD)base_ptr);
				data_directories_present[11] = TRUE;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT) {
				delay_idt.DELAY_IMPORT_DESCRIPTOR = (PImgDelayDescr)data_directory[i]->VirtualAddress;
			}
			else if (i == IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR) {
				cli_header = (PIMAGE_COR20_HEADER)data_directory[i]->VirtualAddress;
			}
		}

		NumOfTreeitems++;

		for (int i = 0; i < (int)file_header->NumberOfSections; i++) {
			COL.hParent = 0;
			COL.item.pszText = L"IMAGE_SECTION_HEADER";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			IT.iItem = 0;
			IT.iSubItem = 0;
			int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCCH)section_header->Name, -1, NULL, NULL);
			MultiByteToWideChar(CP_ACP, 0, (LPCCH)section_header->Name, -1, temp, len);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Name");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x8;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 1;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", section_header->Misc.VirtualSize);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"VirtualSize");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 2;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", section_header->VirtualAddress);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"VirtualAddress");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 3;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", section_header->SizeOfRawData);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"SizeOfRawData");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 4;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", section_header->PointerToRawData);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"PointerToRawData");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 5;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", section_header->PointerToRelocations);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"PointerToRelocations");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 6;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", section_header->PointerToLinenumbers);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"PointerToLinenumbers");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 7;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", section_header->NumberOfRelocations);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"NumberOfRelocations");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 8;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", section_header->NumberOfLinenumbers);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"NumberOfLinenumbers");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = 9;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", section_header->Characteristics);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Characteristics");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", global_RVA);
			global_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			NumOfTreeitems++;

			if (section_header->VirtualAddress <= data_directory[0]->VirtualAddress && data_directory[0]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				export_table = (PIMAGE_EXPORT_DIRECTORY)((DWORD)base_ptr + ((DWORD)export_table - section_header->VirtualAddress + section_header->PointerToRawData));
				data_directories_present[0] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[1]->VirtualAddress && data_directory[1]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				import_table.idt = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)base_ptr + ((DWORD)import_table.idt - section_header->VirtualAddress + section_header->PointerToRawData));
				import_table.VA = (PVOID)section_header->VirtualAddress;
				import_table.P2RAW = (PVOID)section_header->PointerToRawData;
				backup = import_table.idt;
				data_directories_present[1] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[2]->VirtualAddress && data_directory[2]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				rsrc_section.rsrc_types.rsrc_direc = (PIMAGE_RESOURCE_DIRECTORY)((DWORD)base_ptr + ((DWORD)rsrc_section.rsrc_types.rsrc_direc - section_header->VirtualAddress + section_header->PointerToRawData));
				rsrc_section.VA = (PVOID)section_header->VirtualAddress;
				rsrc_section.P2RAW = (PVOID)section_header->PointerToRawData;
				data_directories_present[2] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[4]->VirtualAddress && data_directory[4]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				certificate_table.certi_table = (LPWIN_CERTIFICATE)((DWORD)base_ptr + ((DWORD)certificate_table.certi_table - section_header->VirtualAddress + section_header->PointerToRawData));
				certificate_table.next_list = 0;
				data_directories_present[4] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[5]->VirtualAddress && data_directory[5]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				reloc.reloca = (PIMAGE_BASE_RELOCATION)((DWORD)base_ptr + ((DWORD)reloc.reloca - section_header->VirtualAddress + section_header->PointerToRawData));
				reloc.next_list = 0;
				reloc.word = (PWORD)((DWORD)reloc.reloca + 8);
				data_directories_present[5] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[6]->VirtualAddress && data_directory[6]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				debug_direc = (PIMAGE_DEBUG_DIRECTORY)((DWORD)base_ptr + ((DWORD)debug_direc - section_header->VirtualAddress + section_header->PointerToRawData));
				data_directories_present[6] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[9]->VirtualAddress && data_directory[9]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				tls_table = (PIMAGE_TLS_DIRECTORY)((DWORD)base_ptr + ((DWORD)tls_table - section_header->VirtualAddress + section_header->PointerToRawData));
				data_directories_present[9] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[10]->VirtualAddress && data_directory[10]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				load_cfg_direc = (PIMAGE_LOAD_CONFIG_DIRECTORY)((DWORD)base_ptr + ((DWORD)load_cfg_direc - section_header->VirtualAddress + section_header->PointerToRawData));
				data_directories_present[10] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[12]->VirtualAddress && data_directory[12]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				import_table.VA2 = (PVOID)section_header->VirtualAddress;
				import_table.P2RAW2 = (PVOID)section_header->PointerToRawData;
				data_directories_present[12] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[13]->VirtualAddress && data_directory[13]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				delay_idt.DELAY_IMPORT_DESCRIPTOR = (PImgDelayDescr)((DWORD)base_ptr + ((DWORD)delay_idt.DELAY_IMPORT_DESCRIPTOR - section_header->VirtualAddress + section_header->PointerToRawData));
				delay_idt.VA = (PVOID)section_header->VirtualAddress;
				delay_idt.P2RAW = (PVOID)section_header->PointerToRawData;
				backup2 = delay_idt.DELAY_IMPORT_DESCRIPTOR;
				data_directories_present[13] = TRUE;
			}

			if (section_header->VirtualAddress <= data_directory[14]->VirtualAddress && data_directory[14]->VirtualAddress <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
				cli_header = (PIMAGE_COR20_HEADER)((DWORD)base_ptr + ((DWORD)cli_header - section_header->VirtualAddress + section_header->PointerToRawData));
				data_directories_present[14] = TRUE;
			}

			section_header++;
		}

		section_header = (PIMAGE_SECTION_HEADER)((DWORD)optional_header + (DWORD)file_header->SizeOfOptionalHeader);

		for (int i = 0; i < file_header->NumberOfSections; i++) {
			WCHAR temp2[30] = { 0, };

			COL.hParent = 0;
			int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCCH)(section_header + i)->Name, -1, NULL, NULL);
			MultiByteToWideChar(CP_ACP, 0, (LPCCH)(section_header + i)->Name, -1, temp, len);
			wsprintf(temp2, L"SECTION %s", temp);
			COL.item.pszText = temp2;
			(body_listview + i)->treeitem = TreeView_InsertItem(hTree, &COL);

			(body_listview + i)->hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage((body_listview + i)->hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) - 270;
			LV.iSubItem = 0;
			LV.pszText = L"HEX CODES";
			ListView_InsertColumn((body_listview + i)->hList, 0, &LV);

			LV.cx = 170;
			LV.iSubItem = 1;
			LV.pszText = L"STRINGS";
			ListView_InsertColumn((body_listview + i)->hList, 1, &LV);

			LV.cx = 100;
			LV.iSubItem = 2;
			LV.pszText = L"RVA";
			ListView_InsertColumn((body_listview + i)->hList, 2, &LV);

			(body_listview + i)->Start = (PVOID)(section_header + i)->VirtualAddress;
			(body_listview + i)->End = (PVOID)((section_header + i)->VirtualAddress + (section_header + i)->SizeOfRawData);

			global_RVA = (section_header + i)->VirtualAddress;

			InsertHexcode((PVOID)((DWORD)base_ptr + ((DWORD)(body_listview + i)->Start - (section_header + i)->VirtualAddress + (section_header + i)->PointerToRawData)), (PVOID)((DWORD)base_ptr + ((DWORD)(body_listview + i)->End - (section_header + i)->VirtualAddress + (section_header + i)->PointerToRawData)), (body_listview + i)->hList);
		}

		if (data_directories_present[11] == TRUE) {
			COL.hParent = 0;
			COL.item.pszText = L"BOUND_IMPORT_TABLE";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			local_RVA = data_directory[11]->VirtualAddress;
			itemnumber = 0;

			PIMAGE_BOUND_IMPORT_DESCRIPTOR pBegin = bound_import;
			while (1) {
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", bound_import->OffsetModuleName);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"OffsetModuleName");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 2;
				int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCCH)((DWORD)pBegin + (DWORD)bound_import->OffsetModuleName), -1, NULL, NULL);
				MultiByteToWideChar(CP_ACP, 0, (LPCCH)((DWORD)pBegin + (DWORD)bound_import->OffsetModuleName), -1, temp, len);
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				if (bound_import->OffsetModuleName != 0)
					bound_import++;
				else
					break;
			}
			bound_import = pBegin;

			NumOfTreeitems++;
		}

		if (data_directories_present[1] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[1]->VirtualAddress && data_directory[1]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			COL.item.pszText = L"IMPORT_DIRECTORY_TABLE";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			local_RVA = data_directory[1]->VirtualAddress;

			do {
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", import_table.idt->OriginalFirstThunk);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"OriginalFirstThunk");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", import_table.idt->TimeDateStamp);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"TimeDateStamp");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", import_table.idt->ForwarderChain);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"ForwarderChain");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", import_table.idt->Name);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"Name");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				if (import_table.idt->Name != 0) {
					IT.iSubItem = 2;
					int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCCH)((DWORD)base_ptr + (import_table.idt->Name - (DWORD)import_table.VA + (DWORD)import_table.P2RAW)), -1, NULL, NULL);
					MultiByteToWideChar(CP_ACP, 0, (LPCCH)((DWORD)base_ptr + (import_table.idt->Name - (DWORD)import_table.VA + (DWORD)import_table.P2RAW)), -1, temp, len);
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);
				}

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", import_table.idt->FirstThunk);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"FirstThunk");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				IT.pszText = L"";
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);
			} while (import_table.idt++->FirstThunk != 0);
			import_table.idt = (PIMAGE_IMPORT_DESCRIPTOR)backup;
			NumOfTreeitems++;
		}

		if (data_directories_present[12] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[12]->VirtualAddress && data_directory[12]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}
			COL.item.pszText = L"IMPORT_ADDRESS_TABLE";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			itemnumber = 0;
			local_RVA = import_table.idt->FirstThunk;
			import_table.IMAGE_NAMEADDR_TABLE = ((DWORD)base_ptr + (import_table.idt->FirstThunk - (DWORD)import_table.VA2 + (DWORD)import_table.P2RAW2));

			while (import_table.idt->FirstThunk != 0) {
				WCHAR temp2[50];
				while (*(DWORD *)import_table.IMAGE_NAMEADDR_TABLE != 0) {
					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", *(DWORD *)import_table.IMAGE_NAMEADDR_TABLE);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"RVA");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					import_table.IBN = (PIMAGE_IMPORT_BY_NAME)((DWORD)base_ptr + (*(DWORD *)import_table.IMAGE_NAMEADDR_TABLE - (DWORD)import_table.VA + (DWORD)import_table.P2RAW));

					__try {
						IT.iSubItem = 2;
						int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, import_table.IBN->Name, -1, NULL, NULL);
						MultiByteToWideChar(CP_ACP, 0, import_table.IBN->Name, -1, temp, len);
						wsprintf(temp2, L"%04X   %s", import_table.IBN->Hint, temp);
						IT.pszText = temp2;
						ListView_SetItem(listview[NumOfTreeitems].hList, &IT);
					}
					__except (EXCEPTION_ACCESS_VIOLATION == GetExceptionCode()
						? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_EXECUTION) {
					}

					import_table.IMAGE_NAMEADDR_TABLE += 4;
				}
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", *(DWORD *)import_table.IMAGE_NAMEADDR_TABLE);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"End of Imports");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				IT.pszText = L"";
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				import_table.idt++;
				import_table.IMAGE_NAMEADDR_TABLE = ((DWORD)base_ptr + (import_table.idt->FirstThunk - (DWORD)import_table.VA2 + (DWORD)import_table.P2RAW2));
				local_RVA = import_table.idt->FirstThunk;
			}
			import_table.idt = (PIMAGE_IMPORT_DESCRIPTOR)backup;
			NumOfTreeitems++;
		}

		if (data_directories_present[12] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[12]->VirtualAddress && data_directory[12]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}
			COL.item.pszText = L"IMPORT_NAME_TABLE";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			itemnumber = 0;
			local_RVA = import_table.idt->OriginalFirstThunk;
			import_table.IMAGE_NAMEADDR_TABLE = ((DWORD)base_ptr + (import_table.idt->OriginalFirstThunk - (DWORD)import_table.VA2 + (DWORD)import_table.P2RAW2));

			while (import_table.idt->OriginalFirstThunk != 0) {
				WCHAR temp2[50];
				while (*(DWORD *)import_table.IMAGE_NAMEADDR_TABLE != 0) {
					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", *(DWORD *)import_table.IMAGE_NAMEADDR_TABLE);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"RVA");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					import_table.IBN = (PIMAGE_IMPORT_BY_NAME)((DWORD)base_ptr + (*(DWORD *)import_table.IMAGE_NAMEADDR_TABLE - (DWORD)import_table.VA + (DWORD)import_table.P2RAW));

					__try {
						IT.iSubItem = 2;
						int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, import_table.IBN->Name, -1, NULL, NULL);
						MultiByteToWideChar(CP_ACP, 0, import_table.IBN->Name, -1, temp, len);
						wsprintf(temp2, L"%04X   %s", import_table.IBN->Hint, temp);
						IT.pszText = temp2;
						ListView_SetItem(listview[NumOfTreeitems].hList, &IT);
					}
					__except (EXCEPTION_ACCESS_VIOLATION == GetExceptionCode()
						? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_EXECUTION) {
					}

					import_table.IMAGE_NAMEADDR_TABLE += 4;
				}
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", *(DWORD *)import_table.IMAGE_NAMEADDR_TABLE);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"End of Imports");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				IT.pszText = L"";
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				import_table.idt++;
				import_table.IMAGE_NAMEADDR_TABLE = ((DWORD)base_ptr + (import_table.idt->OriginalFirstThunk - (DWORD)import_table.VA2 + (DWORD)import_table.P2RAW2));
				local_RVA = import_table.idt->OriginalFirstThunk;
			}
			import_table.idt = (PIMAGE_IMPORT_DESCRIPTOR)backup;
			NumOfTreeitems++;
		}

		if (data_directories_present[9] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[9]->VirtualAddress && data_directory[9]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}
			COL.item.pszText = L"IMAGE_TLS_DIRECTORY";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 4;
			LV.iSubItem = 2;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			itemnumber = 0;
			local_RVA = data_directory[9]->VirtualAddress;

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", tls_table->StartAddressOfRawData);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"StartAddressOfRawData");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", tls_table->EndAddressOfRawData);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"EndAddressOfRawData");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", tls_table->AddressOfIndex);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"AddressOfIndex");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", tls_table->AddressOfCallBacks);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"AddressOfCallBacks");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", tls_table->SizeOfZeroFill);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"SizeOfZeroFill");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", tls_table->Characteristics);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Characteristics");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			NumOfTreeitems++;
		}

		if (data_directories_present[6] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[6]->VirtualAddress && data_directory[6]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			COL.item.pszText = L"IMAGE_DEBUG_DIRECTORY";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			itemnumber = 0;
			local_RVA = data_directory[6]->VirtualAddress;

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", debug_direc->Characteristics);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Characteristics");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", debug_direc->TimeDateStamp);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"TimeDateStamp");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", debug_direc->MajorVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MajorVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", debug_direc->MinorVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MinorVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", debug_direc->Type);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Type");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", debug_direc->SizeOfData);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"SizeOfData");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", debug_direc->AddressOfRawData);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"AddressOfRawData");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", debug_direc->PointerToRawData);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"PointerToRawData");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			NumOfTreeitems++;
		}

		if (data_directories_present[10] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[10]->VirtualAddress && data_directory[10]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			COL.item.pszText = L"IMAGE_LOAD_CONFIG_DIRECTORY";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			local_RVA = data_directory[10]->VirtualAddress;
			itemnumber = 0;

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->Size);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Size");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->TimeDateStamp);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"TimeDateStamp");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", load_cfg_direc->MajorVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MajorVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", load_cfg_direc->MinorVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MinorVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->GlobalFlagsClear);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"GlobalFlagsClear");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->GlobalFlagsSet);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"GlobalFlagsSet");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->CriticalSectionDefaultTimeout);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"CriticalSectionDefaultTimeout");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->DeCommitFreeBlockThreshold);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"DeCommitFreeBlockThreshold");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->DeCommitTotalFreeThreshold);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"DeCommitTotalFreeThreshold");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->LockPrefixTable);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"LockPrefixTable");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->MaximumAllocationSize);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MaximumAllocationSize");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->VirtualMemoryThreshold);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"VirtualMemoryThreshold");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->ProcessHeapFlags);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"ProcessHeapFlags");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->ProcessAffinityMask);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"ProcessAffinityMask");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", load_cfg_direc->CSDVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"CSDVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", load_cfg_direc->Reserved1);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Reserved");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->EditList);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"EditList");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->SecurityCookie);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"SecurityCookie");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->SEHandlerTable);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"SEHandlerTable");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", load_cfg_direc->SEHandlerCount);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"SEHandlerCount");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			NumOfTreeitems++;
		}

		if (data_directories_present[0] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[0]->VirtualAddress && data_directory[0]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			COL.item.pszText = L"IMAGE_EXPORT_DIRECTORY";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			local_RVA = data_directory[0]->VirtualAddress;
			itemnumber = 0;

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->Characteristics);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Characteristics");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->TimeDateStamp);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"TimeDateStamp");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", export_table->MajorVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MajorVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", export_table->MinorVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MinorVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->Name);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Name RVA");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->Base);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Ordinal Base");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->NumberOfFunctions);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"NumberOfFunctions");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->NumberOfNames);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"NumberOfNames");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->AddressOfFunctions);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"AddressOfFunctions");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->AddressOfNames);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"AddressOfNames");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", export_table->AddressOfNameOrdinals);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"AddressOfNameOrdinals");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			NumOfTreeitems++;
		}

		if (data_directories_present[14] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[14]->VirtualAddress && data_directory[14]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			COL.item.pszText = L"IMAGE_CLI_HEADER";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 2 - 200;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 2 + 200;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			local_RVA = data_directory[14]->VirtualAddress;

			itemnumber = 0;

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", cli_header->cb);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"cb");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 2;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			NumOfTreeitems++;
		}

		if (data_directories_present[4] == TRUE) {
			COL.hParent = 0;

			COL.item.pszText = L"CERTIFICATE_TABLE";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			local_RVA = data_directory[4]->VirtualAddress;
			itemnumber = 0;

			if (certificate_table.certi_table->wRevision != 0) {
				temp_next = &certificate_table;
				temp_prev = (PVOID)-1;

				while (1) {
					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", ((CERTIFICATE_TABLE *)temp_next)->certi_table->dwLength);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"dwLength");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 2;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", ((CERTIFICATE_TABLE *)temp_next)->certi_table->wRevision);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"wRevision");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 2;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 2;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", ((CERTIFICATE_TABLE *)temp_next)->certi_table->wCertificateType);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"wCertificateType");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 2;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 2;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", ((CERTIFICATE_TABLE *)temp_next)->certi_table->bCertificate);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"bCertificate");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 2;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 1;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					IT.pszText = L"";
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					temp_prev = temp_next;
					temp_next = (CERTIFICATE_TABLE *)malloc(sizeof(CERTIFICATE_TABLE));
					((CERTIFICATE_TABLE *)temp_prev)->next_list = (CERTIFICATE_TABLE *)temp_next;
					((CERTIFICATE_TABLE *)temp_next)->certi_table = ((CERTIFICATE_TABLE *)temp_prev)->certi_table + 1;
					((CERTIFICATE_TABLE *)temp_next)->next_list = NULL;
					if (((CERTIFICATE_TABLE *)temp_prev)->certi_table->wRevision == 0) {
						break;
					}
				}
			}

			NumOfTreeitems++;
		}

		if (data_directories_present[13] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[13]->VirtualAddress && data_directory[13]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			COL.item.pszText = L"DELAY_IMPORT_DESCRIPTORS";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			local_RVA = data_directory[13]->VirtualAddress;
			itemnumber = 0;

			do {
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", delay_idt.DELAY_IMPORT_DESCRIPTOR->grAttrs);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"Attributes");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaDLLName);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"rvaDLLName");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				if (delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaDLLName != 0) {
					IT.iSubItem = 2;
					int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (char *)((DWORD)base_ptr + (delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaDLLName - (DWORD)delay_idt.VA + (DWORD)delay_idt.P2RAW)), -1, NULL, NULL);
					MultiByteToWideChar(CP_ACP, 0, (char *)((DWORD)base_ptr + (delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaDLLName - (DWORD)delay_idt.VA + (DWORD)delay_idt.P2RAW)), -1, temp, len);
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);
				}
				else {}

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaHmod);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"HMODULE");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaIAT);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"rvaIAT");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaINT);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"rvaINT");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaBoundIAT);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"rvaBoundIAT");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaUnloadIAT);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"rvaUnloadIAT");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", delay_idt.DELAY_IMPORT_DESCRIPTOR->dwTimeStamp);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"dwTimeStamp");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				IT.pszText = L"";
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);
			} while (delay_idt.DELAY_IMPORT_DESCRIPTOR++->rvaIAT != 0);

			NumOfTreeitems++;

			delay_idt.DELAY_IMPORT_DESCRIPTOR = (PImgDelayDescr)backup2;

			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaIAT && delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaIAT <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			COL.item.pszText = L"DELAY_IMPORT_ADDRESS_TABLE";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			local_RVA = delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaIAT;

			section_header = (PIMAGE_SECTION_HEADER)((DWORD)optional_header + (DWORD)file_header->SizeOfOptionalHeader);
			for (int i = 0; i < (int)file_header->NumberOfSections; i++) {
				if (section_header->VirtualAddress <= (DWORD)delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaIAT && (DWORD)delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaIAT <= (section_header->VirtualAddress + section_header->Misc.VirtualSize)) {
					delay_idt.VA2 = (PVOID)section_header->VirtualAddress;
					delay_idt.P2RAW2 = (PVOID)section_header->PointerToRawData;
				}
				section_header++;
			}

			itemnumber = 0;

			while (delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaIAT != 0) {
				DWORD NAME_TABLE = ((DWORD)base_ptr + ((DWORD)delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaINT - (DWORD)delay_idt.VA + (DWORD)delay_idt.P2RAW));
				delay_idt.IMAGE_NAMEADDR_TABLE = ((DWORD)base_ptr + ((DWORD)delay_idt.DELAY_IMPORT_DESCRIPTOR->rvaIAT - (DWORD)delay_idt.VA2 + (DWORD)delay_idt.P2RAW2));

				while (*(DWORD *)delay_idt.IMAGE_NAMEADDR_TABLE != 0) {
					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", *(DWORD *)delay_idt.IMAGE_NAMEADDR_TABLE);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"RVA");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					delay_idt.IBN = (PIMAGE_IMPORT_BY_NAME)((DWORD)base_ptr + (*(DWORD *)NAME_TABLE - (DWORD)delay_idt.VA + (DWORD)delay_idt.P2RAW));

					__try {
						WCHAR temp2[50] = { 0, };

						IT.iSubItem = 2;
						int len = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, delay_idt.IBN->Name, -1, NULL, NULL);
						MultiByteToWideChar(CP_ACP, 0, delay_idt.IBN->Name, -1, temp, len);
						wsprintf(temp2, L"%04X   %s", delay_idt.IBN->Hint, temp);
						IT.pszText = temp2;
						ListView_SetItem(listview[NumOfTreeitems].hList, &IT);
					}
					__except (EXCEPTION_ACCESS_VIOLATION == GetExceptionCode()
						? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_EXECUTION) {
					}

					delay_idt.IMAGE_NAMEADDR_TABLE += 4;
					(DWORD)NAME_TABLE += 4;
				}

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", *(DWORD *)delay_idt.IMAGE_NAMEADDR_TABLE);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"RVA");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				IT.pszText = L"";
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				delay_idt.DELAY_IMPORT_DESCRIPTOR++;
			}

			NumOfTreeitems++;
		}

		if (data_directories_present[5] == TRUE) {
			temp_next = &reloc;
			temp_prev = 0;

			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[5]->VirtualAddress && data_directory[5]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			itemnumber = 0;

			COL.item.pszText = L"IMAGE_BASE_RELOCATION";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			local_RVA = data_directory[5]->VirtualAddress;

			while (1) {
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", ((RELOCATION *)temp_next)->reloca->VirtualAddress);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"RvaOfBlock");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", ((RELOCATION *)temp_next)->reloca->SizeOfBlock);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"SizeOfBlock");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				for (int i = 0; i < (int)(((RELOCATION *)temp_next)->reloca->SizeOfBlock / sizeof(WORD) - 4); i++) {
					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", *((RELOCATION *)temp_next)->word);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"TypeRVA");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x2;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					((RELOCATION *)temp_next)->word++;
				}

				if (((RELOCATION *)temp_next)->reloca->SizeOfBlock != 0) {
					temp_prev = temp_next;
					temp_next = (RELOCATION *)malloc(sizeof(RELOCATION));
					((RELOCATION *)temp_prev)->next_list = (RELOCATION *)(temp_next);
					((RELOCATION *)temp_next)->reloca = (PIMAGE_BASE_RELOCATION)(((RELOCATION *)temp_prev)->word);
					((RELOCATION *)temp_next)->word = (PWORD)(((DWORD)((RELOCATION *)temp_next)->reloca) + 8);
					((RELOCATION *)temp_next)->next_list = NULL;

					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					IT.pszText = L"";
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);
				}

				if (((RELOCATION *)temp_next)->reloca->SizeOfBlock == 0)
					break;
			}
			NumOfTreeitems++;
		}

		if (data_directories_present[2] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[2]->VirtualAddress && data_directory[2]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			itemnumber = 0;

			COL.item.pszText = L"IMAGE_RESOURCE_DIRECTORY TYPE";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			local_RVA = data_directory[2]->VirtualAddress;

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", rsrc_section.rsrc_types.rsrc_direc->Characteristics);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"Characteristics");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 3;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%08X", rsrc_section.rsrc_types.rsrc_direc->TimeDateStamp);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"TimeDateStamp");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 3;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x4;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", rsrc_section.rsrc_types.rsrc_direc->MajorVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MajorVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 3;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", rsrc_section.rsrc_types.rsrc_direc->MinorVersion);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"MinorVersion");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 3;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", rsrc_section.rsrc_types.rsrc_direc->NumberOfNamedEntries);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"NumberOfNamedEntries");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 3;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iItem = itemnumber++;
			IT.iSubItem = 0;
			wsprintf(temp, L"%04X", rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries);
			IT.pszText = temp;
			ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 1;
			wsprintf(temp, L"NumberOfIdEntries");
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			IT.iSubItem = 3;
			wsprintf(temp, L"%X", local_RVA);
			local_RVA += 0x2;
			IT.pszText = temp;
			ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

			if (rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries != 0 || rsrc_section.rsrc_types.rsrc_direc->NumberOfNamedEntries != 0) {
				rsrc_section.rsrc_types.rsrc_direc_ents = (PIMAGE_RESOURCE_DIRECTORY_ENTRY *)malloc(sizeof(PIMAGE_RESOURCE_DIRECTORY_ENTRY)*(rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_types.rsrc_direc->NumberOfNamedEntries));
				rsrc_section.rsrc_types.rsrc_direc_ents[0] = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(rsrc_section.rsrc_types.rsrc_direc + 1);

				for (int i = 0; i < (rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_types.rsrc_direc->NumberOfNamedEntries); i++) {
					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", rsrc_section.rsrc_types.rsrc_direc_ents[i]->Id);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"Id");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", rsrc_section.rsrc_types.rsrc_direc_ents[i]->OffsetToDirectory);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"OffsetToDirectory");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					if (i != (rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_types.rsrc_direc->NumberOfNamedEntries - 1))
						rsrc_section.rsrc_types.rsrc_direc_ents[i + 1] = rsrc_section.rsrc_types.rsrc_direc_ents[i] + 1;
				}
			}
			NumOfTreeitems++;
		}

		rsrc_section.numofnameid = 0;
		if (data_directories_present[2] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[2]->VirtualAddress && data_directory[2]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			itemnumber = 0;

			COL.item.pszText = L"IMAGE_RESOURCE_DIRECTORY NameID";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			rsrc_section.rsrc_nameid = (IMAGE_RESOURCE_DIRECTORY_LEVEL *)malloc(sizeof(IMAGE_RESOURCE_DIRECTORY_LEVEL) * rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries);

			for (int i = 0; i < rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries; i++) {
				rsrc_section.rsrc_nameid[i].rsrc_direc = (PIMAGE_RESOURCE_DIRECTORY)((DWORD)rsrc_section.rsrc_types.rsrc_direc + (DWORD)rsrc_section.rsrc_types.rsrc_direc_ents[i]->OffsetToDirectory);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", rsrc_section.rsrc_nameid[i].rsrc_direc->Characteristics);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"Characteristics");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", rsrc_section.rsrc_nameid[i].rsrc_direc->TimeDateStamp);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"TimeDateStamp");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%04X", rsrc_section.rsrc_nameid[i].rsrc_direc->MajorVersion);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"MajorVersion");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x2;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%04X", rsrc_section.rsrc_nameid[i].rsrc_direc->MinorVersion);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"MinorVersion");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x2;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%04X", rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfNamedEntries);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"NumberOfNamedEntries");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x2;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%04X", rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfIdEntries);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"NumberOfIdEntries");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x2;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				rsrc_section.rsrc_nameid[i].rsrc_direc_ents = (PIMAGE_RESOURCE_DIRECTORY_ENTRY *)malloc(sizeof(PIMAGE_RESOURCE_DIRECTORY_ENTRY)*(rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfNamedEntries));
				rsrc_section.rsrc_nameid[i].rsrc_direc_ents[0] = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(rsrc_section.rsrc_nameid[i].rsrc_direc + 1);

				for (int j = 0; j < (rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfNamedEntries); j++) {
					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", rsrc_section.rsrc_nameid[i].rsrc_direc_ents[j]->Id);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"Id");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", rsrc_section.rsrc_nameid[i].rsrc_direc_ents[j]->OffsetToDirectory);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"OffsetToDirectory");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					rsrc_section.numofnameid++;
					if (j != (rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfNamedEntries - 1))
						rsrc_section.rsrc_nameid[i].rsrc_direc_ents[j + 1] = rsrc_section.rsrc_nameid[i].rsrc_direc_ents[j] + 1;
				}

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				IT.pszText = L"";
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);
			}
			NumOfTreeitems++;
		}

		rsrc_section.numofdata = 0;
		if (data_directories_present[2] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[2]->VirtualAddress && data_directory[2]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			int nameid = 0;
			itemnumber = 0;

			COL.item.pszText = L"IMAGE_RESOURCE_DIRECTORY LANGUAGE";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			rsrc_section.rsrc_language = (IMAGE_RESOURCE_DIRECTORY_LEVEL *)malloc(sizeof(IMAGE_RESOURCE_DIRECTORY_LEVEL) * rsrc_section.numofnameid);

			for (int i = 0; i < rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries; i++)
				for (int j = 0; j < (rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_nameid[i].rsrc_direc->NumberOfNamedEntries); j++)
					rsrc_section.rsrc_language[nameid++].rsrc_direc = (PIMAGE_RESOURCE_DIRECTORY)((DWORD)rsrc_section.rsrc_types.rsrc_direc + (DWORD)rsrc_section.rsrc_nameid[i].rsrc_direc_ents[j]->OffsetToDirectory);

			for (int i = 0; i < rsrc_section.numofnameid; i++) {
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", rsrc_section.rsrc_language[i].rsrc_direc->Characteristics);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"Characteristics");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", rsrc_section.rsrc_language[i].rsrc_direc->TimeDateStamp);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"TimeDateStamp");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%04X", rsrc_section.rsrc_language[i].rsrc_direc->MajorVersion);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"MajorVersion");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x2;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%04X", rsrc_section.rsrc_language[i].rsrc_direc->MinorVersion);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"MinorVersion");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x2;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%04X", rsrc_section.rsrc_language[i].rsrc_direc->NumberOfNamedEntries);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"NumberOfNamedEntries");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x2;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%04X", rsrc_section.rsrc_language[i].rsrc_direc->NumberOfIdEntries);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"NumberOfIdEntries");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x2;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				rsrc_section.rsrc_language[i].rsrc_direc_ents = (PIMAGE_RESOURCE_DIRECTORY_ENTRY *)malloc(sizeof(PIMAGE_RESOURCE_DIRECTORY_ENTRY)*rsrc_section.rsrc_language[i].rsrc_direc->NumberOfIdEntries);
				rsrc_section.rsrc_language[i].rsrc_direc_ents[0] = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(rsrc_section.rsrc_language[i].rsrc_direc + 1);

				for (int j = 0; j < (rsrc_section.rsrc_language[i].rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_language[i].rsrc_direc->NumberOfNamedEntries); j++) {
					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", rsrc_section.rsrc_language[i].rsrc_direc_ents[j]->Id);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"Id");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iItem = itemnumber++;
					IT.iSubItem = 0;
					wsprintf(temp, L"%08X", rsrc_section.rsrc_language[i].rsrc_direc_ents[j]->OffsetToData);
					IT.pszText = temp;
					ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 1;
					wsprintf(temp, L"OffsetToData");
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					IT.iSubItem = 3;
					wsprintf(temp, L"%X", local_RVA);
					local_RVA += 0x4;
					IT.pszText = temp;
					ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

					rsrc_section.numofdata++;
					if (j != (rsrc_section.rsrc_language[i].rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_language[i].rsrc_direc->NumberOfNamedEntries - 1))
						rsrc_section.rsrc_language[i].rsrc_direc_ents[j + 1] = rsrc_section.rsrc_language[i].rsrc_direc_ents[j] + 1;
				}
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				IT.pszText = L"";
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);
			}
			NumOfTreeitems++;
		}

		if (data_directories_present[2] == TRUE) {
			for (int i = 0; i < file_header->NumberOfSections; i++) {
				if ((DWORD)(body_listview + i)->Start <= data_directory[2]->VirtualAddress && data_directory[2]->VirtualAddress <= (DWORD)(body_listview + i)->End) {
					COL.hParent = body_listview[i].treeitem;
				}
			}

			int data = 0;
			itemnumber = 0;

			COL.item.pszText = L"IMAGE_RESOURCE_DATA_ENTRY";
			listview[NumOfTreeitems].treeitem = TreeView_InsertItem(hTree, &COL);

			listview[NumOfTreeitems].hList = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME, WC_LISTVIEW, NULL, WS_CHILD | WS_BORDER | LVS_REPORT | LVS_NOSORTHEADER,
				rect.right / 2 + 10, rect.top + 50, rect.right / 2 - 20, rect.bottom - 70, hWnd, NULL, hInst, NULL);

			SendMessage(listview[NumOfTreeitems].hList, WM_SETFONT, (WPARAM)hFont, (LPARAM)false);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 0;
			LV.pszText = L"DATA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 0, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 1;
			LV.pszText = L"DESCRIPTION";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 1, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 2;
			LV.pszText = L"VALUE";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 2, &LV);

			LV.cx = (rect.right / 2 - 20) / 3;
			LV.iSubItem = 3;
			LV.pszText = L"RVA";
			ListView_InsertColumn(listview[NumOfTreeitems].hList, 3, &LV);

			rsrc_section.rsrc_data = (PIMAGE_RESOURCE_DATA_ENTRY *)malloc(sizeof(PIMAGE_RESOURCE_DATA_ENTRY)*rsrc_section.numofdata);

			for (int i = 0; i < rsrc_section.numofnameid; i++)
				for (int j = 0; j < (rsrc_section.rsrc_language[i].rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_language[i].rsrc_direc->NumberOfNamedEntries); j++)
					rsrc_section.rsrc_data[data++] = (PIMAGE_RESOURCE_DATA_ENTRY)((DWORD)rsrc_section.rsrc_types.rsrc_direc + (DWORD)rsrc_section.rsrc_language[i].rsrc_direc_ents[j]->OffsetToData);

			for (int i = 0; i < rsrc_section.numofdata; i++) {
				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", rsrc_section.rsrc_data[i]->OffsetToData);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"OffsetToData");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", rsrc_section.rsrc_data[i]->Size);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"Size");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", rsrc_section.rsrc_data[i]->CodePage);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"CodePage");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				wsprintf(temp, L"%08X", rsrc_section.rsrc_data[i]->Reserved);
				IT.pszText = temp;
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 1;
				wsprintf(temp, L"Reserved");
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iSubItem = 3;
				wsprintf(temp, L"%X", local_RVA);
				local_RVA += 0x4;
				IT.pszText = temp;
				ListView_SetItem(listview[NumOfTreeitems].hList, &IT);

				IT.iItem = itemnumber++;
				IT.iSubItem = 0;
				IT.pszText = L"";
				ListView_InsertItem(listview[NumOfTreeitems].hList, &IT);
			}
			NumOfTreeitems++;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		MessageBox(hWnd, L"경고! 파일이 패킹되어 있을 수 있습니다.\n프로그램을 종료합니다.", L"파일 로드 실패", MB_OK);
		PostQuitMessage(0);
	}
}

VOID RemoveValues(HWND hWnd) {
	__try {
		if (data_directories_present[2] == TRUE) {
			free(rsrc_section.rsrc_data);
			for (int i = 0; i < rsrc_section.numofnameid; i++) {
				free(rsrc_section.rsrc_language[i].rsrc_direc_ents);
			}
			free(rsrc_section.rsrc_language);

			for (int i = 0; i < (rsrc_section.rsrc_types.rsrc_direc->NumberOfIdEntries + rsrc_section.rsrc_types.rsrc_direc->NumberOfNamedEntries); i++) {
				free(rsrc_section.rsrc_nameid[i].rsrc_direc_ents);
			}

			free(rsrc_section.rsrc_nameid);

			free(rsrc_section.rsrc_types.rsrc_direc_ents);
		}

		if (data_directories_present[4] == TRUE) {
			temp_prev = certificate_table.next_list;
			temp_next = certificate_table.next_list->next_list;

			while (temp_next != NULL) {
				free(temp_prev);
				temp_prev = temp_next;
				temp_next = ((CERTIFICATE_TABLE *)temp_prev)->next_list;
			}
		}

		if (data_directories_present[5] == TRUE) {
			temp_prev = reloc.next_list;
			temp_next = reloc.next_list->next_list;

			while (temp_next != NULL) {
				free(temp_prev);
				temp_prev = temp_next;
				temp_next = ((RELOCATION *)temp_prev)->next_list;
			}
		}

		for (int i = 0; i < 16; i++) {
			data_directories_present[i] = FALSE;
		}

		for (int i = 0; i < 50 && listview[i].hList != NULL; i++) {
			DestroyWindow(listview[i].hList);
		}

		for (int i = 0; body_listview != NULL && i < file_header->NumberOfSections; i++) {
			DestroyWindow(body_listview[i].hList);
		}

		if (body_listview != NULL) {
			free(body_listview);
			DestroyWindow(hTree);
			UnmapViewOfFile(base_ptr);
			CloseHandle(hFile);
			CloseHandle(hMap);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
	memset(listview, NULL, sizeof(listview));
	hTree = NULL;
	body_listview = NULL;

	global_RVA = 0;
	local_RVA = 0;

	InvalidateRgn(hWnd, NULL, TRUE);
}

VOID InsertHexcode(PVOID Start, PVOID End, HWND hWndOfListview) {
	PBYTE read = (PBYTE)Start;
	WCHAR str[17] = { 0, };
	WCHAR hax[50] = { 0, };
	WCHAR RVA[10] = { 0, };

	int i, itemnumber = 0;
	LVITEM IT;
	IT.mask = LVIF_TEXT;

	while (read < (PBYTE)End) {
		for (i = 0; i <= 7 && read < (PBYTE)End; i++) {
			if (33 <= *read && *read <= 126) {
				str[i] = *read;
				wsprintf(hax, L"%s%02X", hax, *read);
				wsprintf(hax, L"%s ", hax);
			}
			else {
				str[i] = '.';
				wsprintf(hax, L"%s%02X", hax, *read);
				wsprintf(hax, L"%s ", hax);
			}
			read++;
		}

		wsprintf(hax, L"%s ", hax);

		for (i = 8; i <= 15 && read < (PBYTE)End; i++) {
			if (33 <= *read && *read <= 126) {
				str[i] = *read;
				wsprintf(hax, L"%s%02X", hax, *read);
				wsprintf(hax, L"%s ", hax);
			}
			else {
				str[i] = '.';
				wsprintf(hax, L"%s%02X", hax, *read);
				wsprintf(hax, L"%s ", hax);
			}
			read++;
		}

		IT.iItem = itemnumber++;
		IT.iSubItem = 0;
		IT.pszText = hax;
		ListView_InsertItem(hWndOfListview, &IT);

		IT.iSubItem = 1;
		IT.pszText = str;
		ListView_SetItem(hWndOfListview, &IT);

		IT.iSubItem = 2;
		wsprintf(RVA, L"%X", global_RVA);
		global_RVA += i;
		IT.pszText = RVA;
		ListView_SetItem(hWndOfListview, &IT);

		memset(str, 0, sizeof(str));
		memset(hax, 0, sizeof(hax));

		i = 0;
	}
}