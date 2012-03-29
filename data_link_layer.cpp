#include "struct_defs.h"
#include "data_link_layer.h"
#include "signal_defs.h"
#include "physical_layer.h"

pid_t phys_layer_pid;

// make sure to only use signal safe functions in here!  Check signal(7) for more info
// newer POSIX realtime signals with attached values for the win!
// see sigqueue(2) and sigaction(2) for more info about the attached values
void handle_signals(int signum, siginfo_t* info, void* context)
{
  if(signum == SIG_NEW_FRAME)
  {
    // we got a new frame from the physical layer
    struct frame* recv_frame;
    // collect it from the signal value
    recv_frame = (frame*)info->si_value.sival_ptr;
    // do frame receive logic here
  }
  else if(signum == SIG_NEW_PACKET)
  { 
    // we got a new packet from the app layer!
    struct packet* recv_packet;
    recv_packet = (packet*)info->si_value.sival_ptr;
  } 
  else if(signum == SIG_TIMER_ELAPSED)
  {
    // do timer logic here
  }
}

void init_data_link_layer(bool is_server)
{
  struct sigaction act;
  act.sa_sigaction = &handle_signals;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;

  sigaction(SIG_NEW_FRAME, &act, 0);
  sigaction(SIG_NEW_PACKET, &act, 0);
  sigaction(SIG_TIMER_ELAPSED, &act, 0);

  pid_t my_pid = getpid();
  phys_layer_pid = fork();

  if (phys_layer_pid == 0) // this will become the physical layer
  {
    PhysicalLayer p(my_pid, is_server);
    if (is_server)
    {
      p.init_connection(0, 0);
    }
    else
    {
      p.init_connection("client a", "localhost");
    }
  }
}
