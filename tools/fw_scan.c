/* -*- Mode: C -*- */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <getopt.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>

#include <slang.h>
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

#include <ohci.h>

/* Constants */

enum Color_obj {
  COLOR_NORMAL  = 0,
  COLOR_HILIGHT = 1,
  COLOR_MYSELF  = 2,
  COLOR_STATUS  = 3,
  COLOR_BROKEN  = 4,
};

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

/* Globals */

volatile static bool done = false;
volatile static bool screen_size_changed = false;

static int delay = 1;		/* Update every second. */
static int port = 0;		/* Default port is 0. Should be fine
				   for most. */

static raw1394handle_t fw_handle;

/* Implementation */

static void sigwinch_handler (int sig)
{
  screen_size_changed = true;
  SLsignal (SIGWINCH, sigwinch_handler);
}

static void sigint_handler (int sig)
{
  static unsigned interrupted = 0;

  if (interrupted++ == 0) {
    /* First interrupt. */
    done = true;
  } else {
    /* Second interrupt (or more) */
    SLang_reset_tty();
    _exit(EXIT_FAILURE);
  }

  SLsignal (SIGINT, sigwinch_handler);
}

int
main(int argc, char **argv)
{
  /* Command line parsing */
  int opt;

  while ((opt = getopt(argc, argv, "d:p:")) != -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    case 'd':
      delay = atoi(optarg);
      break;
    default:
      fprintf(stderr, "Usage: %s [-d delay] [-p port]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  /* Connect to raw1394 device. */
  fw_handle = raw1394_new_handle_on_port(port);
  if (fw_handle == NULL) {
    perror("raw1394_new_handle_on_port");
    exit(EXIT_FAILURE);
  }

  /* Set up slang UI library. */
  SLsignal (SIGWINCH, sigwinch_handler);
  SLsignal (SIGINT, sigint_handler);

  SLtt_get_terminfo();
  SLang_init_tty(-1, 0, 0);
  SLsmg_init_smg();

  atexit(SLang_reset_tty);	/* Called in reverse order */
  atexit(SLsmg_reset_smg);

  /* Setting up colors */
  SLtt_set_color(COLOR_NORMAL, "normal", "lightgray", "black");
  SLtt_set_color(COLOR_HILIGHT, "hilight", "white", "black");
  SLtt_set_color(COLOR_MYSELF, "myself", "brightblue", "black");
  SLtt_set_color(COLOR_STATUS, "status", "white", "gray");
  SLtt_set_color(COLOR_BROKEN, "broken", "brightred", "black");
  
  /* Main loop */
  while (!done) {
    if (screen_size_changed) {
      SLtt_get_screen_size ();
      SLsmg_reinit_smg ();
      /* Redraw... */
    }

    SLsmg_Newline_Behavior = SLSMG_NEWLINE_PRINTABLE;

    /* Print node info. */
    int nodes = raw1394_get_nodecount(fw_handle);
    nodeid_t local_id = raw1394_get_local_id(fw_handle);
    nodeid_t irm_id = raw1394_get_irm_id(fw_handle);
    nodeid_t max_disp = MIN(nodes, SLtt_Screen_Rows - 1);

    for (unsigned i = 0; i < max_disp; i++) {
      bool myself = (i == NODE_NO(local_id));
      bool root   = (i == nodes - 1);
      bool irm    = (i == NODE_NO(irm_id));

      /* Get CROM. */
      bool broken = false;
      bool info_available = false;
      char *status = "UNDEF";
      static quadlet_t crom_buf[1024/4]; /* Max 1K CROM */
      nodeid_t target = LOCAL_BUS | i;

      for (unsigned word = 0; word < 5; word++) {
	int ret = raw1394_read(fw_handle, target, CSR_REGISTER_BASE + CSR_CONFIG_ROM + word*sizeof(quadlet_t),
			       sizeof(quadlet_t), crom_buf + word);
	
	if (ret != 0) {
	  broken = true;
	  break;
	}

	if (word == 0) {
	  if (crom_buf[word] == 0) {
	    status = "INIT";
	    break;
	  } else if ((crom_buf[word] >> 24) == 1) {
	    status = "DUMB";	/* Vendor CROM */
	    break;
	  }
	} else {
	  status = "RUN";
	  info_available = true;
	}

      }
      

      /* Finished collecting information. Now display it. */

      SLsmg_gotorc(i, 0);
      SLsmg_set_color(myself ? COLOR_MYSELF : (broken ? COLOR_BROKEN : COLOR_NORMAL));

      SLsmg_printf("%3u %c%c | %6s %8x %8x |", i,
		   root ? 'R' : ' ',
		   irm ?  'I' : ' ',
		   status,
		   broken ? 0xDEAD : crom_buf[0],
		   (info_available && !broken) ? crom_buf[1] : 0xDEAD
		   );

      SLsmg_erase_eol();
    }

    /* Clear output from last iteration. */
    SLsmg_set_color(COLOR_NORMAL);
    SLsmg_erase_eos();

    /* Status line */
    SLsmg_gotorc(SLtt_Screen_Rows - 1, 0);
    SLsmg_Newline_Behavior = SLSMG_NEWLINE_PRINTABLE;
    SLsmg_set_color(COLOR_STATUS);

    static int i = 0;
    SLsmg_printf("Hit q to quit | %2d nodes | generation %2u | libraw1394 %s",
		 nodes,
		 raw1394_get_generation(fw_handle), 
		 raw1394_get_libversion());
    SLsmg_erase_eol();

    /* Process Firewire messages */
    struct pollfd fd = { .fd = raw1394_get_fd(fw_handle),
			 .events = POLLIN | POLLOUT | POLLERR };

    while (poll(&fd, 1, 0) == 1) {
      raw1394_loop_iterate(fw_handle);
    }

    /* Sleep waiting for user input. */
    SLang_input_pending(1);

    /* Process user input */
    while (SLang_input_pending(0)) {
      switch (SLang_getkey()) {
      case 'q':
	done = true;
	goto skip;
      case 'r':
	raw1394_reset_bus(fw_handle);
	break;
      default:
	/* IGNORE */
	break;
      }
    }



  skip:
    SLsmg_gotorc(-1, 0);
    SLsmg_refresh();
  }

  return 0;
}

/* EOF */
