/* -*- Mode: C -*- */

#include <stdio.h>
#include <string.h>
#include <gc.h>

#include "fw_conf_parse.h"

char **
parse_str(char *line, unsigned *argc)
{
  char *saveptr;

  char **recurse(char *str, unsigned count) {
    char *token = strtok_r(str, " \n", &saveptr);
    char **buf;

    if (token == NULL) {
      buf = GC_malloc(count*sizeof(char *));
      *argc = count;
    } else {
      buf = recurse(NULL, count+1);
      buf[count] = GC_strdup(token);
    }

    return buf;
  };

  return recurse(line, 0);
}


struct conf_item *
parse_config_file(const char *file_name)
{
  FILE *fd = fopen(file_name, "r");

  if (fd == NULL)
    return NULL;

  char buf[256];
  struct conf_item *first = NULL;
  struct conf_item *last = NULL;

  while (fgets(buf, sizeof(buf), fd) != NULL) {
    if (buf[0] == '#')
      /* Comment */
      continue;

    struct conf_item *nconf = GC_malloc(sizeof(struct conf_item));
    nconf->argv = parse_str(buf, &(nconf->argc));
    nconf->next = NULL;

    if (nconf->argc < 2)
      /* Skip lines with less then two tokens. */
      continue;

    if (strcmp(nconf->argv[0], "root") == 0) {
      nconf->command = ROOT;
    } else if (strcmp(nconf->argv[0], "load") == 0) {
      nconf->command = LOAD;
    } else if (strcmp(nconf->argv[0], "exec") == 0) {
      nconf->command = EXEC;
    } else 
      /* Skip unknown line. */
      continue;

    /* Append conf item at end of list. */
    if (first == NULL) {
      first = nconf;
    }
    
    if (last != NULL) {
      last->next = nconf;
    }
    last = nconf;
  }

  fclose(fd);
  return first;
}
