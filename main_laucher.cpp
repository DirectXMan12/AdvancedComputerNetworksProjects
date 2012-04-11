#include "physical_layer.h"
#include "data_link_layer.h"
#include "err_macros.h"
#include "sys/wait.h"
#include "sys/prctl.h"
#include <iostream>

pid_t dll_pid;
bool already_sent = false;
bool is_server = false;
bool flow_on = false;
bool good_to_send = false;

char* split_buffer = new char[sizeof(struct packet)];
bool concat_next_packet = false;

using namespace std;

void sendPacketP2(const char* payload, int payload_len, int command)
{
  while(!flow_on) {}
  flow_on = false; // reset flow_on

  SHM_GRAB_NEW(struct packet, p, packetid);
  memcpy(p->payload, payload, payload_len);
  p->pl_data_len = payload_len;
  p->command_type = command;
  p->is_split = 0x1;

  sigval v;
  v.sival_int = packetid;
  sigqueue(dll_pid, SIG_NEW_PACKET, v);
  SHM_RELEASE(struct packet, p);
}

void sendPacket(bool isErr, const char* payload, int payload_len, int command)
{
  while(!flow_on) {}
  flow_on = false; // reset flow_on

  if (sizeof(struct packet) - 256 + payload_len > 150)
  {

    unsigned int overflow = sizeof(struct packet) - 256 + payload_len - 150;
    char* plp2 = new char[overflow];
    memset(plp2, 0, overflow);

    SHM_GRAB_NEW(struct packet, p, packetid);
    memcpy(p->payload, payload, payload_len - overflow);
    p->pl_data_len = payload_len - overflow;
    p->command_type = command;
    p->is_split = 0x1;

    sigval v;
    v.sival_int = packetid;
    sigqueue(dll_pid, SIG_NEW_PACKET, v);
    SHM_RELEASE(struct packet, p);

    memcpy(plp2, payload+(payload_len-overflow), overflow);
    sendPacketP2(plp2, overflow, command);
  }
  else
  {
    SHM_GRAB_NEW(struct packet, p, packetid);
    memcpy(p->payload, payload, payload_len);
    p->pl_data_len = payload_len;
    p->command_type = command;
    p->is_split = 0;

    sigval v;
    v.sival_int = packetid;
    //POST_INFO("APPLICATION_LAYER: Sending packet with command " << command);
    sigqueue(dll_pid, SIG_NEW_PACKET, v);
    SHM_RELEASE(struct packet, p);
  }
}

void handle_app_signals(int, siginfo_t*, void*);

void fork_to_new_client(int comm_sock)
{
  if (fork() == 0)
  {
    pid_t my_pid = getpid();
    
    // TODO: see if we actually have an argument
    is_server = true;

    dll_pid = fork();
    if(dll_pid < 0)
    {
      //failed to fork
      POST_ERR("APPLICATION_LAYER: Failed to fork off data link layer...");
    }
    else if (dll_pid == 0) // we are the child
    {
      if(is_server) prctl(PR_SET_NAME, (unsigned long) "prog1s_dll", 0, 0, 0);
      else prctl(PR_SET_NAME, (unsigned long) "prog1c_dll", 0, 0, 0);
      init_data_link_layer(true, my_pid, true, comm_sock);
    }
    else
    {
      srandom(getpid());
      struct sigaction act;
      act.sa_sigaction = &handle_app_signals;
      sigemptyset(&act.sa_mask);
      act.sa_flags = SA_SIGINFO | SA_RESTART;

      sigaction(SIG_NEW_PACKET, &act, 0);
      sigaction(SIG_FLOW_ON, &act, 0);
      sigaction(SIG_NEW_CLIENT, &act, 0);
    }

    while(!good_to_send) {}
    sendPacket(0, "hello1", 6, 0);
    sendPacket(0, "hello2", 6, 0);
    while(1) waitpid(-1, 0, 0);
  }
}

void handle_app_signals(int signum, siginfo_t* info, void* context)
{
  //POST_INFO("APP_LAYER: got signal " << signum);
  if (signum == SIG_FLOW_ON)
  {
      flow_on = (info->si_value.sival_int == 1);
  }
  else if (signum == SIG_NEW_CLIENT)
  {
    POST_INFO("APPLICATION_LAYER: New client on socket " << info->si_value.sival_int << ", forking new stack...");
    fork_to_new_client(info->si_value.sival_int);
  }
  else if (signum == SIG_NEW_PACKET)
  {
    SHM_GRAB(struct packet, pack, (info->si_value.sival_int));
    
    if(pack->is_split && !concat_next_packet)
    {
      concat_next_packet = true;   
      memset(split_buffer, 0, sizeof(struct packet));
      memcpy(split_buffer, pack, 150);

      cout << "Split packet part 1: " << pack->payload << endl;
    }
    else if (pack->is_split && concat_next_packet)
    {
      memcpy(split_buffer+148, pack->payload, (int)pack->pl_data_len);
      concat_next_packet = false;

      cout << "Split packet part 2 (" << (int)pack->pl_data_len << "): " << pack->payload << endl;

      cout << ((packet*)split_buffer)->payload << endl;
    }
    else
    {
      cout << pack->payload << endl;
    }
    good_to_send = true;
    if (!is_server)
    {
      sendPacket(false, "resp", 4, 0);
    }

    SHM_RELEASE(struct packet, pack); 
    SHM_DESTROY((info->si_value.sival_int));
  }
}

int main(int argc, char* argv[])
{
  pid_t my_pid = getpid();
  
  // TODO: see if we actually have an argument
  if (argv[1][0] == '1')
  {
    POST_INFO("APPLICATION_LAYER: We are a server");
    is_server = true;
  }
  else POST_INFO("APPLICATION_LAYER: We are a client");

  dll_pid = fork();
  if(dll_pid < 0)
  {
    //failed to fork
    POST_ERR("APPLICATION_LAYER: Failed to fork off data link layer...");
  }
  else if (dll_pid == 0) // we are the child
  {
    if(is_server) prctl(PR_SET_NAME, (unsigned long) "prog1s_dll", 0, 0, 0);
    else prctl(PR_SET_NAME, (unsigned long) "prog1c_dll", 0, 0, 0);
    init_data_link_layer(argv[1][0] == '1', my_pid, false, 0);
  }
  else
  {
    srandom(getpid());
    struct sigaction act;
    act.sa_sigaction = &handle_app_signals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO | SA_RESTART;

    sigaction(SIG_NEW_PACKET, &act, 0);
    sigaction(SIG_FLOW_ON, &act, 0);
    sigaction(SIG_NEW_CLIENT, &act, 0);
  }

  if (is_server)
  {
    while(1) waitpid(-1, 0, 0);
  }
  else // we are a client, send test packets
  {
    sendPacket(0, "hi!", 3, 0);
    int i;
    while(1)
    {
      cin >> i;
      if (i == -1)
      {
        sendPacket(0, "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer eu est dolor. Sed rutrum felis a libero tempor pharetra. Vivamus in orci non dui accumsan dignissim imperdiet et lorem. Fusce suscipit, tortor non tempor lobortis, tellus elit egestas nullam.", 256, 1);
      }
      else
      {
        for(int j = 0; j < i; j++) sendPacket(0, "hi!", 3, 0);
      }
    }
  }
}
