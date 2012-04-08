#include "physical_layer.h"
#include "data_link_layer.h"
#include "err_macros.h"
#include "sys/wait.h"
#include "sys/prctl.h"

pid_t dll_pid;
bool already_sent = false;
bool is_server = false;

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
      POST_ERR("Failed to Fork");
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
      act.sa_flags = SA_SIGINFO;

      sigaction(SIG_NEW_PACKET, &act, 0);
      sigaction(SIG_FLOW_ON, &act, 0);
      sigaction(SIG_NEW_CLIENT, &act, 0);
    }

    while(1) waitpid(-1, 0, 0);
  }
}

void send_a_test_packet()
{
  packet p;
  p.payload[0] = 'h';
  p.payload[1] = 'i';
  p.payload[2] = '!';
  p.command_type = 0;
  p.pl_data_len = 3;
  p.seq_num = 0;
  sigval v;
  v.sival_ptr = &p;
  POST_INFO("APP_LAYER: sending new test packet");
  sigqueue(dll_pid, SIG_NEW_PACKET, v);
}

void handle_app_signals(int signum, siginfo_t* info, void* context)
{
  POST_INFO("APP_LAYER: got signal " << signum);
  if (signum == SIG_FLOW_ON && !already_sent && !is_server)
  {
    already_sent = true;
    send_a_test_packet(); // NOTE: shouldn't be calling an external function from here unless we make it async-safe
  }
  else if (signum == SIG_NEW_CLIENT)
  {
    POST_INFO("New client on socket " << info->si_value.sival_int << ", forking");
    fork_to_new_client(info->si_value.sival_int);
  }
}

int main(int argc, char* argv[])
{
  pid_t my_pid = getpid();
  
  // TODO: see if we actually have an argument
  if (argv[1][0] == '1')
  {
    POST_INFO("we are a server");
    is_server = true;
  }
  else POST_INFO("we are a client");

  dll_pid = fork();
  if(dll_pid < 0)
  {
    //failed to fork
	  POST_ERR("Failed to Fork");
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
    act.sa_flags = SA_SIGINFO;

    sigaction(SIG_NEW_PACKET, &act, 0);
    sigaction(SIG_FLOW_ON, &act, 0);
    sigaction(SIG_NEW_CLIENT, &act, 0);
  }


  while(1) waitpid(-1, 0, 0);
}
