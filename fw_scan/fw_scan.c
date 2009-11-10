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
#include <gc.h>
#include <slang.h>
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <gelf.h>

/* Our own stuff */
#include <ohci-constants.h>
#include <morbo.h>
#include <mbi.h>

#include "fw_b0rken.h"
#include "fw_conf_parse.h"
#include "fw_scan_memory.h"

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

static raw1394handle_t fw_handle;
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

static struct node_info *
collect_node_info(unsigned target_no)
{
  assert(target_no < 63);

  struct node_info *info = GC_NEW(struct node_info);
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

        if (text_length > 20)
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
      SLsmg_printf(" | pEntry %08x MBI %08x", info->kernel_entry_point, info->multiboot_ptr);
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

char *
strjoin(int argc, char **argv, char fill)
{
  assert(argc >= 1);

  /* Build command line. */
  size_t length = 0;
  for (unsigned i = 0; i < argc; i++)
    length += strlen(argv[i]) + 1;
  
  char fillb[2] = { fill, 0 };
  char *buf = GC_MALLOC_ATOMIC(length);
  buf[0] = 0;
  for (unsigned i = 0; i < argc; i++) {
    strcat(buf, argv[i]);
    if (i < argc-1) strcat(buf, fillb);
  }
  
  return buf;
}

static void
do_boot_screen(struct node_info *boot_node_info)
{
  nodeid_t node = boot_node_info->node_no | LOCAL_BUS;
  /* XXX Hard coded config */
  struct conf_item *conf = parse_config_file("foo.cfg");
  uint32_t mbi_ptr;
  struct mbi mbi;
  int res;

  SLsmg_gotorc(0, 0);
  SLsmg_set_color(COLOR_NORMAL);
  SLsmg_erase_eos();
  SLsmg_Newline_Behavior = SLSMG_NEWLINE_SCROLLS;
  
  if (conf == NULL) {
    SLsmg_printf("Failed to read config.");
    goto done;
  }

  /* Read pointer to MBI */
  res = raw1394_read_retry(fw_handle, node, boot_node_info->multiboot_ptr,
			   sizeof(uint32_t), &mbi_ptr);
  if (res == -1) {
    SLsmg_printf("1 errno %d\n", errno);
    goto done;
  }

  /* Read MBI */
  res = raw1394_read_large(fw_handle, node, mbi_ptr, sizeof(struct mbi),
			   (quadlet_t *)&mbi);
  if (res == -1) {
    SLsmg_printf("Reading Multiboot info failed: %s\n", strerror(errno));
    goto done;
  }
  
  SLsmg_printf("flags %x\n", mbi.flags);
  if ((mbi.flags & MBI_FLAG_MMAP) == 0) {
    SLsmg_printf("No memory map on target. You are out of luck.\n");
    goto fail;
  }
  SLsmg_printf("mmap_addr %x - %x (%d bytes)\n", mbi.mmap_addr, mbi.mmap_addr + mbi.mmap_length,
	       mbi.mmap_length);

  /* Read memory map */
  struct memory_map *mmap_buf = GC_MALLOC_ATOMIC(mbi.mmap_length);
  res = raw1394_read_large(fw_handle, node, mbi.mmap_addr, mbi.mmap_length,
			   (quadlet_t *)mmap_buf);
  if (res == -1) {
    SLsmg_printf("Reading memory map failed: %s\n", strerror(errno));
    goto done;
  }

  struct memory_info *mem_info = parse_mbi_mmap(mmap_buf, mbi.mmap_length);

  unsigned i = 0;
  for (struct memory_info *cur = mem_info; cur != NULL; cur = cur->next, i++) {
    if (cur->type == 1) 	/* Available */
      SLsmg_printf("mmap[%u] addr %8"PRIx64" size %8"PRIX64" type %x\n",
		   i, cur->addr, cur->length, cur->type);
  }

  /* Apply config */

  uint64_t entry_point = ~0ULL;	/* Entry point  */
  uint64_t module_load_address = 0; /* Set in EXEC */

  unsigned total_modules = 0;
  unsigned current_module = 0;
  for (struct conf_item *cur = conf; cur != NULL; cur = cur->next)
    total_modules += (cur->command == LOAD) ? 1 : 0;

  /* Bookkeeping to generate mbi module map. */
  char *string_buffer = GC_MALLOC_ATOMIC(MODULE_STRING_BUFFER_SIZE);
  unsigned string_buffer_cur = 0;
  struct module *module_info = GC_MALLOC_ATOMIC(sizeof(struct module)*total_modules);

  struct timeval tv_start, tv_end;
  int fd;
  char *buf;
  Elf *elf;

  for (struct conf_item *cur = conf; cur != NULL; cur = cur->next) {
    switch (cur->command) {
    case LOAD:			/* Load a module */
      if (module_load_address == 0) {
	SLsmg_printf("load before exec. Bail out.\n");
	goto fail;
      }

      fd = open(cur->argv[1], O_RDONLY);
      if (fd == -1) {
	SLsmg_printf("Could not open %s: %s\n", cur->argv[1], strerror(errno));
	goto done;
      }
      off_t size = lseek(fd, 0, SEEK_END);
      SLsmg_printf("Module %s: 0x%lx Bytes\n", cur->argv[1], size);

      void *mod_map = mmap(NULL, size, PROT_READ, MAP_PRIVATE,
			   fd, 0);
      if (mod_map == MAP_FAILED) {
	SLsmg_printf("Map failed\n");
	goto mod_fail;
      }

      SLsmg_printf("Mapped at %p.\n", mod_map);
      
      SLsmg_refresh();
      gettimeofday(&tv_start, NULL);
      res = raw1394_write_large(fw_handle, node, module_load_address, size,
				(quadlet_t *)mod_map);
      if (res == -1) {
	SLsmg_printf("Could not load module: %s\n", strerror(errno));
	goto mod_fail;
      }
      gettimeofday(&tv_end, NULL);
      bw_info(size, &tv_start, &tv_end);
      
      /* Build module info */
      module_info[current_module].mod_start = module_load_address;
      module_info[current_module].mod_end   = module_load_address + size - 1;
      module_info[current_module].reserved  = 0;

      /* Build command line. */

      buf = strjoin(cur->argc-1, cur->argv + 1, ' ');
      size_t length = strlen(buf) + 1;

      module_info[current_module].string = string_buffer_cur;
      string_buffer_cur += length;

      memcpy(string_buffer + string_buffer_cur, buf, length);

      module_load_address += ROUND_UP_PAGE(size);
      current_module      += 1;

    mod_done:
      close(fd);
      break;
    mod_fail:
      close(fd);
      goto fail;
    case EXEC:			/* Load and unpack an ELF binary */
      if (entry_point != ~0ULL) {
	SLsmg_printf("You have more than one exec statement in your config. Bail out...\n");
	goto fail;
      }
      SLsmg_printf("Kernel %s\n", cur->argv[1]);

      fd = open(cur->argv[1], O_RDONLY);

      if (fd == -1) {
	SLsmg_printf("Could not open %s: %s\n", cur->argv[1], strerror(errno));
	goto done;
      }

      elf = elf_begin(fd, ELF_C_READ, NULL);

      /* Iterate over program headers. */
      GElf_Ehdr ehdr; gelf_getehdr(elf, &ehdr);
      entry_point = ehdr.e_entry;

      for (unsigned p = 0; p < ehdr.e_phnum; p++) {
	GElf_Phdr phdr; gelf_getphdr(elf, p, &phdr);
	
	if (phdr.p_type == PT_LOAD) {
	  SLsmg_printf("LOAD paddr 0x%"PRIx64" (file 0x%"PRIx64", mem 0x%"PRIx64")\n",
		       phdr.p_paddr, phdr.p_filesz, phdr.p_memsz);

	  if (phdr.p_paddr < MODULE_LOAD_LOWER_BOUND) {
	    SLsmg_printf("Memory conflict. Link your kernel at an address higher than 0x%8x.\n",
			 MODULE_LOAD_LOWER_BOUND);
	    goto elf_fail;
	  }

	  if (!mem_range_available(mem_info, phdr.p_paddr, phdr.p_memsz)) {
	    SLsmg_printf("Memory not free!\n");
	    goto elf_fail;
	  }
	  
	  /* Take a guess where to put modules. */
	  if ((phdr.p_paddr + phdr.p_memsz) > module_load_address)
	    module_load_address = ROUND_UP_PAGE(phdr.p_paddr + phdr.p_memsz);
	  
	  buf = GC_MALLOC(phdr.p_memsz);
	  /* GC returns zero'd memory. Otherwise we had to zero it
	     explicitly. */

	  res = lseek(fd, phdr.p_offset, SEEK_SET);
	  if (res == (off_t)-1) {
	    SLsmg_printf("Could not seek: %s\n", strerror(errno));
	    goto elf_fail;
	  }
	  res = read(fd, buf, phdr.p_filesz);
	  if (res != phdr.p_filesz) {
	    SLsmg_printf("Could not read: %s\n", strerror(errno));
	    goto elf_fail;
	  }
	  
	  SLsmg_refresh();
	  gettimeofday(&tv_start, NULL);
	  res = raw1394_write_large(fw_handle, node, phdr.p_paddr, phdr.p_memsz,
					(quadlet_t *)buf);
	  if (res == -1) {
	    SLsmg_printf("Could not write ELF segment: %s\n", strerror(errno));
	    goto elf_fail;
	  }
	  gettimeofday(&tv_end, NULL);
	  bw_info(phdr.p_memsz, &tv_start, &tv_end);
	}
      }

    elf_done:
      elf_end(elf);
      close(fd);

      /* Write command line */
      char *buf = strjoin(cur->argc-1, cur->argv + 1, ' ');
      strcpy(string_buffer + string_buffer_cur, buf);
      mbi.cmdline = string_buffer_cur;
      string_buffer_cur += strlen(buf) + 1;

      break;
    elf_fail:
      elf_end(elf);
      close(fd);
      goto fail;

    default:
      SLsmg_printf("Skipping unimplemented statement in config.\n");
    }
  }

  if (module_load_address == 0)
    goto fail;

  /* Modules */
  assert(current_module == total_modules);
  SLsmg_printf("Finalizing module info.\n");
  SLsmg_printf("String table at 0x%"PRIx64"\n", module_load_address);
  res = raw1394_write_large(fw_handle, node, module_load_address, string_buffer_cur,
                            (quadlet_t *)string_buffer);
  if (res == -1) {
    SLsmg_printf("Could not write string buffer: %s\n", strerror(errno));
    goto fail;
  }

  /* Fixing string offset */
  mbi.cmdline += module_load_address;
  for (unsigned i = 0; i < total_modules; i++)
    module_info[i].string += module_load_address;
  
  /* Write module info. */
  module_load_address += ROUND_UP_PAGE(string_buffer_cur);
  SLsmg_printf("Module info at 0x%"PRIx64"\n", module_load_address);
  res = raw1394_write_large(fw_handle, node, module_load_address, sizeof(struct module)*total_modules,
                            (quadlet_t *)module_info);
  if (res == -1) {
    SLsmg_printf("Could not write module info: %s\n", strerror(errno));
    goto fail;
  }

  /* Updating MBI and writing it back */
  mbi.mods_count = total_modules;
  mbi.mods_addr  = module_load_address;
  mbi.flags |= MBI_FLAG_MODS | MBI_FLAG_CMDLINE;
  res = raw1394_write_large(fw_handle, node, mbi_ptr, sizeof(struct mbi),
                            (quadlet_t *)(&mbi));
  if (res == -1) {
    SLsmg_printf("Could not write module info: %s\n", strerror(errno));
    goto fail;
  }

  SLsmg_printf("Starting the box...\n");
  SLsmg_refresh();
  
  res = raw1394_write_retry(fw_handle, node, boot_node_info->kernel_entry_point, sizeof(uint32_t),
                            (quadlet_t *)&entry_point);
  if (res == -1) {
    SLsmg_printf("Could not write ELF segment: %s\n", strerror(errno));
    /* XXX Cleanup */
    goto done;
  }

 fail: {}
 done:
  SLsmg_printf("Press a key to continue.\n");
  SLsmg_refresh();

  GC_gcollect();		/* Collect large temporary data. */
  while (!SLang_input_pending(-1)) {
  }
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

    struct node_info *info = collect_all_info(&nodes);

    switch (state) {
    case OVERVIEW:
      do_overview_screen(info);
      break;
    case BOOT:
      for (struct node_info *cur = info; cur != NULL; cur = cur->next) {
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
    SLsmg_printf("Hit q to quit | %d nodes | generation %u | libraw1394 %s | %zx/%zx %zx",
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

/* Local Variables: */
/* mode:outline-minor */
/* End: */
