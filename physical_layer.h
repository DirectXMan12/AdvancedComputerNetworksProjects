#include "signal_defs.h"

#ifndef __PHYS_LAYER__
#define __PHYS_LAYER__

public class PhysicalLayer
{
  public:
    // dp: pid of the data link layer PID
    // is: is this a server physical layer?
    PhysicalLayer(pid_t dp, bool is);
    int init_connection( const char* client_name, const char* server_name);

  private:
    void acceptAsServer();
    int ipc_sock;
    struct sockaddr_un* ipc_addr;
    int tcp_sock;
    pid_t dll_pid;

};

#endif
