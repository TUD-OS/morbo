// -*- Mode: C++ -*-

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <brimstone2.h>
#include "morbo-packet.h"

enum tcode {
  BLOCK_WRITE_REQUEST   = 0x1,
  BLOCK_READ_REQUEST    = 0x5,
  QUADLET_READ_RESPONSE = 0x6,
  BLOCK_READ_RESPONSE   = 0x7,
};

#define MAX_PACKET_SIZE 2048
#define MAX_LABELS      (1<<6)

namespace Brimstone {

  template <typename T>
  class FreeGuard {
  protected:
    T *_ptr;
  public:
    FreeGuard(T *obj)
      : _ptr(obj)
    {}

    ~FreeGuard() {
      delete _ptr;
    }
  };

  class Packet {
  public:
    packet_header header;
    uint32_t *data;
    

    uint8_t tcode() const { return (data[0] & 0xF0) >> 4; }
    uint8_t tlabel() const { return (data[0] & 0xFF00) >> 10; }

    Packet(packet_header h)
      : header(h)
    {
      data = (uint32_t *)(new char[header.size]);
    }

    ~Packet() {
      delete data;
    }
  };

  Packet *
  Bus::read_packet()
  {
    packet_header h;
    read(_fd, &h, sizeof(h));
    
    Packet *p = new Packet(h);
    read(_fd, p->data, h.size);

    return p;
  }

  Bus::Bus(const char *dev)
    : tlabel(0)
  {
    _fd = open(dev, O_RDWR);
    if (_fd < 0) {
      perror("open");
      // XXX Don't exit. Throw exception instead.
      exit(EXIT_FAILURE);
    }
  }

  Bus::~Bus()
  {
    close(_fd);
  }

  uint16_t
  Bus::node_id()
  {
    morbo_selfid_buffer *si = new morbo_selfid_buffer;

    ioctl(_fd, MORBO_IOC_GET_SELFID, si);

    uint16_t node_id = si->node_id & 0xFFFF;
    delete si;

    return node_id;
  }

  uint32_t
  Bus::quadlet_read(uint32_t node, uint64_t address)
  {
    uint8_t label = tlabel++;
    struct {
      packet_header h;
      uint32_t buf[3];
    } p;

    p.h.size = sizeof(p.buf);
    p.h.event = MORBO_PACKET_REQ;

    p.buf[0] =
      /* 400MBit/s */ 2<<16 |
      /* label */     (label&0x3F) << 10 |
      /* retry_X   */ 1<<8  |
      /* tCode     */ 4<<4;
    p.buf[1] = node << 16 | (address >> 32);
    p.buf[2] = address & 0xFFFFFFFFULL;

    write(_fd, &p, sizeof(p));

    while (1) {
      Packet *p = read_packet();
      FreeGuard<Packet> pg(p);

      if ((p->tcode() == QUADLET_READ_RESPONSE) &&
	  (p->tlabel() == label)) {
	return p->data[3];
      }

      printf("Skipping packet: tcode=%x, tlabel=%x\n", p->tcode(), p->tlabel());
      // XXX Implement some kind of timeout.
    }
  }

  void
  Bus::quadlet_write(uint32_t node, uint64_t address, uint32_t data)
  {
    uint8_t label = tlabel++;
    struct {
      packet_header h;
      uint32_t buf[4];
    } p;
    
    p.h.size = sizeof(p.buf);
    p.h.event = MORBO_PACKET_REQ;
    
    p.buf[0] =
      /* 400MBit/s */ 2<<16 |
      /* label */     (label&0x3F) << 10 |
      /* retry_X   */ 1<<8  |
      /* tCode     */ 0<<4;

    p.buf[1] = node << 16 | (address >> 32);
    p.buf[2] = address & 0xFFFFFFFFULL;
    p.buf[3] = data;
    write(_fd, &p, sizeof(p));
  }

  void
  Bus::post_read(uint32_t node, uint64_t address, void *buf, size_t buf_size)
  {
    // Split read into blocks of 128K
    const unsigned max_block = MAX_LABELS * MAX_PACKET_SIZE;
    if (buf_size > max_block) {
      post_read(node, address, buf, max_block);
      post_read(node, address + max_block, (void *)(max_block + (char *)buf),
		buf_size - max_block);
      return;
    }

    unsigned chunks = (buf_size + 2047) / 2048;
    unsigned chunks_rcvd = 0;

    // Sent blocks with tlabels starting from 0. The tlabels can then
    // be used to reconstruct the position in the buffer.
    ssize_t read_bytes = buf_size;
    for (unsigned i = 0; i < chunks; i++) {
      struct {
	packet_header h;
	uint32_t buf[4];
      } p;

      p.h.size = sizeof(p.buf);
      p.h.event = MORBO_PACKET_REQ;

      uint64_t p_addr = address + i*MAX_PACKET_SIZE;
      
      p.buf[0] =
	/* 400MBit/s */ 2<<16 |
	/* label */     (i&0x3F) << 10 |
	/* retry_X   */ 1<<8  |
	/* tCode     */ BLOCK_READ_REQUEST<<4;
      p.buf[1] = node << 16 | (p_addr >> 32);
      p.buf[2] = p_addr & 0xFFFFFFFFULL;
      p.buf[3] = ((read_bytes > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : read_bytes) << 16;

      write(_fd, &p, sizeof(p));

      // This is only wrong for the last iteration, when it doesn't
      // matter anymore.
      read_bytes -= MAX_PACKET_SIZE;
    }

    // The requests are sent. Start receiving.
    while (chunks_rcvd < chunks) {
      Packet *p = read_packet();
      FreeGuard<Packet> pg(p);

      if ((p->tcode() == BLOCK_READ_RESPONSE) &&
	  (p->tlabel() < chunks)) {
	memcpy(p->tlabel()*MAX_PACKET_SIZE + (char *)buf, &(p->data[4]), p->data[3]>>16);
	chunks_rcvd++;
	continue;
      }

      printf("Skipping packet: tcode=%x, tlabel=%x\n", p->tcode(), p->tlabel());
      // XXX Implement some kind of timeout.
    }

  }

  void
  Bus::post_write(uint32_t node, uint64_t address, const void *buf, size_t buf_size)
  {
    size_t offset = 0;

    while (buf_size > 0) {
      uint16_t packet_size = (buf_size > 2048) ? 2048 : buf_size;
      struct sp {
	packet_header h;
	uint32_t data[];
      };
      sp *p = (sp *)malloc(sizeof(packet_header) + 4*sizeof(uint32_t) + packet_size);

      p->h.size = 4*sizeof(uint32_t) + packet_size;
      p->h.event = MORBO_PACKET_REQ;
      p->data[0] =
	/* 400MBit/s */ 2<<16 |
	/* retry_X   */ 1<<8  |
	/* tCode     */ BLOCK_WRITE_REQUEST << 4;
      p->data[1] = node << 16 | (address >> 32);
      p->data[2] = address & 0xFFFFFFFFULL;
      p->data[3] = packet_size << 16;

      memcpy(&p->data[4], offset + (const char *)buf, packet_size);
      write(_fd, p, sizeof(packet_header) + 4*sizeof(uint32_t) + packet_size);
      free(p);

      buf_size -= packet_size;
      offset   += packet_size;
    }
  }

}

// EOF
