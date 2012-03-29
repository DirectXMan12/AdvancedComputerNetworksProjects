#include "physical_layer.h"
#include "err_macros.h"

PhysicalLayer::PhysicalLayer(pid_t dp, bool is)
{
  this->dll_pid = dp;
  this->is_server = is;
}

int PhysicalLayer::init_connection(const char* client_name, const char* server_name)
{
  // open Unix domain socket
  this->ipc_sock = socket(PF_UNIX, SOCK_DGRAM, 0); 
  struct sockaddr_un ipc_addr;

  ipc_addr.sun_family = AF_UNIX;
  ipc_addr.sun_path = PHYS_SOCK;
  
  this->ipc_addr = &ipc_addr;

  // bind the specified file to this socket
  bind(this->ipc_sock, this->ipc_addr, sizeof(struct sockaddr_un));
  // TODO: initialize signal-based IO (or select-based IO if really needed)

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
      POST_ERR("issue resolving server name \"" << argv[1] << "\" to address: " << gai_strerror(lookup_res));
      return -1;
    }
   
    struct addrinfo* rp;
    bool success = false;

    // loop through the linked list, creating a socket and attempting to use it to connect to the address
    for (rp = server_addr; rp != NULL; rp = rp->ai_next)
    {
      this->tcp_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

      if (client_socket < 0)
      {
        POST_ERR("unable to allocate a socket");
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
        POST_WARN("Couldn't bind to one of the returned address: " << strerror(errno));
        close(client_socket);
      } 
    } 

    if (!success)
    {
        POST_ERR("could not bind socket to any of the addresses to which the specified server name resolved");
        return -2;
    }

    freeaddrinfo(server_addr); // free the linked list of result addresses
    
    // send the client identification string
    String msg = "PHYS_LAYER_CLIENT::";
    msg+= client_name;
    msg+= "::";
    write(this->tcp_sock, msg.c_str(), sizeof(msg.c_str()));
  }
  else // otherwise we must be a server
  {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8675);

    // allocate a file descriptor
    server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    // bind to any address (as specified above)
    if (bind(this->tcp_sock, (const sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
      POST_ERR("issue binding socket)");
      return -1;
    }

    // and listen (but not accept yet)
    if (listen(server_socket, 5) < 0)
    {
      POST_ERR("issue listening on socket");
      return -2;
    }

    this->acceptAsServer();
  }

  // TODO: set signal handler for TCP socket
  return 0;
}

void PhysicalLayer::acceptAsServer()
{
  while(1)
  { 
    POST_INFO("PHYSICAL LAYER: listening...");
    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);
    int client_sock = accept(this->tcp_sock, (sockaddr*) &client_addr, &client_size);
    // we got a new client connecting (i.e. there is another physical layer connection request)

    if (fork() == 0) // we are the child now
    {
      /*fd_set rfds;

      FD_ZERO(&rfds);
      FD_SET(client_sock, &rfds);*/
      while(1)
      {
        /*int select_res = select(client_sock+1, &rfds, NULL, NULL, NULL);
        if (select_res == -1)
        {
          POST_ERR("something went wrong with select!");
          // TODO: do something
        }
        else if (FD_ISSET(client_addr, &rfds))
        {*/
          char buff[153];    // assume for now that frame is fixed-width, not variable
          read(client_sock, buff, 153); // TODO: implement variable-width frames by encoding payload and writing an end-of-frame byte
          sendto(this->ipc_sock, buff, 153, 0, this->ipc_addr, sizeof(struct sockaddr_un));

          sigval v;
          v.sv_int = 0;
          sigqueue(this->dll_pid, SIG_NEW_FRAME, v); // signal the dll that we have a new frame incoming

                  //}
      }
    }
    
  }
}

#endif
