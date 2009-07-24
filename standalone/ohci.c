/* -*- Mode: C -*- */

/* Based on fulda. Original copyright: */
/*
 * Copyright (C) 2007  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the FULDA package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */


#include <stdbool.h>

#include <morbo.h>

#include <util.h>
#include <ohci.h>
#include <ohci-registers.h>
#include <ohci-crm.h>
#include <crc16.h>
#include <asm.h>

/* Constants */

#define NEVER 0xFFFFFFFFUL
#define RESET_TIMEOUT 10000
#define PHY_TIMEOUT   10000
#define MISC_TIMEOUT  10000

// #define FANCY_WAIT

/* Globals */

static __attribute__ ((aligned(1024)))
ohci_config_rom_t   crom;

/* XXX Should be in struct ohci_controller. */
static __attribute__ ((aligned(2048)))
uint32_t selfid_buf[504];

/* Some debugging macros */

#define OHCI_INFO(msg, args...) printf("OHCI: " msg, ## args)

/* Access to OHCI registers */
#define OHCI_REG(dev, reg) (dev->ohci_regs[reg/4])


#define DIR_TYPE_IMM    0
#define DIR_TYPE_CSROFF 1
#define DIR_TYPE_LEAF   2
#define DIR_TYPE_DIR    3

#define CROM_DIR_ENTRY(type, key_id, value) (((type) << 30) | ((key_id) << 24) | (value))

/* TODO This could be made nicer, but C sucks... */
static void
ohci_generate_crom(struct ohci_controller *ohci, ohci_config_rom_t *crom)
{
  /* Initialize with zero */
  memset(crom, 0, sizeof(ohci_config_rom_t));

  /* Copy the first quadlets */
  crom->field[1] = OHCI_REG(ohci, BusID);

  /* We dont want to be bus master. */
  crom->field[2] = OHCI_REG(ohci, BusOptions) & 0x0FFFFFFF;

  crom->field[3] = OHCI_REG(ohci, GUIDHi);
  crom->field[4] = OHCI_REG(ohci, GUIDLo);

  /* Protect 4 words by CRC. */
  crom->field[0] = 0x04040000 | crc16(&(crom->field[1]), 4);

  /* Now we can generate a root directory */

  crom->field[5] = 4 << 16;   /* 4 words follow. Put CRC here later. */
  crom->field[6] = 0x03 << 24 | MORBO_VENDOR_ID; /* Immediate */
  crom->field[7] = 0x17 << 24 | MORBO_MODEL_ID;	 /* Immediate */
  crom->field[8] = 0x81 << 24 | 2;		 /* Text descriptor */
  crom->field[9] = MORBO_INFO_DIR << 24 | 8;	 /* Leaf */
  crom->field[5] |= crc16(&(crom->field[6]), 4);

  crom->field[10] = 0x0006 << 16; /* 6 words follow */
  crom->field[11] = 0;
  crom->field[12] = 0;
  crom->field[13] = 'Morb';
  crom->field[14] = 'o - ';
  crom->field[15] = 'OHCI';
  crom->field[16] = ' v1\0';
  crom->field[10] |= crc16(&(crom->field[11]), 6);

  crom->field[17] = 0x002 << 16; /* 1 words follow */
  crom->field[18] = (uint32_t)&multiboot_info; /* Pointer to pointer to multiboot info */
  crom->field[19] = (uint32_t)&kernel_entry_point; /* Pointer to uint32_t */
  crom->field[17] |= crc16(&(crom->field[18]), 2);

}

static void
ohci_load_crom(struct ohci_controller *ohci, ohci_config_rom_t *crom)
{
  if (((uint32_t)crom) & 1023) {
    OHCI_INFO("Misaligned Config ROM!\n");
    /* XXX Should abort here. */
  }
  
  /* Update Info on OHC. This is done by writing to the address
     pointed to by the ConfigROMmap register. */
  uint32_t *phys_crom = (uint32_t *)OHCI_REG(ohci, ConfigROMmap);
  for (unsigned i = 0; i < CONFIG_ROM_SIZE; i ++)
    phys_crom[i] = ntohl(crom->field[i]);
  
  OHCI_REG(ohci, HCControlSet) = HCControl_BIBimageValid;

  if ((OHCI_REG(ohci, HCControlSet) & HCControl_linkEnable) == 0) {
    /* We are not up yet, so set these manually. */
    OHCI_REG(ohci, ConfigROMhdr) = crom->field[0];
    OHCI_REG(ohci, BusOptions)   = crom->field[2];
  }
}

