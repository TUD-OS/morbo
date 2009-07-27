// -*- Mode: C++ -*-

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include <linux/firewire-cdev.h>

#include <cstring>

#include <brimstone.h>

namespace Brimstone {

  // RCode -> str
  const char *rcode_str[] = { "COMPLETE",
			      "", "", "",
			      "CONFLICT_ERROR",
			      "DATA_ERROR",
			      "TYPE_ERROR",
			      "ADDRESS_ERROR",
			      "", "", "", "", "", "", "", "",
			      "SEND_ERROR (Juju)",
			      "CANCELLED (Juju)",
			      "BUSY (Juju)",
			      "GENERATION (Juju)",
			      "NO_ACK (Juju)" };

  // SyscallError

  const char *
  SyscallError::what() const throw()
  {
    return strerror(_errno);
  }

  SyscallError::SyscallError() throw()
    : _errno(errno)
  {}

  // Port

  void
  Port::update_bus_info(struct fw_cdev_event_bus_reset *e)
  {
    node_id = e->node_id;
    local_node_id = e->local_node_id;
    bm_node_id = e->bm_node_id;
    irm_node_id = e->irm_node_id;
    root_node_id = e->root_node_id;
    generation = e->generation;

    log << "Generation " << generation;
    log << " | root " << root_node_id;
    log << " | we " << local_node_id << endl; 
  }

  void
  Port::get_info()
  {
    struct fw_cdev_get_info req;
    struct fw_cdev_event_bus_reset bus_reset_evt;
    memset(&req, 0, sizeof(req));

    req.version = FW_CDEV_VERSION;
    req.bus_reset = (uint64_t)&bus_reset_evt;
    
    int err = ioctl(fd, FW_CDEV_IOC_GET_INFO, &req);
    if (err < 0)
      throw new SyscallError;

    update_bus_info(&bus_reset_evt);
  }

  void
  Port::wait()
  {
  again:
    struct pollfd pollfd = { fd, POLLIN, 0 };

    // Poll with infinite timeout.
    int ret = poll(&pollfd, 1, -1);

    if (ret < 0)
      throw new SyscallError;

    if (ret == 0) {
      log << "Nothing happened, yet poll returned?" << endl;
      goto again;
    }
  }


  class Request {
  public:
    Port &port;
    const static unsigned max_retries = 10;
    unsigned retries_left;

    virtual void retry() = 0;

    Request(Port &port) : port(port), retries_left(max_retries) {}
  };

  void
  Port::read_event()
  {
    int len = read(fd, buffer, sizeof(buffer));
    if (len < 0)
      throw new SyscallError;

    log << "Got " << len << " bytes from kernel." << endl;
    fw_cdev_event *evt = (fw_cdev_event *)buffer;
    Request *req;

    switch (evt->common.type) {
    case FW_CDEV_EVENT_BUS_RESET:
      log << "Bus reset." << endl;
      update_bus_info(&evt->bus_reset);
      break;

    case FW_CDEV_EVENT_RESPONSE:
      log << "Response rcode " << evt->response.rcode;
      if (evt->response.rcode < (sizeof(rcode_str)/sizeof(char *))) {
	log << " (" << rcode_str[evt->response.rcode] << ")";
      }	
      log << " length " << evt->response.length << endl;

      req = (Request *)evt->response.closure;

      if (evt->response.rcode == RCODE_COMPLETE) {
	log << "Deleting " << req << "." << endl;
	delete req;
      }

      break;

    default:
      log << "Unknown event " << evt->common.type << ". Ignoring." << endl;
      break;
    }
  }

  class ReadRequest : public Request {
  private:
    uint32_t node;
    uint64_t addr;
    void *buf;
    size_t buf_size;

  public:
    virtual void retry()
    {
      if (retries_left-- == 0)
	throw new TooManyRetriesError;

      struct fw_cdev_send_request req;
      memset(&req, 0, sizeof(req));
      
      req.tcode = TCODE_READ_BLOCK_REQUEST;
      req.length = buf_size;
      req.offset = addr;
      req.closure = (uint64_t)this;
      req.data = 0;		// Data is sent with event.
      req.generation = port.generation;

      int ret = ioctl(port.get_fd(), FW_CDEV_IOC_SEND_REQUEST, &req);
      if (ret < 0)
	throw new SyscallError;
    }

    ReadRequest(Port &_port, uint32_t node, uint64_t addr, void *buf, size_t buf_size)
      : Request(_port), node(node), addr(addr), buf(buf), buf_size(buf_size)
    { }
  };


  void
  Port::post_block_read(uint32_t node, uint64_t addr, void *buf, size_t buf_size)
  {
    log << "Post read for node " << node << " addr ";
    log << addr << " size " << buf_size << endl;

    Request *closure = new ReadRequest (*this, node, addr, buf, buf_size);
    closure->retry();
  }

  Port::Port(const char *dev, ostream &log) : log(log) {
    log << showbase << hex;
    log << "Port opened: " << dev << endl;

    fd = open(dev, O_RDWR);
    if (fd == -1) {
      throw new SyscallError;
    }

    get_info();
  }

  

  Port::~Port() {
    close(fd);
    log << "Port closed." << endl;
  }

}

// EOF
