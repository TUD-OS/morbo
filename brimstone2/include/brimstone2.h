// -*- Mode: C++ -*-

#pragma once

#include <cstdint>
#include <iostream>
#include <exception>

namespace Brimstone {

  using namespace std;

  class Packet;

  class Bus {
  protected:
    int _fd;
    uint8_t tlabel;

    Packet *read_packet();

  public:
    Bus(const char *dev);
    ~Bus();

    uint16_t node_id();
    uint32_t quadlet_read(uint32_t node, uint64_t address);
    void quadlet_write(uint32_t node, uint64_t address, uint32_t data);

    void post_write(uint32_t node, uint64_t address, const void *buf, size_t buf_size);
    void post_read(uint32_t node, uint64_t address, void *buf, size_t buf_size);
  };
}

// EOF
