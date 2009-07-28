// -*- Mode: C++ -*-

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include <linux/firewire-cdev.h>

#include <cassert>
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

  // Node

  Node::Node(const char *dev, ostream &log) : _log(log), _in_flight(0) {
    _log << showbase << hex;
    _log << "Node opened: " << dev << endl;

    _fd = open(dev, O_RDWR);
    if (_fd == -1) {
      throw new SyscallError;
    }

    get_info();
  }

  Node::~Node() {
    close(_fd);
    _log << "Node closed." << endl;
  }

  void
  Node::update_bus_info(struct fw_cdev_event_bus_reset *e)
  {
    _node_id = e->node_id;
    _local_node_id = e->local_node_id;
    _bm_node_id = e->bm_node_id;
    _irm_node_id = e->irm_node_id;
    _root_node_id = e->root_node_id;
    _generation = e->generation;

    _log << "Generation " << _generation;
    _log << " | root " << _root_node_id;
    _log << " | we " << _local_node_id << endl; 
  }

  void
  Node::get_info()
  {
    struct fw_cdev_get_info req;
    struct fw_cdev_event_bus_reset bus_reset_evt;
    memset(&req, 0, sizeof(req));

    req.version = FW_CDEV_VERSION;
    req.bus_reset = (uint64_t)&bus_reset_evt;
    
    int err = ioctl(_fd, FW_CDEV_IOC_GET_INFO, &req);
    if (err < 0)
      throw new SyscallError;

    update_bus_info(&bus_reset_evt);
  }

  bool
  Node::wait(bool block)
  {
  again:
    struct pollfd pollfd = { _fd, POLLIN, 0 };

    // Poll with infinite timeout.
    int ret = ::poll(&pollfd, 1, block ? -1 : 0);

    if (ret < 0)
      throw new SyscallError;

    if (block && (ret == 0)) {
      _log << "Nothing happened, yet poll returned?" << endl;
      goto again;
    }

    return ret == 1;
  }

  void
  Node::poll()
  {
    while (wait(false))
      read_event();
  }

  void
  Node::barrier()
  {
    while (_in_flight > 0) {
      wait(true);
      read_event();
    }
  }


  class Request {
  protected:
    Node &_node;
    const static unsigned max_retries = 10;
    unsigned retries_left;

    uint64_t addr;
    void *buf;
    size_t buf_size;

    virtual void post() = 0;

  public:

    void retry()
    {
      if (retries_left-- == 0)
	throw new TooManyRetriesError;
      post();
    }

    virtual void done(fw_cdev_event *evt) = 0;

    Request(Node &node, uint64_t addr, void *buf, size_t buf_size) 
      : _node(node), retries_left(max_retries), addr(addr), buf(buf), buf_size(buf_size)
    {}
  };

  void
  Node::read_event()
  {
    int len = read(_fd, _buffer, sizeof(_buffer));
    if (len < 0)
      throw new SyscallError;

    _log << "Got " << len << " bytes from kernel." << endl;
    fw_cdev_event *evt = (fw_cdev_event *)_buffer;
    Request *req;

    switch (evt->common.type) {
    case FW_CDEV_EVENT_BUS_RESET:
      _log << "Bus reset." << endl;
      update_bus_info(&evt->bus_reset);
      break;

    case FW_CDEV_EVENT_RESPONSE:
      _log << "Response rcode " << evt->response.rcode;
      if (evt->response.rcode < (sizeof(rcode_str)/sizeof(char *))) {
	_log << " (" << rcode_str[evt->response.rcode] << ")";
      }	
      _log << " length " << evt->response.length << endl;

      req = (Request *)evt->response.closure;

      if (evt->response.rcode == RCODE_COMPLETE) {
	_in_flight -= 1;
	req->done(evt);
	_log << "Deleting " << req << "." << endl;
	delete req;
      } else {
	_log << "Retrying..." << endl;
	req->retry();
      }

      break;

    default:
      _log << "Unknown event " << evt->common.type << ". Ignoring." << endl;
      break;
    }
    _log << _in_flight << " operations in flight." << endl;
  }

  // Read requests

  class ReadRequest : public Request {
  protected:
    virtual void post()
    {

      _node.log() << "Post read addr ";
      _node.log() << addr << " size " << buf_size << endl;

      struct fw_cdev_send_request req;
      memset(&req, 0, sizeof(req));
      
      req.tcode = (buf_size == 4) ? TCODE_READ_QUADLET_REQUEST : TCODE_READ_BLOCK_REQUEST;
      req.length = buf_size;
      req.offset = addr;
      req.closure = (uint64_t)this;
      req.data = 0;		// Data is returned with event.
      req.generation = _node.generation();

      int ret = ioctl(_node.fd(), FW_CDEV_IOC_SEND_REQUEST, &req);
      if (ret < 0)
	throw new SyscallError;
    }

  public:
    virtual void done(fw_cdev_event *evt)
    {
      assert(evt->response.length <= buf_size);
      memcpy(buf, evt->response.data, evt->response.length);
    }

    ReadRequest(Node &_node, uint64_t addr, void *buf, size_t buf_size)
      : Request(_node, addr, buf, buf_size)
    { }
  };


  void
  Node::post_read(uint64_t addr, void *buf, size_t buf_size)
  {
    _in_flight += 1;
    _log << _in_flight << " operations in flight." << endl;
    Request *closure = new ReadRequest (*this, addr, buf, buf_size);
    closure->retry();
  }

  // Write requests

  class WriteRequest : public Request {
  protected:
    virtual void post()
    {
      _node.log() << "Post write addr ";
      _node.log() << addr << " size " << buf_size << endl;

      struct fw_cdev_send_request req;
      memset(&req, 0, sizeof(req));
      
      req.tcode = (buf_size == 4) ? TCODE_WRITE_QUADLET_REQUEST : TCODE_WRITE_BLOCK_REQUEST;
      req.length = buf_size;
      req.offset = addr;
      req.closure = (uint64_t)this;
      req.data = (uint64_t)buf;
      req.generation = _node.generation();

      int ret = ioctl(_node.fd(), FW_CDEV_IOC_SEND_REQUEST, &req);
      if (ret < 0)
	throw new SyscallError;

    }

  public:
    virtual void done(fw_cdev_event *evt)
    {
      // Nothing to be done.
    }

    WriteRequest(Node &_node, uint64_t addr, void *buf, size_t buf_size)
      : Request(_node, addr, buf, buf_size)
    { }
  };

  void
  Node::post_write(uint64_t addr, void *buf, size_t buf_size)
  {
    _in_flight += 1;
    _log << _in_flight << " operations in flight." << endl;
    Request *closure = new WriteRequest (*this, addr, buf, buf_size);
    closure->retry();
  }


}

// EOF
