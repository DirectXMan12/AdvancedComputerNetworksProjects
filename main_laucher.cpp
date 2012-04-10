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

using namespace std;

void sendPacket(bool isErr, const char* payload, int payload_len, int command)
{
  while(!flow_on) {}
  flow_on = false; // reset flow_on

  SHM_GRAB_NEW(struct packet, p, packetid);
//  if (!isErr)
//  {
    memcpy(p->payload, payload, payload_len);
    p->pl_data_len = payload_len;
//  }
/*  else
  {
    p->payload[0] = 'E'; 
    p->payload[1] = 'R';
    p->payload[2] = 'R';

    p->pl_data_len = 3;
  }*/
  p->command_type = command;
  p->seq_num = 0;

  sigval v;
  v.sival_int = packetid;
  //POST_INFO("APPLICATION_LAYER: Sending packet with command " << command);
  sigqueue(dll_pid, SIG_NEW_PACKET, v);
  SHM_RELEASE(struct packet, p);
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
    
    POST_INFO("APPLICATION_LAYER: new packet: " << pack->payload);
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
      for(int j = 0; j < i; j++) sendPacket(0, "hi!", 3, 0);
    }
  }
}
