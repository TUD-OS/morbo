/* -*- Mode: C -*- */

#include <util.h>
#include <ohci.h>
#include <ohci-registers.h>
#include <ohci-crm.h>
#include <crc16.h>
#include <asm.h>

/* Globals */

static __attribute__ ((aligned(1024)))
ohci_config_rom_t   crom;

/* Some debugging macros */

#define OHCI_INFO(msg, args...) printf("OHCI: " msg, ## args)

/* Access to OHCI registers */
#define OHCI_REG(dev, reg) dev[reg/4]


#define DIR_TYPE_IMM    0
#define DIR_TYPE_CSROFF 1
#define DIR_TYPE_LEAF   2
#define DIR_TYPE_DIR    3

#define CROM_DIR_ENTRY(type, key_id, value) (((type) << 30) | ((key_id) << 24) | (value))

/* TODO This could be made nicer, but C sucks... */
static void
ohci_generate_crom(ohci_dev_t dev, ohci_config_rom_t *crom)
{
  /* Initialize with zero */
  for (unsigned i = 0; i < 0x100; i++) 
    crom->field[i] = 0;

  /* Copy the first quadlets */
  crom->field[1] = OHCI_REG(dev, BusID);
  out_description("BusID       : ", crom->field[1]);

  /* We dont want to be bus master. */
  crom->field[2] = OHCI_REG(dev, BusOptions) & 0x0FFFFFFF;
  out_description("BusOptions  : ", crom->field[2]);

  crom->field[3] = OHCI_REG(dev, GUIDHi);
  crom->field[4] = OHCI_REG(dev, GUIDLo);
  out_description("GUID_hi:", OHCI_REG(dev, GUIDHi));
  out_description("GUID_lo:", OHCI_REG(dev, GUIDLo));

  /* Config ROM is 4 bytes. Protect 4 bytes by CRC. */
  crom->field[0] = 0x04040000 | crc16(&(crom->field[1]), 4);

  /* Now we can generate a root directory */

  crom->field[5] = 1 << 16;   /* 1 word follow. Put CRC here later. */
  crom->field[6] = 0x81000001;  /* Module_Vendor_ID? */
  crom->field[5] |= crc16(&(crom->field[6]), 1);

  crom->field[7] = 0x0006 << 16; /* 6 */
  crom->field[8] = 0;
  crom->field[9] = 0;
  crom->field[10] = 'Morb';
  crom->field[11] = 'o - ';
  crom->field[12] = 'OHCI';
  crom->field[13] = '  v0';
  crom->field[7] |= crc16(&(crom->field[8]), 6);

}

static void
ohci_load_crom(ohci_dev_t dev, ohci_config_rom_t *crom)
{
  if (((uint32_t)crom) & 1023) {
    OHCI_INFO("Misaligned Config ROM!\n");
    /* XXX Should abort here. */
  }
  
  /* Update Info on OHC. This is done by writing to the address
     pointed to by the ConfigROMmap register. */
  uint32_t *phys_crom = (uint32_t *)OHCI_REG(dev, ConfigROMmap);
  for (unsigned i = 0; i < CONFIG_ROM_SIZE; i ++)
    phys_crom[i] = ntohl(crom->field[i]);
  
  OHCI_REG(dev, HCControlSet) = HCControl_BIBimageValid;

  if ((OHCI_REG(dev, HCControlSet) & HCControl_linkEnable) == 0) {
    /* We are not up yet, so set these manually. */
    OHCI_REG(dev, ConfigROMhdr) = crom->field[0];
    OHCI_REG(dev, BusOptions)   = crom->field[2];
  }
}

static void
wait_loop(ohci_dev_t dev, uint32_t reg, uint32_t mask, uint32_t value)
{
  const char waitchars[] = "|/-\\";
  unsigned i = 0;
  
  OHCI_INFO("Waiting...  ");

  while ((OHCI_REG(dev, reg) & mask) != value) {
    wait(1);
    printf("[D%c", waitchars[i++ & 3]);

  }
  printf("\n");
}


static uint8_t
phy_read(ohci_dev_t dev, uint8_t addr)
{
  OHCI_REG(dev, PhyControl) = PhyControl_Read(addr);
  wait_loop(dev, PhyControl, PhyControl_ReadDone, PhyControl_ReadDone);
  return PhyControl_ReadData(OHCI_REG(dev, PhyControl));
}

/** Write a OHCI PHY register.
 * \param dev the host controller.
 * \param addr the address to write to.
 * \param data the data to write.
 */
static void
phy_write(ohci_dev_t dev, uint8_t addr, uint8_t data)
{
  OHCI_REG(dev, PhyControl) = PhyControl_Write(addr, data);
  wait_loop(dev, PhyControl, PhyControl_WriteDone, PhyControl_WriteDone);
}

/** Force a bus reset.
 * \param dev the host controller.
 */
void
ohci_force_bus_reset(ohci_dev_t dev)
{
  uint8_t phy1 = phy_read(dev, 1);
  out_description("phy1 = ", phy1);
  OHCI_INFO("Bus reset.\n");

  /* Enable IBR in Phy 1 */
  /* XXX Set RHB to 0? */
  phy1 |= 1<<6;
  phy_write(dev, 1, phy1);
}

/** Perform a soft-reset of the Open Host Controller and wait until it
 *  has reinitialized itself.
 */
static void
ohci_softreset(ohci_dev_t dev)
{
  OHCI_INFO("Soft-resetting controller...\n");
  OHCI_REG(dev, HCControlSet) = HCControl_softReset;

  wait_loop(dev, HCControlSet, HCControl_softReset, 0);

  OHCI_INFO("Reset completed.\n");
}


bool
ohci_initialize(ohci_dev_t dev)
{
  // soft reset chip
  ohci_softreset(dev);

  // disable all including byte swapping
  OHCI_REG(dev, HCControlClear) = ~0U;

  // enable LinkPowerStatus
  OHCI_INFO("Start SCLK.\n");
  OHCI_REG(dev, HCControlSet) = HCControl_LPS;

  /* Wait for LPS to come up. */
  wait_loop(dev, HCControlSet, HCControl_LPS, HCControl_LPS);

  // wait some time to let device enable the link
  wait(50);

  /* Phy dump */
  /* XXX Reading PHY0 loops forever in wait_loop. Link probably needs to be up for that. */
  for (unsigned i = 1; i < 4; i++) {
    OHCI_INFO("phy[%d] = 0x%x\n", i, phy_read(dev, i));
  }

  // reset Link Control register
  OHCI_REG(dev, LinkControlClear) = 0xFFFFFFFF;

  // accept requests from all nodes
  OHCI_REG(dev, AsReqFilterHiSet) = 0x80000000;
  OHCI_REG(dev, AsReqFilterLoSet) = 0xFFFFFFFF;

  // accept physical requests from all nodes in our local bus
  OHCI_REG(dev, PhyReqFilterHiSet) = 0x7FFFFFFF;
  OHCI_REG(dev, PhyReqFilterLoSet) = 0xFFFFFFFF;

  // allow access up to 0xffff00000000
  OHCI_REG(dev, PhyUpperBound) = 0xFFFF0000;

  // we retry because of a busy partner
  OHCI_REG(dev, ATRetries) = 0x822;

  if (OHCI_REG(dev, HCControlSet) & HCControl_linkEnable) {
    OHCI_INFO("Link is already enabled. Why?!\n");
  }

  /* Set Config ROM */
  OHCI_INFO("Updating config ROM.\n");
  ohci_generate_crom(dev, &crom);
  ohci_load_crom(dev, &crom);

  /* enable link */
  OHCI_REG(dev, HCControlSet) = HCControl_linkEnable;

  /* Enable ports */
  uint8_t phy_2 = phy_read(dev, 2);
  uint8_t ports = phy_2 & ((1<<5) - 1);
  bool enhanced_registers = ((phy_2 >> 5) & 1) == 1;
  uint8_t topspeed = phy_2 >> 6;
  OHCI_INFO("Card has %d ports.\n"
	    "Enhanced PHY: %s\n"
	    "Top speed   : %d\n",
	    ports, enhanced_registers ? "yes" : "no", topspeed);

#warning TODO Dump port info.

  /* Wait for link to come up. */
  wait_loop(dev, HCControlSet, HCControl_linkEnable, HCControl_linkEnable);

  // display link speed and max size of packets
  OHCI_INFO("Bus Options: 0x%x\n", OHCI_REG(dev, BusOptions));
  OHCI_INFO("HCControl:   0x%x\n", OHCI_REG(dev, HCControlSet));

  /* Display GUID */
  OHCI_INFO("GUID_hi:     0x%x\n", OHCI_REG(dev, GUIDHi));
  OHCI_INFO("GUID_lo:     0x%x\n", OHCI_REG(dev, GUIDLo));

  return true;
}

/** Wait until we get a vaild bus number. */
uint8_t
ohci_wait_nodeid(ohci_dev_t dev)
{
  wait_loop(dev, NodeID, NodeID_idValid, NodeID_idValid);

  uint32_t nodeid_reg = OHCI_REG(dev, NodeID);
  uint8_t node_number = nodeid_reg & NodeID_nodeNumber;

  return node_number;
}


/* EOF */