static void
wait_loop(struct ohci_controller *ohci, uint32_t reg, uint32_t mask, uint32_t value, uint32_t max_ticks)
{
#ifdef FANCY_WAIT
  const char waitchars[] = "|/-\\";
  OHCI_INFO("Waiting...  ");
#endif
  unsigned i = 0;

  while ((OHCI_REG(ohci, reg) & mask) != value) {
    wait(1);
#ifdef FANCY_WAIT
    printf("[D%c", waitchars[i++ & 3]);
#endif
    assert((max_ticks == NEVER) || (i < max_ticks), "Timeout!");
  }
#ifdef FANCY_WAIT
  printf("\n");
#endif
}


static uint8_t
phy_read(struct ohci_controller *ohci, uint8_t addr)
{
  OHCI_REG(ohci, PhyControl) = PhyControl_Read(addr);
  wait_loop(ohci, PhyControl, PhyControl_ReadDone, PhyControl_ReadDone, PHY_TIMEOUT);

  uint8_t result = PhyControl_ReadData(OHCI_REG(ohci, PhyControl));
  return result;
}

/** Write a OHCI PHY register.
 * \param dev the host controller.
 * \param addr the address to write to.
 * \param data the data to write.
 */
static void
phy_write(struct ohci_controller *ohci, uint8_t addr, uint8_t data)
{
  OHCI_REG(ohci, PhyControl) = PhyControl_Write(addr, data);
  wait_loop(ohci, PhyControl, PhyControl_WriteDone, 0, PHY_TIMEOUT);
}

static void
phy_page_select(struct ohci_controller *ohci, enum phy_page page, uint8_t port)
{
  assert(ohci->enhanced_phy_map, "no page select possible");
  assert(port < 16, "bad port");
  assert(page <  7, "bad page");

  phy_write(ohci, 7, (page << 5) | port);
}

/** Force a bus reset.
 * \param dev the host controller.
 */
void
ohci_force_bus_reset(struct ohci_controller *ohci)
{
  uint8_t phy1 = phy_read(ohci, 1);
  out_description("phy1 = ", phy1);
  OHCI_INFO("Bus reset.\n");

  /* Enable IBR in Phy 1 */
  /* XXX Set RHB to 0? */
  phy1 |= 1<<6;
  phy_write(ohci, 1, phy1);
}

/** Perform a soft-reset of the Open Host Controller and wait until it
 *  has reinitialized itself.
 */
static void
ohci_softreset(struct ohci_controller *ohci)
{
  OHCI_INFO("Soft-resetting controller...\n");
  OHCI_REG(ohci, HCControlSet) = HCControl_softReset;
  wait_loop(ohci, HCControlSet, HCControl_softReset, 0, RESET_TIMEOUT);
  OHCI_INFO("Reset completed.\n");
}

/* Check version of controller. Returns true, if it is supported. */
static bool
ohci_check_version(struct ohci_controller *ohci)
{
  /* Check version of OHCI controller. */
  uint32_t ohci_version_reg = OHCI_REG(ohci, Version);

  assert(ohci_version_reg != 0xFFFFFFFF, "Invalid PCI data?");

  bool     guid_rom      = (ohci_version_reg >> 24) == 1;
  uint8_t  ohci_version  = (ohci_version_reg >> 16) & 0xFF;
  uint8_t  ohci_revision = ohci_version_reg & 0xFF;

  printf("GUID = %s, version = %x, revision = %x\n",
	 guid_rom ? "yes" : "no?!", ohci_version, ohci_revision);
  
  if ((ohci_version <= 1) && (ohci_revision < 10)) {
    printf("Controller implements OHCI %d.%d. But we require at least 1.10.\n",
	   ohci_version, ohci_revision);
    return false;
  }

  return true;
}

bool
ohci_initialize(const struct pci_device *pci_dev,
		struct ohci_controller *ohci)
{
  assert(ohci != NULL, "Invalid pointer");
  ohci->pci = pci_dev;
  ohci->ohci_regs = (volatile uint32_t *) pci_cfg_read_uint32(ohci->pci, PCI_CFG_BAR0);

  ohci->as_request_filter  = ~0LLU; /* Enable access from all nodes */
  ohci->phy_request_filter = ~0LLU; /* to all memory. */

  assert((uint32_t)ohci->ohci_regs != 0xFFFFFFFF, "Invalid PCI read?");

  printf("OHCI registers are at 0x%x.\n", ohci->ohci_regs);
  printf("OHCI (%x:%x) is a %s.\nQuirks: %x\n", 
	 pci_dev->db->vendor_id,
	 pci_dev->db->device_id,
	 pci_dev->db->device_name,
	 pci_dev->db->quirks);

  if (ohci->ohci_regs == NULL) {
    printf("Uh? OHCI register pointer is NULL.\n");
    return false;
  }

