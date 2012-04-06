#include "physical_layer.h"
#include "data_link_layer.h"
#include "err_macros.h"

pid_t dll_pid;

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
  sigqueue(dll_pid, SIG_NEW_PACKET, v);
}

void handle_app_signals(int signum, siginfo_t* info, void* context)
{
  POST_INFO("I like cereal!");
}

int main(int argc, char* argv[])
{
  pid_t my_pid = getpid();

  dll_pid = fork();
  // TODO: see if we actually have an argument
  if (argv[1][0] == '1') POST_INFO("we are a server");
  else POST_INFO("we are a client");

  if (dll_pid == 0) // we are the child
  {
    init_data_link_layer(argv[1][0] == '1', my_pid);
  }
  else
  {
    struct sigaction act;
    act.sa_sigaction = &handle_app_signals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;

    sigaction(SIG_NEW_FRAME, &act, 0);
  }

  send_a_test_packet();

  while(1) {}
}
