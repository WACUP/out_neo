#include <windows.h>
#include <commctrl.h>
#include <tchar.h>
#include "tab.h"
#include "../out.h"
#include <loader/loader/utils.h>
#include "../resource.h"
#include "../outMixer.h"

extern outMixer *g_pMixer;

INT_PTR CALLBACK TabSheet::DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TabSheet *dlg;
	if (uMsg == WM_INITDIALOG)
	{
		DarkModeSetup(hwnd);

		SetWindowLongPtr(hwnd, DWLP_USER, lParam);

		dlg = (TabSheet *)lParam;
		if (!dlg) return TRUE;
		dlg->hwnd = hwnd;

		if (!UXThemeFunc(IPC_ISWINTHEMEPRESENT))
		{
			UXThemeFunc((WPARAM)hwnd);
		}
	}
	else if (uMsg == WM_DESTROY)
	{
		HWND output_plugin_list = GetDlgItem(hwnd, IDC_CMB_OUTPUT);
		if (IsWindow(output_plugin_list))
		{
			const int count = (int)SendMessage(output_plugin_list, CB_GETCOUNT, 0, 0);
			for (int i = 0; i < count; i++)
			{
				SafeFree((void*)SendMessage(output_plugin_list,
										CB_GETITEMDATA, i, 0));
			}
		}
	}

	dlg = (TabSheet *)GetWindowLongPtr(hwnd, DWLP_USER);
	if (!dlg) return FALSE;

	return dlg->message(hwnd, uMsg, wParam, lParam);
}

TabSheet::TabSheet(UINT _dlg_res)
{
	hwnd = 0;
	parent = 0;
	dlg_res = _dlg_res;
}

TabSheet::~TabSheet()
{
	destroy_dlg();
}

HWND TabSheet::create_dlg(HWND _parent)
{
	destroy_dlg();
	parent = _parent;
	hwnd = CreateDialogParamW( g_OutModMaster.hDllInstance, MAKEINTRESOURCEW(dlg_res), parent, DialogProc, (LONG_PTR)this );
	return hwnd;
}

void TabSheet::destroy_dlg()
{
	if (IsWindow(hwnd))
	{
		DestroyWindow(hwnd);
		hwnd = 0;
	}

	parent = 0;
}

void TabSheet::switch_on()
{
	if (IsWindow(hwnd)) ShowWindow(hwnd, SW_SHOWNA);
}

void TabSheet::switch_off()
{
	if (IsWindow(hwnd)) ShowWindow(hwnd, SW_HIDE);
}

BOOL TabSheet::message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_COMMAND)
	{
		command(LOWORD(wParam), HIWORD(wParam));
		return TRUE;
	}
	return FALSE;
}

/*INT_PTR CALLBACK TabDlg::DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TabDlg *dlg = NULL;
	if (uMsg == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);

		dlg = (TabDlg *)lParam;
		if (!dlg) return TRUE;
		dlg->hwnd = hwnd;
	}

	if (!dlg) dlg = (TabDlg*)GetWindowLongPtr(hwnd, DWLP_USER);
	return (dlg ? dlg->message(hwnd, uMsg, wParam, lParam) : FALSE);
}*/

TabDlg::TabDlg(HWND _parent) 
{
	hwnd = 0;
	parent = _parent;

	tab_ctl = 0;
	active_page = 0;
	page_count = 0;

	memset(pages, 0, sizeof(pages));
	memset(titles, 0, sizeof(titles));
}

TabDlg::~TabDlg()
{
	for (int i = 0; i < page_count; i++)
	{
		if (pages[i])
		{		
			delete pages[i];
			pages[i] = NULL;
		}

		if (titles[i])
		{
			free(titles[i]);
			titles[i] = NULL;
		}
	}
}

void TabDlg::init()
{
#if 1
	hwnd = parent;
	SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)this);
	tab_ctl = GetDlgItem(hwnd, IDC_TABBED_PREFS_TAB);
#else
	tab_ctl = CreateWindow(WC_TABCONTROL, TEXT(""), WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
						   0, 0, 100, 100, hwnd, (HMENU)534, g_OutModMaster.hDllInstance, 0);
