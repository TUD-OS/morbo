/* From Nova. GPL'd */
#include <asm.h>
#include <serial.h>
#include <bda.h>
#include <util.h>

enum Port  {
  RHR         = 0,    // Receive Holding Register     (read)
  THR         = 0,    // Transmit Holding Register    (write)
  IER         = 1,    // Interrupt Enable Register    (write)
  ISR         = 2,    // Interrupt Status Register    (read)
  FCR         = 2,    // FIFO Control Register        (write)
  LCR         = 3,    // Line Control Register        (write)
  MCR         = 4,    // Modem Control Register       (write)
  LSR         = 5,    // Line Status Register         (read)
  MSR         = 6,    // Modem Status Register        (read)
  SPR         = 7,    // Scratchpad Register          (read/write)
  DLR_LOW     = 0,
  DLR_HIGH    = 1,
};

enum {
  IER_RHR_INTR        = 1u << 0,  // Receive Holding Register Interrupt
  IER_THR_INTR        = 1u << 1,  // Transmit Holding Register Interrupt
  IER_RLS_INTR        = 1u << 2,  // Receive Line Status Interrupt
  IER_MOS_INTR        = 1u << 3,  // Modem Status Interrupt

  FCR_FIFO_ENABLE     = 1u << 0,  // FIFO Enable
  FCR_RECV_FIFO_RESET = 1u << 1,  // Receiver FIFO Reset
  FCR_TMIT_FIFO_RESET = 1u << 2,  // Transmit FIFO Reset
  FCR_DMA_MODE        = 1u << 3,  // DMA Mode Select
  FCR_RECV_TRIG_LOW   = 1u << 6,  // Receiver Trigger LSB
  FCR_RECV_TRIG_HIGH  = 1u << 7,  // Receiver Trigger MSB

  ISR_INTR_STATUS     = 1u << 0,  // Interrupt Status
  ISR_INTR_PRIO       = 7u << 1,  // Interrupt Priority

  LCR_DATA_BITS_5     = 0u << 0,
  LCR_DATA_BITS_6     = 1u << 0,
  LCR_DATA_BITS_7     = 2u << 0,
  LCR_DATA_BITS_8     = 3u << 0,
  LCR_STOP_BITS_1     = 0u << 2,
  LCR_STOP_BITS_2     = 1u << 2,
  LCR_PARITY_ENABLE   = 1u << 3,
  LCR_PARITY_EVEN     = 1u << 4,
  LCR_PARITY_FORCE    = 1u << 5,
  LCR_BREAK           = 1u << 6,
  LCR_DLAB            = 1u << 7,

  MCR_DTR             = 1u << 0,  // Data Terminal Ready
  MCR_RTS             = 1u << 1,  // Request To Send
  MCR_GPO_1           = 1u << 2,  // General Purpose Output 1
  MCR_GPO_2           = 1u << 3,  // General Purpose Output 2
  MCR_LOOP            = 1u << 4,  // Loopback Check

  LSR_RECV_READY      = 1u << 0,
  LSR_OVERRUN_ERROR   = 1u << 1,
  LSR_PARITY_ERROR    = 1u << 2,
  LSR_FRAMING_ERROR   = 1u << 3,
  LSR_BREAK_INTR      = 1u << 4,
  LSR_TMIT_HOLD_EMPTY = 1u << 5,
  LSR_TMIT_EMPTY      = 1u << 6
};

static uint16_t serial_base;

void
serial_send (int c)
{
  while (!(inb (serial_base + LSR) & LSR_TMIT_HOLD_EMPTY)) {
    asm volatile ("pause");
  }

  outb (serial_base + THR, c);
}

void
serial_init()
{
  /* Disable output if there is no serial port. */
  /* XXX This is disabled, because it is not reliable. */
  //output_enabled = (serial_ports(get_bios_data_area()) > 0);

  /* Get our port from the BIOS data area. */
  serial_base = get_bios_data_area()->com_port[0];

  /* Programming the first serial adapter (8N1) */
  outb (serial_base + LCR, LCR_DLAB);
  
  /* 115200 baud */
  outb (serial_base + DLR_LOW,  1);
  outb (serial_base + DLR_HIGH, 0);
    
  /* 9600 baud */
  /*   outb (serial_base + DLR_LOW,  0x0C); */
  /*   outb (serial_base + DLR_HIGH, 0); */
  
  outb (serial_base + LCR, LCR_DATA_BITS_8 | LCR_STOP_BITS_1);
  outb (serial_base + IER, 0);
  outb (serial_base + FCR, FCR_FIFO_ENABLE | FCR_RECV_FIFO_RESET | FCR_TMIT_FIFO_RESET);
  outb (serial_base + MCR, MCR_DTR | MCR_RTS);
}

/* EOF */
