// -*- Mode: C++ -*-

#pragma once

#include <gc/gc_cpp.h>

#include <cstdint>
#include <iostream>
#include <exception>

// Forward declarations
struct fw_cdev_event_bus_reset;

namespace Brimstone {

  using namespace std;

  // Exception handling

  class BrimstoneException : public exception {
  };

  class SyscallError : public BrimstoneException {
  protected:
    int _errno;
  public:
    virtual const char *what() const throw();
    SyscallError() throw();
  };

  class TooManyRetriesError : public BrimstoneException {
  public:
    virtual const char *what() const throw() {
      return "Too many retries.";
    }
  };

  // I/O API

  class Port : 
    // "cleanup" is more like a wish. ;) The cleanup handler is not
    // called if the object survives until program exit.
    public gc_cleanup 
  {
  protected:
    int fd;
    ostream &log;

    static const unsigned buffer_size = 4096;
    char buffer[buffer_size] __attribute__((aligned(16)));

    void update_bus_info(::fw_cdev_event_bus_reset *bus_reset_event);
    void get_info();

  public:
    uint32_t node_id;
    uint32_t local_node_id;
    uint32_t bm_node_id;
    uint32_t irm_node_id;
    uint32_t root_node_id;
    uint32_t generation;

    void wait();
    void read_event();

    void post_block_read(uint32_t node, uint64_t addr, void *buf, size_t buf_size);
    // void post_block_write(uint32_t node, uint64_t addr, void *buf, size_t buf_size);

    // void barrier();

    uint32_t quadlet_read(uint32_t node, uint64_t addr);
    void quadlet_write(uint32_t node, uint64_t addr, uint32_t data);

    int get_fd() const { return fd; }

    Port(const char *dev, ostream &log);
    virtual ~Port();
  };

}

// EOF