  if (!ohci_check_version(ohci)) {
    return false;
  }

  /* Do a softreset. */
  ohci_softreset(ohci);
  
  /* Disable linkEnable to be able to configure the low level stuff. */
  OHCI_REG(ohci, HCControlClear) = HCControl_linkEnable;
  wait_loop(ohci, HCControlSet, HCControl_linkEnable, 0, MISC_TIMEOUT);

  /* Disable stuff we don't want/need, including byte swapping. */
  OHCI_REG(ohci, HCControlClear) = HCControl_noByteSwapData | HCControl_ackTardyEnable;

  /* Enable posted writes. With posted writes enabled, the controller
     may return ack_complete for physical write requests, even if the
     data has not been written yet. For coherency considerations,
     refer to Chapter 3.3.3 in the OHCI spec. */
  OHCI_REG(ohci, HCControlSet) = HCControl_postedWriteEnable;

  /* Enable LinkPower Status. It should be on already, but what the
     heck... If it is disabled, only some OHCI registers are
     accessible and we cannot communicate with the PHY. */
  OHCI_INFO("Enable LPS.\n");
  OHCI_REG(ohci, HCControlSet) = HCControl_LPS;
  wait_loop(ohci, HCControlSet, HCControl_LPS, HCControl_LPS, MISC_TIMEOUT);
  OHCI_INFO("LPS is up.\n");

  /* Disable contender bit */
  uint8_t phy4 = phy_read(ohci, 4);
  OHCI_INFO("phy4 = %x\n", phy4);
  phy_write(ohci, 4, phy4 & ~0x40);

  /* LPS is up. We can now communicate with the PHY. Discover how many
     ports we have and whether this PHY supports the enhanced register
     map. */
  uint8_t phy_2 = phy_read(ohci, 2);
  ohci->total_ports = phy_2 & ((1<<5) - 1);
  ohci->enhanced_phy_map = (phy_2 >> 5) == 7;

  OHCI_INFO("Controller has %d ports and %s enhanced PHY map.\n",
	    ohci->total_ports,
	    ohci->enhanced_phy_map ? "an" : "no");

  if (ohci->enhanced_phy_map) {

    /* Enable all ports. */
    for (unsigned port = 0; port < ohci->total_ports; port++) {


      phy_page_select(ohci, PORT_STATUS, port);

      uint8_t reg0 = phy_read(ohci, 8);
      if ((reg0 & PHY_PORT_DISABLED) != 0) {
	OHCI_INFO("Enabling port %d.\n", port);
	phy_write(ohci, 8, reg0 & ~PHY_PORT_DISABLED);
      } else {
	OHCI_INFO("Port %d is already enabled.\n", port);
      }
    }
  }

  /* Check if we are responsible for configuring IEEE1394a
     enhancements. */
  if (OHCI_REG(ohci, HCControlSet) & HCControl_programPhyEnable) {
    OHCI_INFO("Enabling IEEE1394a enhancements.\n");
    OHCI_REG(ohci, HCControlSet) = HCControl_aPhyEnhanceEnable;

  } else {
    OHCI_INFO("IEEE1394a enhancements are already configured.\n");
  }

  // wait some time to let device enable the link
  // wait(50);

  // reset Link Control register
  OHCI_REG(ohci, LinkControlClear) = 0xFFFFFFFF;

  // accept requests from all nodes
  OHCI_REG(ohci, AsReqFilterHiSet) = ohci->as_request_filter >> 32;
  OHCI_REG(ohci, AsReqFilterLoSet) = ohci->as_request_filter & 0xFFFFFFFF;

  // accept physical requests from all nodes in our local bus
  OHCI_REG(ohci, PhyReqFilterHiSet) = ohci->phy_request_filter >> 32;
  OHCI_REG(ohci, PhyReqFilterLoSet) = ohci->phy_request_filter & 0xFFFFFFFF;

  // allow access up to 0xffff00000000
  OHCI_REG(ohci, PhyUpperBound) = 0xFFFF0000;
  if (OHCI_REG(ohci, PhyUpperBound) == 0) {
    OHCI_INFO("PhyUpperBound doesn't seem to be implemented. (No cause for alarm.)\n");
  }

  /* Set SelfID buffer */
  selfid_buf[0] = 0xDEADBEEF;		/* error checking */
  OHCI_REG(ohci, SelfIDBuffer) = (uint32_t)selfid_buf;
  OHCI_REG(ohci, LinkControlSet) = LinkControl_rcvSelfID;

  OHCI_INFO("Generation: %x\n", (OHCI_REG(ohci, SelfIDCount) >> 16) & 0xFF);

  // we retry because of a busy partner
  OHCI_REG(ohci, ATRetries) = 0xFFF;

