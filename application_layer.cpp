#include <sys/wait.h>
#include <sys/prctl.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "err_macros.h"
#include "physical_layer.h"
#include "data_link_layer.h"
using namespace std;

pid_t dll_pid;
bool already_sent = false;
bool is_server = false;
sqlite3 *database;
int rc;
char *ErrMsg =0;
bool successfulLogin = false;
char* myUsername;
int personID;

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
      struct sigaction act;
      act.sa_sigaction = &handle_app_signals;
      sigemptyset(&act.sa_mask);
      act.sa_flags = SA_SIGINFO | SA_RESTART;

      sigaction(SIG_NEW_PACKET, &act, 0);
      sigaction(SIG_FLOW_ON, &act, 0);
      sigaction(SIG_NEW_CLIENT, &act, 0);
    }

    while(1) waitpid(-1, 0, 0);
  }
}

void send_a_test_packet()
{
  SHM_GRAB_NEW(struct packet, p, packetid);
  p->payload[0] = 'h';
  p->payload[1] = 'i';
  p->payload[2] = '!';
  p->command_type = 0;
  p->pl_data_len = 3;
  p->seq_num = 0;

  sigval v;
  v.sival_int = packetid;
  POST_INFO("APP_LAYER: sending new test packet");
  sigqueue(dll_pid, SIG_NEW_PACKET, v);
  SHM_RELEASE(struct packet, p);
}

void handle_app_signals(int signum, siginfo_t* info, void* context)
{
  //POST_INFO("APP_LAYER: got signal " << signum);
  if (signum == SIG_FLOW_ON && !already_sent && !is_server)
  {
    already_sent = true;
    send_a_test_packet(); // NOTE: shouldn't be calling an external function from here unless we make it async-safe
  }
  else if (signum == SIG_NEW_CLIENT)
  {
    POST_INFO("APPLICATION_LAYER: New client on socket " << info->si_value.sival_int << ", forking new stack...");
    fork_to_new_client(info->si_value.sival_int);
  }
  else if (signum == SIG_NEW_PACKET)
  {
    SHM_GRAB(struct packet, pack, (info->si_value.sival_int));
    // do some stuff, memcpy
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
    struct sigaction act;
    act.sa_sigaction = &handle_app_signals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO | SA_RESTART;

    sigaction(SIG_NEW_PACKET, &act, 0);
    sigaction(SIG_FLOW_ON, &act, 0);
    sigaction(SIG_NEW_CLIENT, &act, 0);

    if (is_server)
    {
      rc = sqlite3_open("test.db", &database);
      if(rc)
      {
        POST_ERR("APPLICATION_LAYER: Database could not be accessed");
        sqlite3_close(database);
      }
      else
      {
        POST_ERR("APPLICATION_LAYER: Database was accessed");
        insertPeople("matt", "ferreira", "WPI");
        selectPeople();
      }
    }
  }

  while(1) waitpid(-1, 0, 0);
}

void sendPacket(bool isErr, char* payload, int payload_len, unsigned int command)
{
  SHM_GRAB_NEW(struct packet, p, packetid);
  memcopy(p->payload, payload, payload_len);
  p->command_type = command;
  p->pl_data_len = payload_len;
  p->seq_num = 0;

  sigval v;
  v.sival_int = packetid;
  POST_INFO("APP_LAYER: sending new test packet");
  sigqueue(dll_pid, SIG_NEW_PACKET, v);
  SHM_RELEASE(struct packet, p);
}
