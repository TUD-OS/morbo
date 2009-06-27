// -*- Mode: C++ -*-

#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <signal.h>

#define ENABLE_SLFUTURE_CONST
#include <slang.h>

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

// Globals

volatile static bool screen_size_changed = false;

static int port = 0;
static raw1394handle_t fw_handle;

// Implementation

static void sigwinch_handler (int sig)
{
  screen_size_changed = true;
  SLsignal (SIGWINCH, sigwinch_handler);
}

static void sigint_handler (int sig)
{
  SLang_reset_tty();
  _exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
  int opt;

  // Command line parsing
  while ((opt = getopt(argc, argv, "d:p:")) != -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    default:
      fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  // Connect to raw1394 device
  fw_handle = raw1394_new_handle_on_port(port);
  if (fw_handle == NULL) {
    perror("raw1394_new_handle_on_port");
    exit(EXIT_FAILURE);
  }

  // Set up slang UI library.
  SLsignal (SIGWINCH, sigwinch_handler);
  SLsignal (SIGINT, sigint_handler);

  SLtt_get_terminfo();
  SLang_init_tty(-1, 0, 0);
  SLsmg_init_smg();

  atexit(SLang_reset_tty);      // Called in reverse order
  atexit(SLsmg_reset_smg);

  SLsmg_Newline_Behavior = SLSMG_NEWLINE_SCROLLS;
  SLsmg_printf("Foo!\n");
  SLsmg_refresh();
  
  sleep(5);
  
  return 0;
}
