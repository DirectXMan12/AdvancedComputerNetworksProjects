#include "signal_defs.h"
#include "struct_defs.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <errno.h>
#include <string.h>
#include <signal.h>

#ifndef __PHYS_LAYER__
#define __PHYS_LAYER__

class PhysicalLayer
{
  public:
    // dp: pid of the data link layer PID
    // is: is this a server physical layer?
    PhysicalLayer(pid_t dp, bool is);
    int init_connection( const char* client_name, const char* server_name);
    int tcp_sock;

  private:
    void acceptAsServer();
    bool is_server;
    int ipc_sock;
    struct sockaddr_un* ipc_addr;
    pid_t dll_pid;

};

#endif
