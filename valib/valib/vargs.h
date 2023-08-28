/*
  Command-line arguments handling
*/

#ifndef VALIB_VARGS_H
#define VALIB_VARGS_H

enum arg_type { argt_exist, argt_bool, argt_num, argt_hex };
bool is_arg(char *arg, const char *name, arg_type type);
bool arg_bool(char *arg);
double arg_num(char *arg);
int arg_hex(char *arg);

#endif
