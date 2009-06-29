/* -*- Mode: C -*- */

/* C */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* "POSIX" */
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>		/* for ntohl */


/* Libraries */
#include <slang.h>
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

/* Our own stuff */
#include <ohci.h>
#include <morbo.h>

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
static int nodes = 0;

static const char *status_names[] = { "UNDEF",
				      "B0rK",
				      "INIT",
				      "RUN",
				      "DUMB" };

struct node_info_t {
  enum {
    UNDEF = 0,
    BROKEN = 1,
    INIT = 2,
    RUN = 3,
    DUMB = 4,
  } status;
  bool bootable;
  uint32_t multiboot_ptr;	/* Pointer to pointer */
  uint32_t kernel_entry_point;	/* Pointer to kernel_entry_point uint32_t */
  uint64_t guid;
  char info_str[32];
};

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

static void
parse_config_rom(nodeid_t target, struct node_info_t *info)
{
  static quadlet_t crom_buf[32];

  info->status = UNDEF;
  info->bootable = false;

  for (unsigned word = 0; word < (sizeof(crom_buf)/4); word++) {
    const unsigned max_tries = 5;
    unsigned tries = 0;
  again: {}

    int ret = raw1394_read(fw_handle, target, CSR_REGISTER_BASE + CSR_CONFIG_ROM + word*sizeof(quadlet_t),
			   sizeof(quadlet_t), crom_buf + word);
    if (ret != 0) {
      /* Retry up to max_tries times. We get spurious
	 EGAIN/EWOULDBLOCK (even if fw_handle should be set to
	 blocking mode) */
      switch (errno) {
      case EAGAIN:
	if (tries++ < max_tries)
	  goto again;
	break;
      };
      info->status = BROKEN;
      return;
    }

    crom_buf[word] = ntohl(crom_buf[word]);

    if (word == 0) {
      if (crom_buf[word] == 0) {
	info->status = INIT;
	return;
      } else if ((crom_buf[word] >> 24) == 1) {
	info->status = DUMB;
	return;
      }
    } else {
      info->status = RUN;
    }
  }

  /* Store GUID */
  info->guid = (uint64_t)crom_buf[3] << 32 | crom_buf[4];
  
  /* Config ROM obtained. Check for Morbo. */
  /* XXX Check CRCs and bounds */
  unsigned bus_info_length = crom_buf[0] >> 24;

  /* Interpret large values as garbage. */
  if (bus_info_length > 32)
    return;

  uint32_t *root_dir = &crom_buf[bus_info_length + 1];
  unsigned root_dir_length = root_dir[0] >> 16;
  
  /* Interpret large values as garbage. */
  if (root_dir_length > 32)
    return;

  bool vendor_ok = false;
  bool model_ok  = false;
  bool boot_info = false;

  for (unsigned i = 1; i <= root_dir_length; i++) {
    switch (root_dir[i] >> 24) {
    case 0x03:			/* Vendor ID */
      vendor_ok = (root_dir[i] & 0xFFFFFF) == MORBO_VENDOR_ID;
      break;
    case 0x17:			/* Model ID */
      model_ok  = (root_dir[i] & 0xFFFFFF) == MORBO_MODEL_ID;
      break;
    case 0x81:			/* Text descriptor */
      {
	unsigned text_off = i + (root_dir[i] & 0xFFFFFF);
	if (text_off > 32)
	  return;

	unsigned text_length = root_dir[text_off] >> 16;

	memset(info->info_str, 0, sizeof(info->info_str));
	strncpy(info->info_str, (char *)(&root_dir[text_off + 3]),
		MIN(sizeof(info->info_str),
		    text_length*4));
      }
      break;
    case MORBO_INFO_DIR:
      {
	unsigned info_off = i + (root_dir[i] & 0xFFFFFF);
	if (info_off > 32)
	  return;
	
	info->multiboot_ptr = root_dir[info_off + 1];
	info->kernel_entry_point = root_dir[info_off + 2];
	boot_info = true;
      }
      break;
    default:
      /* Unknown entry. What now? Ignoring...*/
      break;
    }
  }

  if (vendor_ok && model_ok && boot_info)
    info->bootable = true;
}

/* Generate the overview list. This is the default screen. */
static void
do_overview_screen(void)
{
  SLsmg_Newline_Behavior = SLSMG_NEWLINE_PRINTABLE;
  
  /* Print node info. */
  nodes = raw1394_get_nodecount(fw_handle);
  nodeid_t local_id = raw1394_get_local_id(fw_handle);
  nodeid_t irm_id = raw1394_get_irm_id(fw_handle);
  nodeid_t max_disp = MIN(nodes, SLtt_Screen_Rows - 1);
  
  for (unsigned i = 0; i < max_disp; i++) {
    bool myself = (i == NODE_NO(local_id));
    bool root   = (i == nodes - 1);
    bool irm    = (i == NODE_NO(irm_id));
    
    /* Get node info */
    struct node_info_t info;
    nodeid_t target = LOCAL_BUS | i;
    
    parse_config_rom(target, &info);

    /* Finished collecting information. Now display it. */

    SLsmg_gotorc(i, 0);
    SLsmg_set_color(myself ? COLOR_MYSELF : ((info.status == BROKEN) ? COLOR_BROKEN : COLOR_NORMAL));

    SLsmg_printf("%3u %c%c | %016llx %4s %4s | %s", i,
		 root ? 'R' : ' ',
		 irm ?  'I' : ' ',
		 (info.status == RUN) ? info.guid : 0LLU,
		 status_names[info.status],
		 (info.bootable ? "BOOT" : ""),
		 (info.bootable ? info.info_str : "")
		 );

    SLsmg_erase_eol();
  }
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
      screen_size_changed = false;
      /* Redraw... */
    }

    do_overview_screen();

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

    SLsmg_gotorc(-1, 0);
    SLsmg_refresh();

    /* Process Firewire messages */

/*     struct pollfd fd = { .fd = raw1394_get_fd(fw_handle), */
/* 			 .events = POLLIN | POLLOUT }; */

/*     while (poll(&fd, 1, 0) == 1) { */
/*       raw1394_loop_iterate(fw_handle); */
/*     } */

    /* Sleep waiting for user input. */
    SLang_input_pending(1);

    /* Process user input */
    while (SLang_input_pending(0)) {
      switch (SLang_getkey()) {
      case 'q':
	done = true;
	break;
      case 'r':
	raw1394_reset_bus(fw_handle);
	break;
      default:
	/* IGNORE */
	break;
      }
    }

  }

  return 0;
}

/* EOF */
