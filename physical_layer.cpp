#include "physical_layer.h"
#include "err_macros.h"
#include "stdlib.h"
#include <string.h>
#include <errno.h>
#include "sendfd.h"

PhysicalLayer* phys_obj;

PhysicalLayer::PhysicalLayer(pid_t dp, bool is)
{
  this->dll_pid = dp;
  this->is_server = is;
}

void handle_phys_layer_signals(int signum, siginfo_t* info, void* context)
{
  //POST_INFO("PHYSICAL_LAYER: Got signal " << signum);
  if (signum == SIGSEGV)
  {
    POST_ERR("PHYSICAL_LAYER: Segfault!");
    exit(2);
  }

  SHM_GRAB(struct frame, fts, (info->si_value.sival_int));

  // TODO: do random error stuff in Data Link Layer

  // send the frame on its merry way
  write(phys_obj->tcp_sock, (void*)fts, sizeof(struct frame));
  //POST_INFO("PHYSICAL_LAYER: sent data on socket " << phys_obj->tcp_sock);
  SHM_RELEASE(struct frame, fts);
  SHM_DESTROY((info->si_value.sival_int));
}

int PhysicalLayer::init_connection(const char* client_name, const char* server_name, bool is_comm_process)
{
  // initialize TCP socket
  if (!this->is_server) // if we are a client
  {
    // all this stuff is to translate the server name into an appropriate address
    struct addrinfo server_addr_hints;
    struct addrinfo* server_addr;
    unsigned short server_port = 8675;  

    server_addr_hints.ai_family = AF_INET;
    server_addr_hints.ai_socktype = SOCK_STREAM;
    server_addr_hints.ai_protocol = IPPROTO_TCP;
    server_addr_hints.ai_addrlen = 0;
    server_addr_hints.ai_addr = 0;
    server_addr_hints.ai_canonname = 0;
    server_addr_hints.ai_flags = 0;

    // here we actually search for the name and get a linked list of results back
    int lookup_res = getaddrinfo(server_name, "8675", &server_addr_hints, &server_addr);
    if (lookup_res < 0)
    {
      POST_ERR("PHYSICAL_LAYER: Issue resolving server name \"" << server_name << "\" to address: " << gai_strerror(lookup_res));
      return -1;
    }
   
    struct addrinfo* rp;
    bool success = false;

    // loop through the linked list, creating a socket and attempting to use it to connect to the address
    for (rp = server_addr; rp != NULL; rp = rp->ai_next)
    {
      this->tcp_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

      if (this->tcp_sock < 0)
      {
        POST_ERR("PHYSICAL_LAYER: Unable to allocate a socket");
        return -2;
      }

      int res = connect(this->tcp_sock, rp->ai_addr, rp->ai_addrlen);
       
      if (res == 0)
      {
        success = true;
        break;
      }
      else
      {
        // if we fail at connecting to this result address, close the socket and try again on a new socket with the next result address
        POST_WARN("PHYSICAL_LAYER: Couldn't bind to one of the returned address: " << strerror(errno));
        close(this->tcp_sock);
      } 
    } 

    if (!success)
    {
        POST_ERR("PHYSICAL_LAYER: Could not bind socket to any of the addresses to which the specified server name resolved");
        return -2;
    }

    freeaddrinfo(server_addr); // free the linked list of result addresses
    
    // send the client identification string
    std::string msg = "PHYS_LAYER_CLIENT::";
    msg+= client_name;
    msg+= "::";
    write(this->tcp_sock, msg.c_str(), msg.length());

    phys_obj = this;
    
    struct sigaction act;
    act.sa_sigaction = &handle_phys_layer_signals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIG_NEW_FRAME, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);

    sigval v;
    v.sival_int = 1; // tell the dll that we are ready
    sigqueue(this->dll_pid, SIG_FLOW_ON, v);
    this->readAsClient();
  }
  else // otherwise we must be a server
  {
    if (!is_comm_process)
    {
      struct sockaddr_in server_addr;
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      server_addr.sin_port = htons(8675);

      // allocate a file descriptor
      this->tcp_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

      // bind to any address (as specified above)
      if (bind(this->tcp_sock, (const sockaddr*) &server_addr, sizeof(server_addr)) < 0)
      {
        POST_ERR("PHYSICAL_LAYER: Issue binding socket: " << strerror(errno));
        return -1;
      }

      // and listen (but not accept yet)
      if (listen(this->tcp_sock, 5) < 0)
      {
        POST_ERR("PHYSICAL_LAYER: Issue listening on socket: " << strerror(errno));
        return -2;
      }
    }
    else
    {
      this->tcp_sock = *((int*)server_name); // just pass the socket as the server name
    }

    phys_obj = this;
    
    struct sigaction act;
    act.sa_sigaction = &handle_phys_layer_signals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIG_NEW_FRAME, &act, NULL);
    
    sigval v;
    v.sival_int = 1; // tell the dll that we are ready
    sigqueue(this->dll_pid, SIG_FLOW_ON, v);

    this->acceptAsServer(is_comm_process);
  }

  return 0;
}

