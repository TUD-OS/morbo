/* -*- Mode: C -*- */

#pragma once

struct conf_item {
  struct conf_item *next;

  /* Just a convenience to avoid having to call strcmp all the time. I
     miss real keywords... */
  enum {
    ROOT,
    LOAD,
    EXEC,
  } command;

  unsigned argc;
  char **argv;
};

struct conf_item *parse_config_file(const char *file_name);

/* EOF */
