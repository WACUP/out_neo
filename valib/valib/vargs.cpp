#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "vargs.h"

bool is_arg(char *arg, const char *name, arg_type type)
{
  if (arg[0] != '-') return false;
  arg++;

  while (*name)
    if (*name && *arg != *name) 
      return false;
    else
      name++, arg++;

  if (type == argt_exist && *arg == '\0') return true;
  if (type == argt_bool && (*arg == '\0' || *arg == '+' || *arg == '-')) return true;
  if (type == argt_num && (*arg == ':' || *arg == '=')) return true;
  if (type == argt_hex && (*arg == ':' || *arg == '=')) return true;

  return false;
}

bool arg_bool(char *arg)
{
  arg += strlen(arg) - 1;
  if (*arg == '-') return false;
  return true;
}

double arg_num(char *arg)
{
  arg += strlen(arg);
  while (*arg != ':' && *arg != '=')
    arg--;
  arg++;
  return atof(arg);
}

int arg_hex(char *arg)
{
  arg += strlen(arg);
  while (*arg != ':' && *arg != '=')
    arg--;
  arg++;

  int result;
  sscanf(arg, "%x", &result);
  return result;
}
