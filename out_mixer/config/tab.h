#ifndef TABDLG_H
#define TABDLG_H

#include <windows.h>

#define MAX_PAGES 100

class TabSheet;
class TabDlg;

class TabSheet
{
private:
  static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
  HWND         hwnd;
  HWND         parent;
  HINSTANCE    hinstance;
  DLGTEMPLATE *dlg_template;
  
  virtual HWND create_dlg(HINSTANCE hinstance, HWND parent);
  virtual void destroy_dlg();

  virtual void switch_on();
  virtual void switch_off();

  virtual BOOL message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  virtual void command(int control, int message)  {};
  virtual void init_controls()                    {};
  virtual void init()                             {};

  friend class TabDlg;

public:
  TabSheet(HMODULE hmodule, LPCSTR dlg_res);
  ~TabSheet();

};

class TabDlg
{
private:
  static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
  HWND      hwnd;
  HWND      parent;
  HINSTANCE hinstance;
  LPCSTR    dlg_res;

  HWND      tab_ctl;
  int       active_page;
  int       page_count;
  TabSheet *pages[MAX_PAGES];
  char     *titles[MAX_PAGES];

  virtual BOOL message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  virtual void command(int control, int message);
  virtual void init();

public:
  TabDlg(HINSTANCE hinstance, LPCSTR dlg_res, HWND parent);
  ~TabDlg();

  int  get_page_count()     { return page_count; };
  TabSheet *get_page(int i) { return i < page_count? pages[i]: 0; };
  void add_page(int i, TabSheet *sheet, char *title);
  void switch_to(int page);

  int exec(LPCTSTR title);
};

#endif
