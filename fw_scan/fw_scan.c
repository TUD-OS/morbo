/* -*- Mode: C -*- */

/* C */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>		/* for PRIx64 and friends */
#include <string.h>

/* "POSIX" */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>		/* for ntohl */
#include <sys/time.h>
#include <sys/mman.h>

/* Libraries */
#include <slang.h>
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

/* Our own stuff */
#include <ohci-constants.h>
#include <morbo.h>
#include <mbi.h>

#include "fw_b0rken.h"

/* Constants */

enum Color_obj {
  COLOR_NORMAL  = 0,
  COLOR_HILIGHT = 1,
  COLOR_MYSELF  = 2,
  COLOR_STATUS  = 3,
  COLOR_BROKEN  = 4,
};

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define ROUND_UP_PAGE(X) (((X) + 0xFFF) & ~0xFFF)

#define MODULE_STRING_BUFFER_SIZE (0x1000)

/* Black list the first 64K of remote memory. Morbo is there. */
#define MODULE_LOAD_LOWER_BOUND   (0x00110000) 

/* Globals */

volatile static bool done = false;
volatile static bool screen_size_changed = false;

static int delay = 1;		/* Update every second. */
static int port = 0;		/* Default port is 0. Should be fine
				   for most. */

static raw1394handle_t fw_handle = NULL;
static int nodes = 0;

static const char *status_names[] = { "UNDEF",
				      "B0rK",
				      "INIT",
				      "RUN",
				      "DUMB" };

struct node_info {
  struct node_info *next;

  enum {
    UNDEF = 0,
    BROKEN = 1,
    INIT = 2,
    RUN = 3,
    DUMB = 4,
  } status;
  
  unsigned node_no;

  bool bootable;
  bool busmaster;
  bool irm;
  bool me;
  uint64_t guid;

