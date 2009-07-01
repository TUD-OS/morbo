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
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>		/* for ntohl */

/* Libraries */
#include <gc.h>
#include <slang.h>
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <libelf/gelf.h>

/* Our own stuff */
#include <ohci.h>
#include <morbo.h>
#include <mbi.h>

#include "fw_b0rken.h"
#include "fw_conf_parse.h"

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
  struct node_info_t *next;

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
  uint32_t kernel_entry_point;	/* Pointer to kernel_entry_point uint32_t */

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

static struct node_info_t *
collect_node_info(unsigned target_no)
{
  assert(target_no < 63);

  struct node_info_t *info = GC_MALLOC(sizeof(struct node_info_t));
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
	  break;

	unsigned text_length = root_dir[text_off] >> 16;

        if (text_length > 10)
          break;

        for (unsigned i = 0; i < text_length; i++)
          root_dir[text_off + 1 + i] =  ntohl(root_dir[text_off + 1 + i]);

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
	  break;;
	
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

 done:
  return info;
}

static struct node_info_t *
collect_all_info(int *node_count)
{
  struct node_info_t *info = NULL;
  unsigned nodes = raw1394_get_nodecount(fw_handle);

  for (unsigned i = nodes; i > 0; i--) {
    struct node_info_t *new = collect_node_info(i - 1);
    new->next = info;
    info = new;
  }

  *node_count = nodes;

  return info;
}

/* Generate the overview list. This is the default screen. */
static void
do_overview_screen(struct node_info_t *info)
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
      SLsmg_printf(" | pEntry %08x MBI %08x", info->kernel_entry_point, info->multiboot_ptr);
    }

    SLsmg_erase_eol();

    pos++;
    info = info->next;
  }
}

static void
do_boot_screen(struct node_info_t *boot_node_info)
{
  nodeid_t node = boot_node_info->node_no | LOCAL_BUS;
  /* XXX Hard coded config */
  struct conf_item *conf = parse_config_file("foo.cfg");
  uint32_t mbi_ptr;
  struct mbi mbi;

  SLsmg_gotorc(0, 0);
  SLsmg_set_color(COLOR_NORMAL);
  SLsmg_erase_eos();
  SLsmg_Newline_Behavior = SLSMG_NEWLINE_SCROLLS;
  
  if (conf == NULL) {
    SLsmg_printf("Failed to read config.");
    goto done;
  }

  /* Where is the pointer to the pointer to the MBI? */
  int res = raw1394_read_retry(fw_handle, node, boot_node_info->multiboot_ptr,
			       sizeof(uint32_t), &mbi_ptr);
  if (res == -1) {
    SLsmg_printf("1 errno %d\n", errno);
    goto done;
  }

  /* Where is the pointer to the MBI? */
  /* XXX Might exceed maximum request size */
  res = raw1394_read_compat(fw_handle, node, mbi_ptr, sizeof(struct mbi),
			    (quadlet_t *)&mbi);
  if (res == -1) {
    SLsmg_printf("Reading Multiboot info failed: %s\n", strerror(errno));
    goto done;
  }
  
  SLsmg_printf("flags %x\n", mbi.flags);
  SLsmg_printf("mmap_addr %x - %x (%d bytes)\n", mbi.mmap_addr, mbi.mmap_addr + mbi.mmap_length,
	       mbi.mmap_length);

  /* Read memory map */
  /* XXX Might exceed maximum request size */
  struct memory_map *mmap_buf = GC_MALLOC(mbi.mmap_length);
  res = raw1394_read_compat(fw_handle, node, mbi.mmap_addr, mbi.mmap_length,
			    (quadlet_t *)mmap_buf);
  if (res == -1) {
    SLsmg_printf("Reading memory map failed: %s\n", strerror(errno));
    goto done;
  }

 done:
  SLsmg_refresh();
  sleep(1);
}

int
main(int argc, char **argv)
{
  GC_INIT();

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

/*   int flags = fcntl(raw1394_get_fd(fw_handle), F_GETFL); */
/*   assert((flags & ~O_NONBLOCK) == 0); */
  if (fcntl(raw1394_get_fd(fw_handle), F_SETFL, 0) != 0) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }

  if (elf_version(EV_CURRENT) == EV_NONE ) {
    /* library out of date */
    fprintf(stderr, "Elf library out of date!\n");
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
    static enum { OVERVIEW, BOOT } state = OVERVIEW;
    unsigned boot_no;

    if (screen_size_changed) {
      SLtt_get_screen_size ();
      SLsmg_reinit_smg ();
      screen_size_changed = false;
      /* Redraw... */
    }

    struct node_info_t *info = collect_all_info(&nodes);

    switch (state) {
    case OVERVIEW:
      do_overview_screen(info);
      break;
    case BOOT:
      for (struct node_info_t *cur = info; cur != NULL; cur = cur->next) {
	if (cur->node_no == boot_no) {
	  
	  if (!cur->bootable)
	    break;
	  
	  do_boot_screen(cur);
	}
      }
      state = OVERVIEW;
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
    SLsmg_printf("Hit q to quit | %d nodes | generation %u | libraw1394 %s | %x/%x %x",
		 nodes,
		 raw1394_get_generation(fw_handle), 
		 raw1394_get_libversion(),
		 GC_get_free_bytes(),
		 GC_get_heap_size(),
		 GC_get_bytes_since_gc()
		 );
    SLsmg_erase_eol();

    SLsmg_gotorc(-1, 0);
    SLsmg_refresh();

    /* Process Firewire messages */

    struct pollfd fd = { .fd = raw1394_get_fd(fw_handle),
			 .events = POLLIN | POLLOUT };

    while (poll(&fd, 1, 0) == 1) {
      raw1394_loop_iterate(fw_handle);
    }

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
	/* Digits */
	if ((key >= '0') && (key <= '9')) {
	  boot_no = key - '0';
	  state = BOOT;
	}

	break;
      }
    }

  }

  return 0;
}

/* EOF */