#endif
	if (!IsWindow(tab_ctl)) return;

	//SendMessage(tab_ctl, WM_SETFONT, (WPARAM)SendMessage(hwnd, WM_GETFONT, 0, 0), MAKELPARAM(1, 0));

	int i;
	TCITEM tie = {0};
	RECT dlg_rect = {0}/*, tab_rect = {0}*/;
	tie.mask = TCIF_TEXT; 

	for (i = 0; i < page_count; i++)
	{
		pages[i]->create_dlg(hwnd);
		tie.pszText = titles[i];
		TabCtrl_InsertItem(tab_ctl, i, &tie); 

		/*GetWindowRect(pages[i]->hwnd, &dlg_rect);
		if (dlg_rect.right - dlg_rect.left > tab_rect.right)  tab_rect.right  = dlg_rect.right - dlg_rect.left;
		if (dlg_rect.bottom - dlg_rect.top > tab_rect.bottom) tab_rect.bottom = dlg_rect.bottom - dlg_rect.top;*/
	}

	/*DWORD base_units = GetDialogBaseUnits(); 
	int cx_margin = LOWORD(base_units) / 4; 
	int cy_margin = HIWORD(base_units) / 8;

	TabCtrl_AdjustRect(tab_ctl, TRUE, &tab_rect);
	OffsetRect(&tab_rect, cx_margin - tab_rect.left, cy_margin - tab_rect.top); 
	memcpy(&dlg_rect, &tab_rect, sizeof(RECT));
	TabCtrl_AdjustRect(tab_ctl, FALSE, &dlg_rect);*/

	GetClientRect(tab_ctl, &dlg_rect);
	TabCtrl_AdjustRect(tab_ctl, FALSE, &dlg_rect);

	// Adjust tab control
	/*SetWindowPos(tab_ctl, 0, tab_rect.left, tab_rect.top, 
				 tab_rect.right - tab_rect.left, tab_rect.bottom - tab_rect.top, 
				 SWP_NOZORDER);*/

	for (i = 0; i < page_count; i++)
		SetWindowPos(pages[i]->hwnd, HWND_BOTTOM, dlg_rect.left,
					 dlg_rect.top, 0, 0, (i == active_page ?
					 SWP_SHOWWINDOW : SWP_HIDEWINDOW) |
					 SWP_NOACTIVATE | SWP_NOSIZE);

	TabCtrl_SetCurSel(tab_ctl, active_page);
	if (page_count) pages[active_page]->switch_on();

	SetWindowPos(parent,tab_ctl/*HWND_TOP*/, dlg_rect.left, dlg_rect.top,
				 0, 0, SWP_NOSIZE | SWP_HIDEWINDOW | SWP_NOACTIVATE);
}

void TabDlg::exec()
{
	// TODO need to account for this being WACUPified
	//		whilst leaving the old code doing cleanup
#if 1
	init();
#else
	DialogBoxParam(g_OutModMaster.hDllInstance,
				   MAKEINTRESOURCE(IDD_TABDLG),
				   parent, DialogProc, (LPARAM)this);
	for (int i = 0; i < page_count; i++)
		pages[i]->destroy_dlg();
	hwnd = 0;
#endif
}

void TabDlg::add_page(int i, TabSheet *sheet, const TCHAR *title)
{
	if (IsWindow(hwnd)) 
	{
		// Cannot add pages to live tab control
		delete sheet;
		return;
	}

	if (i > page_count || i < 0) 
		i = page_count;

	pages[i] = sheet;
	titles[i] = _tcsdup(title);
	page_count++;
}

void TabDlg::switch_to(int page)
{
	if (page >= page_count || page < 0) return;
	if (IsWindow(hwnd))
	{
		pages[active_page]->switch_off();
		pages[page]->switch_on();
	}
	active_page = page;
}

BOOL TabDlg::message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			init();

			DarkModeSetup(hwnd);
			break;
		}
		/*case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDOK:
				{
					EndDialog(hwnd, IDOK);
					break;
				}
				case IDCANCEL:
				{
					EndDialog(hwnd, IDCANCEL);
					break;
				}
			}
			break;
		}*/
		case WM_NOTIFY:
		{
			if (((NMHDR*)lParam)->code == TCN_SELCHANGE) 
			{
				const int page = TabCtrl_GetCurSel(tab_ctl);
				switch_to(page);
				g_pMixer->m_pConfig->Write(TEXT("iLastPrefs"), page);
				break;
			}
			break;
		}
		case WM_DESTROY:
		{
			for (int i = 0; i < page_count; i++)
				pages[i]->destroy_dlg();
			hwnd = 0;
			TabDlg* dlg = (TabDlg*)GetWindowLongPtr(hwnd, DWLP_USER);
			if (dlg)
			{
				SetWindowLongPtr(hwnd, DWLP_USER, 0);
				delete dlg;
			}
			return FALSE;
		}
		default:
		{
			return FALSE;
		}
	}
	return TRUE;
}