  uint32_t multiboot_ptr;	/* Pointer to pointer */

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

static struct node_info *
collect_node_info(unsigned target_no)
{
  assert(target_no < 63);

  static struct node_info nodes[63];
  struct node_info *info = &nodes[target_no];
  quadlet_t crom_buf[32];

  info->status    = UNDEF;
  info->node_no   = target_no;
  info->bootable  = false;	/* Is set later */
  info->busmaster = (target_no == raw1394_get_nodecount(fw_handle)-1);
  info->irm       = (target_no == NODE_NO(raw1394_get_irm_id(fw_handle)));
  info->me        = (target_no == NODE_NO(raw1394_get_local_id(fw_handle)));

  for (unsigned word = 0; word < (sizeof(crom_buf)/4); word++) {
    int ret = raw1394_read_retry(fw_handle, target_no | LOCAL_BUS, CSR_REGISTER_BASE + CSR_CONFIG_ROM + word*sizeof(quadlet_t),
				 sizeof(quadlet_t), crom_buf + word);
    if (ret != 0) {
      info->status = BROKEN;
      goto done;
    }

    crom_buf[word] = ntohl(crom_buf[word]);

    if (word == 0) {
      if (crom_buf[word] == 0) {
	info->status = INIT;
	goto done;
      } else if ((crom_buf[word] >> 24) == 1) {
	info->status = DUMB;
	goto done;
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
    goto done;

  uint32_t *root_dir = &crom_buf[bus_info_length + 1];
  unsigned root_dir_length = root_dir[0] >> 16;
  
  /* Interpret large values as garbage. */
  if (root_dir_length > 32)
    goto done;

  bool vendor_ok = false;
  bool model_ok  = false;
  bool boot_info = false;

  bool info_found = false;

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
	if (info_found) break;
	unsigned text_off = i + (root_dir[i] & 0xFFFFFF);
	if (text_off > 32)
	  break;

	unsigned text_length = root_dir[text_off] >> 16;

        if (text_length > 20)
          break;

        for (unsigned i = 0; i < text_length; i++)
          root_dir[text_off + 1 + i] =  ntohl(root_dir[text_off + 1 + i]);

	memset(info->info_str, 0, sizeof(info->info_str));
	strncpy(info->info_str, (char *)(&root_dir[text_off + 3]),
		MIN(sizeof(info->info_str),
		    text_length*4));
	info_found = true;
      }
      break;
    case MORBO_INFO_DIR:
      {
	unsigned info_off = i + (root_dir[i] & 0xFFFFFF);
	if (info_off > 32)
	  break;;
	
	info->multiboot_ptr = root_dir[info_off + 1];
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

 done:
  return info;
}

static struct node_info *
collect_all_info(int *node_count)
{
  struct node_info *info = NULL;
  unsigned nodes = raw1394_get_nodecount(fw_handle);

  for (unsigned i = nodes; i > 0; i--) {
    struct node_info *new = collect_node_info(i - 1);
    new->next = info;
    info = new;
  }

  *node_count = nodes;

  return info;
}

/* Generate the overview list. This is the default screen. */
static void
do_overview_screen(struct node_info *info)
{
  SLsmg_Newline_Behavior = SLSMG_NEWLINE_PRINTABLE;
  
  /* Print node info. */
  unsigned pos = 0;
  while ((pos < (SLtt_Screen_Rows - 1)) && info) {

    SLsmg_gotorc(pos, 0);
    SLsmg_set_color(info->me ? COLOR_MYSELF : ((info->status == BROKEN) ? COLOR_BROKEN : COLOR_NORMAL));

    SLsmg_printf("%3u %c%c | %016llx %4s %4s | %s",
		 info->node_no,
		 info->busmaster ? 'B' : ' ',
		 info->irm ?  'I' : ' ',
		 (info->status == RUN) ? info->guid : 0LLU,
		 status_names[info->status],
		 (info->bootable ? "BOOT" : ""),
		 info->info_str /* Is an empty string, when no info is available. */
		 );

    if (info->bootable) {
      SLsmg_printf(" | MBI %08x", info->multiboot_ptr);
    }

    SLsmg_erase_eol();

    pos++;
    info = info->next;
  }
}

static void bw_info(uint64_t bytes, struct timeval *start, struct timeval *end)
{
  unsigned long diff_usec
    = (end->tv_sec * 1000000 + end->tv_usec) -
    (start->tv_sec * 1000000 + start->tv_usec);
  float diff_sec = ((float)diff_usec) / 1000000.0;
  
  SLsmg_printf("%.2fs %.2f MB/s\n", diff_sec,
	       (diff_sec == 0.0) ? -1.0 : ((float)bytes) / (diff_sec * 1024 * 1024));
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
    static enum { OVERVIEW } state = OVERVIEW;
    unsigned boot_no;

    if (screen_size_changed) {
      SLtt_get_screen_size ();
      SLsmg_reinit_smg ();
      screen_size_changed = false;
      /* Redraw... */
    }

    /* Connect to raw1394 device. */
    if (fw_handle) raw1394_destroy_handle(fw_handle);
    fw_handle = raw1394_new_handle_on_port(port);
    if (fw_handle == NULL) {
      perror("raw1394_new_handle_on_port");
      exit(EXIT_FAILURE);
    }

    struct node_info *info = collect_all_info(&nodes);

    switch (state) {
    case OVERVIEW:
      do_overview_screen(info);
      break;
    }

    /* Clear output from last iteration. */
    SLsmg_set_color(COLOR_NORMAL);
    SLsmg_erase_eos();

    /* Status line */
    SLsmg_gotorc(SLtt_Screen_Rows - 1, 0);
    SLsmg_Newline_Behavior = SLSMG_NEWLINE_PRINTABLE;
    SLsmg_set_color(COLOR_STATUS);

    static int i = 0;
    SLsmg_printf("Hit q to quit | %d nodes | generation %u | libraw1394 %s",
		 nodes,
		 raw1394_get_generation(fw_handle), 
		 raw1394_get_libversion()
		 );
    SLsmg_erase_eol();

    SLsmg_gotorc(-1, 0);
    SLsmg_refresh();

    /* Sleep waiting for user input. */
    SLang_input_pending(10);

    /* Process user input */
    while (SLang_input_pending(0)) {
      int key = SLang_getkey();
      switch (key) {
      case 'q':
	done = true;
	break;
      case 'r':
	raw1394_reset_bus(fw_handle);
	break;
      default:
        /* Ignore */
	break;
      }
    }

  }

  return 0;
}

/* Local Variables: */
/* mode:outline-minor */
/* End: */
