// -*- Mode: C++ -*-

#include <iostream>

#include <brimstone2.h>

using namespace std;
using namespace Brimstone;

int
main()
{
  Bus b("/dev/morbo0");
  
  cout << "Node ID: " << hex << b.node_id() << endl;

  uint32_t data;
  cout << "Read 0x0 on 0xffc0: " << hex << (data = b.quadlet_read(0xffc0, 0)) << endl;
  cout << "Inc 0x0 on 0xffc0" << endl;
  b.quadlet_write(0xffc0, 0, data+1);
  cout << "Read 0x0 on 0xffc0: " << hex << b.quadlet_read(0xffc0, 0) << endl;

  const char msg[] = "Hello World!";
  char msg_incoming[sizeof(msg)];

  cout << "Trying a block write: " << msg << endl;
  b.post_write(0xffc0, 0, msg, sizeof(msg));
  b.post_read(0Xffc0, 0, msg_incoming, sizeof(msg));
  cout << "Got back: " << msg_incoming << endl;

  char *buf = new char[256*1024];
  b.post_read(0xffc0, 0, buf, 256*1024);
  

  return 0;
}

// EOF
