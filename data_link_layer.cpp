
// make sure to only use signal safe functions in here!  Check signal(7) for more info
// newer POSIX realtime signals with attached values for the win!
// see sigqueue(2) and sigaction(2) for more info about the attached values
void handle_signals(int signum, siginfo_t* info, void* context)
{
  switch(signum)
  {
    case SIG_NEW_FRAME:
      // we got a new frame from the physical layer
      struct frame* recv_frame;
      // collect it from the signal value
      recv_frame = info->si_value.sival_ptr;
      // do frame receive logic here
      break;
    case SIG_NEW_PACKET:
      // we got a new packet from the app layer!
      struct packet* recv_packet;
      recv_packet = info->si_value.sival_ptr;
      break;

    case SIG_TIMER_ELAPSED:
      // do timer logic here
      break;
  }
}