void PhysicalLayer::readAsClient()
{
  POST_INFO("PHYSICAL_LAYER: Listening as client...");
  char buff[sizeof(struct frame)];
  while(1)
  {
    int r = read(this->tcp_sock, buff, sizeof(struct frame));
    if (r < 0)
    {
      POST_ERR("PHYSICAL_LAYER: Issue reading from socket: " << strerror(errno));
      break;
    }
    else if (r == 0)
    {
      POST_WARN("PHYSICAL_LAYER: socket closed...");
      // TODO: send a signal to this effect
      break;
    }
    //POST_INFO("PHYSICAL_LAYER: read " << r << " bytes of data...");
    SHM_GRAB_NEW(struct frame, fts, frid);
    memcpy(fts, buff, sizeof(struct frame));
    sigval v;
    v.sival_int = frid;
    sigqueue(this->dll_pid, SIG_NEW_FRAME, v);
    SHM_RELEASE(struct frame, fts);
  }
}


void PhysicalLayer::acceptAsServer(bool is_comm_process)
{
  if (is_comm_process)
  {
    this->tcp_sock = transferfdfrom("./sock_fdtrans");
    POST_INFO("PHYSICAL_LAYER: awaiting packets from socket " << this->tcp_sock);
    char buff[sizeof(struct frame)];
    while(1)
    {
      int r = read(this->tcp_sock, buff, sizeof(struct frame));
      if (r < 0)
      {
        POST_ERR("PHYSICAL_LAYER: Issue reading from socket: " << strerror(errno));
        break;
      }
      else if (r == 0)
      {
        POST_WARN("PHYSICAL_LAYER: socket closed...");
        // TODO: send a signal to this effect
        break;
      }
      //POST_INFO("PHYSICAL_LAYER: read " << r << " bytes of data...");
      SHM_GRAB_NEW(struct frame, fts, frid);
      memcpy(fts, buff, sizeof(struct frame));
      sigval v;
      v.sival_int = frid;
      sigqueue(this->dll_pid, SIG_NEW_FRAME, v);
    }
  }
  else
  {
    while(1)
    { 
      POST_INFO("PHYSICAL LAYER: listening for clients on socket " << this->tcp_sock);
      struct sockaddr_in client_addr;
      socklen_t client_size = sizeof(client_addr);
      int client_sock = accept(this->tcp_sock, (sockaddr*) &client_addr, &client_size);
      // we got a new client connecting (i.e. there is another physical layer connection request)

      sigval v;
      v.sival_int = client_sock;
      sigqueue(this->dll_pid, SIG_NEW_CLIENT, v);

      char client_info[29];
      read(client_sock, client_info, 29);
      POST_INFO("PHYSICAL_LAYER: Client '" << client_info << "' connected!");
      transferfdto(client_sock, "./sock_fdtrans"); 
    }
  }
}

