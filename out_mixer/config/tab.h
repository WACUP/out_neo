#ifndef TABDLG_H
#define TABDLG_H

#include <windows.h>

#define MAX_PAGES 5

class TabSheet;
class TabDlg;

class TabSheet
{
private:
	static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
	HWND hwnd;
	HWND parent;
	UINT dlg_res;
  
	virtual HWND create_dlg(HWND parent);
	virtual void destroy_dlg();

	virtual void switch_on();
	virtual void switch_off();

	virtual BOOL message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void command(int control, int message) {};

	friend class TabDlg;

public:
	TabSheet(UINT _dlg_res);
	~TabSheet();
};

class TabDlg
{
private:
	//static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
	HWND hwnd;
	HWND parent;

	HWND tab_ctl;
	int active_page;
	int page_count;
	TabSheet *pages[MAX_PAGES];
	TCHAR *titles[MAX_PAGES];

	virtual void init();

public:
	TabDlg(HWND parent);
	~TabDlg();

	int get_page_count() const { return page_count; };
	TabSheet *get_page(int i) { return i < page_count ? pages[i] : 0; };
	void add_page(int i, TabSheet *sheet, TCHAR *title);
	void switch_to(int page);

	void exec();

	virtual BOOL message(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif