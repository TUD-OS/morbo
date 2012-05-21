/* -*- Mode: C -*- */
/*
 * OHCI registers
 *
 * Copyright (C) 2009-2012, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Morbo.
 *
 * Morbo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Morbo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

/* This file adapted from Linux 2.6.28-rc1 /drivers/firewire/fw-ohci.h */

#pragma once

/* OHCI register map */

#define Version                      0x000
#define GUID_ROM                     0x004
#define ATRetries                    0x008
#define CSRData                      0x00C
#define CSRCompareData               0x010
#define CSRControl                   0x014
#define ConfigROMhdr                 0x018
#define BusID                        0x01C
#define BusOptions                   0x020
#define  BusOptions_irmc             (1<<31)
#define  BusOptions_cmc              (1<<30)
#define  BusOptions_isc              (1<<29)
#define  BusOptions_bmc              (1<<28)
#define GUIDHi                       0x024
#define GUIDLo                       0x028
#define ConfigROMmap                 0x034
#define PostedWriteAddressLo         0x038
#define PostedWriteAddressHi         0x03C
#define VendorID                     0x040
#define HCControlSet                 0x050
#define HCControlClear               0x054
#define  HCControl_BIBimageValid	0x80000000
#define  HCControl_noByteSwapData	0x40000000
#define  HCControl_ackTardyEnable	0x20000000
#define  HCControl_programPhyEnable	0x00800000
#define  HCControl_aPhyEnhanceEnable	0x00400000
#define  HCControl_LPS			0x00080000
#define  HCControl_postedWriteEnable	0x00040000
#define  HCControl_linkEnable		0x00020000
#define  HCControl_softReset		0x00010000
#define SelfIDBuffer                 0x064
#define SelfIDCount                  0x068
#define  SelfIDCount_selfIDError	0x80000000
#define IRMultiChanMaskHiSet         0x070
#define IRMultiChanMaskHiClear       0x074
#define IRMultiChanMaskLoSet         0x078
#define IRMultiChanMaskLoClear       0x07C
#define IntEventSet                  0x080
#define IntEventClear                0x084
#define IntMaskSet                   0x088
#define IntMaskClear                 0x08C
#define IsoXmitIntEventSet           0x090
#define IsoXmitIntEventClear         0x094
#define IsoXmitIntMaskSet            0x098
#define IsoXmitIntMaskClear          0x09C
#define IsoRecvIntEventSet           0x0A0
#define IsoRecvIntEventClear         0x0A4
#define IsoRecvIntMaskSet            0x0A8
#define IsoRecvIntMaskClear          0x0AC
#define InitialBandwidthAvailable    0x0B0
#define InitialChannelsAvailableHi   0x0B4
#define InitialChannelsAvailableLo   0x0B8
#define FairnessControl              0x0DC
#define LinkControlSet               0x0E0
#define LinkControlClear             0x0E4
#define   LinkControl_rcvSelfID	(1 << 9)
#define   LinkControl_rcvPhyPkt	(1 << 10)
#define   LinkControl_cycleTimerEnable	(1 << 20)
#define   LinkControl_cycleMaster	(1 << 21)
#define   LinkControl_cycleSource	(1 << 22)
#define NodeID                       0x0E8
#define   NodeID_idValid             0x80000000
#define   NodeID_nodeNumber          0x0000003f
#define   NodeID_busNumber           0x0000ffc0
#define PhyControl                   0x0EC
#define   PhyControl_Read(addr)	(((addr) << 8) | 0x00008000)
#define   PhyControl_ReadDone		0x80000000
#define   PhyControl_ReadData(r)	(((r) & 0x00ff0000) >> 16)
#define   PhyControl_Write(addr, data)	(((addr) << 8) | (data) | 0x00004000)
#define   PhyControl_WriteDone		0x00004000
#define IsochronousCycleTimer        0x0F0
#define AsReqFilterHiSet             0x100
#define AsReqFilterHiClear           0x104
#define AsReqFilterLoSet             0x108
#define AsReqFilterLoClear           0x10C
#define PhyReqFilterHiSet            0x110
#define PhyReqFilterHiClear          0x114
#define PhyReqFilterLoSet            0x118
#define PhyReqFilterLoClear          0x11C
#define PhyUpperBound                0x120

enum phy_page {
  PORT_STATUS = 0,
  VENDOR_INFO = 1,
};

enum phy_port_info {
  /* PHY 8 a.k.a. Register 0 */
  PHY_PORT_DISABLED  = 1 << 0,
  PHY_PORT_CONNECTED = 1 << 2,
  PHY_PORT_CHILD     = 1 << 3,
};

#define ATactive                     (1<<10)
#define ATrun                        (1<<15)

#define AsReqTrContextBase           0x180
#define AsReqTrContextControlSet     0x180
#define AsReqTrContextControlClear   0x184
#define AsReqTrCommandPtr            0x18C

#define AsRspTrContextBase           0x1A0
#define AsRspTrContextControlSet     0x1A0
#define AsRspTrContextControlClear   0x1A4
#define AsRspTrCommandPtr            0x1AC

#define AsReqRcvContextBase          0x1C0
#define AsReqRcvContextControlSet    0x1C0
#define AsReqRcvContextControlClear  0x1C4
#define AsReqRcvCommandPtr           0x1CC

#define AsRspRcvContextBase          0x1E0
#define AsRspRcvContextControlSet    0x1E0
#define AsRspRcvContextControlClear  0x1E4
#define AsRspRcvCommandPtr           0x1EC

/* Isochronous transmit registers */
#define IsoXmitContextBase(n)           (0x200 + 16 * (n))
#define IsoXmitContextControlSet(n)     (0x200 + 16 * (n))
#define IsoXmitContextControlClear(n)   (0x204 + 16 * (n))
#define IsoXmitCommandPtr(n)            (0x20C + 16 * (n))

/* Isochronous receive registers */
#define IsoRcvContextBase(n)         (0x400 + 32 * (n))
#define IsoRcvContextControlSet(n)   (0x400 + 32 * (n))
#define IsoRcvContextControlClear(n) (0x404 + 32 * (n))
#define IsoRcvCommandPtr(n)          (0x40C + 32 * (n))
#define IsoRcvContextMatch(n)        (0x410 + 32 * (n))

/* Interrupts Mask/Events */
#define reqTxComplete		0x00000001
#define respTxComplete		0x00000002
#define ARRQ			0x00000004
#define ARRS			0x00000008
#define RQPkt			0x00000010
#define RSPkt			0x00000020
#define isochTx		0x00000040
#define isochRx		0x00000080
#define postedWriteErr		0x00000100
#define lockRespErr		0x00000200
#define selfIDComplete2		0x00008000
#define selfIDComplete		0x00010000
#define busReset		0x00020000
#define regAccessFail		0x00040000
#define phy			0x00080000
#define cycleSynch		0x00100000
#define cycle64Seconds		0x00200000
#define cycleLost		0x00400000
#define cycleInconsistent	0x00800000
#define unrecoverableError	0x01000000
#define cycleTooLong		0x02000000
#define phyRegRcvd		0x04000000
#define masterIntEnable	0x80000000

#define evt_no_status		0x0
#define evt_long_packet	0x2
#define evt_missing_ack	0x3
#define evt_underrun		0x4
#define evt_overrun		0x5
#define evt_descriptor_read	0x6
#define evt_data_read		0x7
#define evt_data_write		0x8
#define evt_bus_reset		0x9
#define evt_timeout		0xa
#define evt_tcode_err		0xb
#define evt_reserved_b		0xc
#define evt_reserved_c		0xd
#define evt_unknown		0xe
#define evt_flushed		0xf

#define phy_tcode		0xe

/* EOF */
