#include <windows.h>
#include <commctrl.h>
#include "tab.h"


INT_PTR CALLBACK
TabSheet::DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  TabSheet *dlg;
  if (uMsg == WM_INITDIALOG)
  {
    SetWindowLong(hwnd, DWL_USER, lParam);

    dlg = (TabSheet *)lParam;
    if (!dlg) return TRUE;
    dlg->hwnd = hwnd;
  }

  dlg = (TabSheet *)GetWindowLong(hwnd, DWL_USER);
  if (!dlg) return FALSE;

  return dlg->message(hwnd, uMsg, wParam, lParam);
}

TabSheet::TabSheet(HMODULE hmodule, LPCSTR dlg_res) 
{
  HRSRC   hrsrc = FindResource(hmodule, dlg_res, RT_DIALOG); 
  HGLOBAL hglb  = LoadResource(hmodule, hrsrc); 
  dlg_template = (DLGTEMPLATE *) LockResource(hglb);
  
  hwnd = 0;
  parent = 0;
  hinstance = 0;
}

TabSheet::~TabSheet()
{
  destroy_dlg();
}

HWND 
TabSheet::create_dlg(HINSTANCE _hinstance, HWND _parent)
{
  destroy_dlg();
  hinstance = _hinstance;
  parent = _parent;
  hwnd = CreateDialogIndirectParam(hinstance, dlg_template, parent, DialogProc, (LONG)this); 
  return hwnd;
}

void
TabSheet::destroy_dlg()
{
  if (!hwnd) return;

  DestroyWindow(hwnd);

  hwnd = 0;
  parent = 0;
  hinstance = 0;
}

void TabSheet::switch_on()
{
  if (hwnd) ShowWindow(hwnd, SW_SHOW);
}

void TabSheet::switch_off()
{
  if (hwnd) ShowWindow(hwnd, SW_HIDE);
}


BOOL 
TabSheet::message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      init();
      init_controls();
      return TRUE;

    case WM_COMMAND:
      command(LOWORD(wParam), HIWORD(wParam));
      return TRUE;
  }
  return FALSE;
}









INT_PTR CALLBACK
TabDlg::DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  TabDlg *dlg;
  if (uMsg == WM_INITDIALOG)
  {
    SetWindowLong(hwnd, DWL_USER, lParam);

    dlg = (TabDlg *)lParam;
    if (!dlg) return TRUE;
    dlg->hwnd = hwnd;
  }

  dlg = (TabDlg*)GetWindowLong(hwnd, DWL_USER);
  if (!dlg) return FALSE;

  return dlg->message(hwnd, uMsg, wParam, lParam);
}

TabDlg::TabDlg(HINSTANCE _hinstance, LPCSTR _dlg_res, HWND _parent) 
{
  hwnd = 0;
  parent = _parent;
  dlg_res = _dlg_res;
  hinstance = _hinstance;

  tab_ctl = 0;
  active_page = 0;
  page_count = 0;
}

TabDlg::~TabDlg()
{
  for (int i = 0; i < page_count; i++)
  {
    if (pages[i])  delete pages[i];
    if (titles[i]) delete titles[i];
  }
}

void 
TabDlg::init()
{
  InitCommonControls();

  tab_ctl = CreateWindow(WC_TABCONTROL, "", 
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 
        0, 0, 100, 100, hwnd, (HMENU)534, hinstance, 0); 
  if (!tab_ctl) return;

  int i;
  TCITEM tie;
  RECT dlg_rect, tab_rect;
//  const max_str = 255;
//  char str[max_str];

  memset(&tie, 0, sizeof(tie));
  memset(&dlg_rect, 0, sizeof(dlg_rect));
  memset(&tab_rect, 0, sizeof(tab_rect));
  tie.mask = TCIF_TEXT; 

  for (i = 0; i < page_count; i++)
  {
    pages[i]->create_dlg(hinstance, hwnd);

//    GetWindowText(pages[i]->hwnd, str, max_str);
    tie.pszText = titles[i];
    TabCtrl_InsertItem(tab_ctl, i, &tie); 

    GetWindowRect(pages[i]->hwnd, &dlg_rect);
    if (dlg_rect.right - dlg_rect.left > tab_rect.right)  tab_rect.right  = dlg_rect.right - dlg_rect.left;
    if (dlg_rect.bottom - dlg_rect.top > tab_rect.bottom) tab_rect.bottom = dlg_rect.bottom - dlg_rect.top;
  }

  DWORD base_units = GetDialogBaseUnits(); 
  int cx_margin = LOWORD(base_units) / 4; 
  int cy_margin = HIWORD(base_units) / 8;

  TabCtrl_AdjustRect(tab_ctl, TRUE, &tab_rect);
  OffsetRect(&tab_rect, cx_margin - tab_rect.left, cy_margin - tab_rect.top); 
  memcpy(&dlg_rect, &tab_rect, sizeof(RECT));
  TabCtrl_AdjustRect(tab_ctl, FALSE, &dlg_rect);

  // Adjust tab control
  SetWindowPos(tab_ctl, 0, tab_rect.left, tab_rect.top, 
    tab_rect.right - tab_rect.left, tab_rect.bottom - tab_rect.top, 
    SWP_NOZORDER);

  for (i = 0; i < page_count; i++)
    SetWindowPos(pages[i]->hwnd, HWND_TOP, dlg_rect.left, dlg_rect.top, 
      dlg_rect.right - dlg_rect.left, dlg_rect.bottom - dlg_rect.top, 0);

  active_page = 0;
  if (page_count) pages[active_page]->switch_on();
}

int 
TabDlg::exec(LPCTSTR title)
{
  int result = DialogBoxParam(hinstance, dlg_res, parent, DialogProc, (LPARAM)this);
  for (int i = 0; i < page_count; i++)
    pages[i]->destroy_dlg();
  hwnd = 0;
  return result;
}

void
TabDlg::add_page(int i, TabSheet *sheet, char *title)
{
  if (hwnd) 
  {
    // Cannot add pages to live tab control
    delete sheet;
    return;
  }

  if (i > page_count || i < 0) 
    i = page_count;

  pages[i] = sheet;
  titles[i] = strdup(title);
  page_count++;
}

void
TabDlg::switch_to(int page)
{
  if (page >= page_count || page < 0) return;

  pages[active_page]->switch_off();
  pages[page]->switch_on();
  active_page = page;
}


BOOL 
TabDlg::message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
      init();
      return TRUE;

    case WM_COMMAND:
      command(LOWORD(wParam), HIWORD(wParam));
      return TRUE;

    case WM_NOTIFY: 
      if (((NMHDR*)lParam)->code == TCN_SELCHANGE) 
      {
        switch_to(TabCtrl_GetCurSel(tab_ctl));
        return TRUE;
      }
  }
  return FALSE;
}

void TabDlg::command(int control, int message)
{
  switch (control)
  {
  case IDOK:
    EndDialog(hwnd, IDOK);
    return;

  case IDCANCEL:
    EndDialog(hwnd, IDCANCEL);
    return;
  }
}
