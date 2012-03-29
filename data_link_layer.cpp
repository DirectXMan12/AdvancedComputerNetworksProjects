
int phys_ipc_sock;
struct sockaddr_un* phys_ipc_addr;

// make sure to only use signal safe functions in here!  Check signal(7) for more info
void handle_signals(int signum)
{
  switch(signum)
  {
    case SIG_NEW_FRAME:
      // we got a new frame from the physical layer
      struct frame recv_frame;
      recvfrom(phys_ipc_sock, &recv_frame, sizeof(recv_frame), 0, phys_ipc_addr, sizeof(struct sockaddr_un));
      // do frame receive logic here
      break;
    case SIG_NEW_PACKET:
      // we got a new packet from the app layer!
      struct packet recv_packet;
      recvfrom(phys_ipc_sock, &recv_packet, sizeof(recv_packet), phys_ipc_addr, sizeof(struct sockaddr_un));
      break;

    case SIG_TIMER_ELAPSED:

      break;
  }
}