  if (OHCI_REG(ohci, HCControlSet) & HCControl_linkEnable) {
    OHCI_INFO("Link is already enabled. Why?!\n");
  }

  /* Set Config ROM */
  OHCI_INFO("Updating config ROM.\n");
  ohci_generate_crom(ohci, &crom);
  ohci_load_crom(ohci, &crom);

  /* enable link */
  OHCI_REG(ohci, HCControlSet) = HCControl_linkEnable;
  /* Wait for link to come up. */
  wait_loop(ohci, HCControlSet, HCControl_linkEnable, HCControl_linkEnable, MISC_TIMEOUT);
  OHCI_INFO("Link is up.\n");

  /* Print GUID for easy reference. */
  OHCI_INFO("GUID: %llx\n", (uint64_t)(OHCI_REG(ohci, GUIDHi)) << 32 | OHCI_REG(ohci, GUIDLo));

  return true;
}

/** Handle a bus reset condition. Does not return until the reset is
    handled. */
void
ohci_handle_bus_reset(struct ohci_controller *ohci)
{
  /* We have to clear ContextControl.run */
  OHCI_REG(ohci, AsReqTrContextControlClear) = 1 << 15;
  OHCI_REG(ohci, AsRspTrContextControlClear) = 1 << 15;
  
  /* Wait for active DMA to finish. (We don't do DMA... ) */
  wait_loop(ohci, AsReqTrContextControlSet, ATactive, 0, 10);
  wait_loop(ohci, AsRspTrContextControlSet, ATactive, 0, 10);
  
  /* Wait for completion of SelfID phase. */
  assert(OHCI_REG(ohci, LinkControlSet) & LinkControl_rcvSelfID,
	 "selfID receive borken");
  wait_loop(ohci, IntEventSet, selfIDComplete, selfIDComplete, 1000);
  OHCI_INFO("Bus reset complete. Clearing event bits.\n");
  
  /* We are done. Clear bus reset bit. */
  OHCI_REG(ohci, IntEventClear) = busReset;
  
  /* Reset request filters. They are cleared on bus reset. */
  OHCI_REG(ohci, AsReqFilterHiSet) = ohci->as_request_filter >> 32;
  OHCI_REG(ohci, AsReqFilterLoSet) = ohci->as_request_filter & 0xFFFFFFFF;
  OHCI_REG(ohci, PhyReqFilterHiSet) = ohci->phy_request_filter >> 32;
  OHCI_REG(ohci, PhyReqFilterLoSet) = ohci->phy_request_filter & 0xFFFFFFFF;

  printf("AsReqFilter   %x %x\n", OHCI_REG(ohci, AsReqFilterHiSet), OHCI_REG(ohci, AsReqFilterLoSet));
  printf("PhyReqFilter  %x %x\n", OHCI_REG(ohci, PhyReqFilterHiSet), OHCI_REG(ohci, PhyReqFilterLoSet));
  
  uint32_t selfid_count = OHCI_REG(ohci, SelfIDCount);
  uint8_t  selfid_words = (selfid_count >> 2) & 0xFF;
  OHCI_INFO("SelfID generation 0x%x, bytes 0x%x\n",
	    (selfid_count >> 16) & 0xFF,
	    selfid_words * 4);

  for (unsigned i = 0; i < selfid_words; i++) {
    assert(i < sizeof(selfid_buf)/sizeof(uint32_t), "buffer overflow");
    OHCI_INFO("SelfID buf[0x%x] = 0x%x\n", i, selfid_buf[i]);
  }

}

void
ohci_poll_events(struct ohci_controller *ohci)
{
  uint32_t intevent = OHCI_REG(ohci, IntEventSet); /* Unmasked event bitfield */
    
  if ((intevent & busReset) != 0) {
    OHCI_INFO("Bus reset!\n");
    ohci_handle_bus_reset(ohci);
  } else if ((intevent & postedWriteErr) != 0) {
    OHCI_INFO("Posted Write Error\n");
    OHCI_REG(ohci, IntEventClear) = postedWriteErr;
  } else if ((intevent & unrecoverableError) != 0) {
    OHCI_INFO("Unrecoverable Error\n");
    OHCI_REG(ohci, IntEventClear) = unrecoverableError;
  }
}

/** Wait until we get a valid bus number. */
uint8_t
ohci_wait_nodeid(struct ohci_controller *ohci)
{
  wait_loop(ohci, NodeID, NodeID_idValid, NodeID_idValid, MISC_TIMEOUT);

  uint32_t nodeid_reg = OHCI_REG(ohci, NodeID);
  uint8_t node_number = nodeid_reg & NodeID_nodeNumber;

  return node_number;
}


/* EOF */
