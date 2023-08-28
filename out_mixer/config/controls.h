/*
  Controls used at config dialog.

  Tooltip
  -------
  Implements tooltips. For some reasons OleCreatePropertyFrame blocks dialog
  mouse messages (WM_NCHITTEST returns HTNOWHERE), therefore, we have to
  implement own algorithm of tooltip activation based on timer.

  create() - create tooltip control.
  destroy() - destroy tooltip control.
  track() - this function tracks mouse position and activates tooltip when
    needed. It must be called periodically (by timer for example).

  enable() - enable or disable tooltips at all.
  set_width() - set tooltip width. This also enables multi-line mode.
  set_delay() - time when tooltip should appear in ms.

  add_window() - add a window to show tooltip on.
  add cointrol() - add a control to show tooltip on.

  Edit
  ----
  Base for edit controls. Subclasses an edit control to make:
  * 'enter' checks the value and either accepts it or reports an error and
    restores old value.
  * 'escape' just restores an old value.
  * when control looses focus it acts as 'enter'.

  DoubleEdit
  ----------
  Numeric edit control.

  TextEdit
  --------
  String edit control.

  LinkButton
  ----------
  A simple link control

*/

#ifndef CONTROLS_H
#define CONTROLS_H

#include <windows.h>

#define CB_ENTER (WM_USER+1)

class Tooltip
{
protected:
  HWND hwnd;
  HWND tooltip;
  HINSTANCE hinstance;

  POINT mouse_pt;
  __int64 mouse_time;

  bool enabled;
  bool visible;
  int  delay;

  void show(bool show);

public:
  Tooltip();
  ~Tooltip();

  bool create(HINSTANCE _hinstance, HWND _hwnd, bool enabled = true);
  void destroy();
  void track();

  void enable(bool enabled);
  void set_width(int width);
  void set_delay(int ms);

  void add_window(HWND window, const char *text);
  void add_control(int control_id, const char *text);
};

class Edit
{
protected:
  static LRESULT CALLBACK SubClassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  HWND    dlg;
  HWND    hwnd;
  int     item;
  WNDPROC wndproc;
  bool    editing;

  virtual bool read_value() = 0;
  virtual void backup_value() = 0;
  virtual void restore_value() = 0;
  virtual void write_value() = 0;
  virtual const char *incorrect_value() { return "Incorrect value"; }

public:
  Edit(): dlg(0), hwnd(0), item(0), wndproc(0), editing(false) {};
  ~Edit();

  void link(HWND dlg, int item);
  void unlink();
  void enable(bool enabled);
};

class DoubleEdit : public Edit
{
protected:
  double old_value;

  bool read_value();
  void backup_value();
  void restore_value();
  void write_value();
  const char *incorrect_value() { return "Incorrect value: must be number"; }

public:
  double value;
  DoubleEdit() {};

  void update_value(double _value) { value = _value; write_value(); };
};

class TextEdit : public Edit
{
protected:
  size_t size;
  char *old_value;
  char *value;

  bool read_value();
  void backup_value();
  void restore_value();
  void write_value();

public:
  TextEdit(size_t size = 256);
  ~TextEdit();

  void set_text(const char *text);
  const char *get_text() { return value; };
};

class LinkButton
{
protected:
  HWND    dlg;
  HWND    hwnd;
  int     item;
  WNDPROC wndproc;
  HFONT   font;

  static LRESULT CALLBACK SubClassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
  LinkButton(): dlg(0), hwnd(0), item(0), wndproc(0) {};
  ~LinkButton();

  void link(HWND dlg, int item);
  void unlink();  

  virtual void paint(HDC dc);
  virtual void press();
};

/*
class Slider
{
public:
  Slider();
  ~Slider();


  void link(HWND dlg, int item);
  void unlink();
};
*/

/*
class ComboBox
{
protected:
  static LRESULT CALLBACK SubClassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  HWND    dlg;
  HWND    hwnd;
  int     item;
  WNDPROC wndproc;


public:
  COMBOBOXINFO info;
  ComboBox(): dlg(0), hwnd(0), item(0), wndproc(0) {};
  ~ComboBox();

  void link(HWND dlg, int item);
  void unlink();
};
*/


#endif
