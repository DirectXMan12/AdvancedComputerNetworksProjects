#include "struct_defs.h"
#include "data_link_layer.h"
#include "signal_defs.h"
#include "physical_layer.h"
#include "err_macros.h"
#include "sys/wait.h"
#include "sys/prctl.h"
#include "stdlib.h"
#include <iostream>
#include <iomanip>
#include <stdio.h>

using namespace std;

#define TIMER_SECS 2
#define TIMER_NSECS 500000
#define ACK_SECS 1
#define ACK_NSECS 0

#define WIN_SIZE 4

// for internal workings
pid_t phys_layer_pid;
pid_t app_layer_pid;
unsigned int packet_num = 0;;
unsigned int next_NULL_timer = 0;
int frame_counter = 0;

struct timer_info
{
  int assoc_with; // the associated sequence number
  timer_t* timer_id;
};

struct timer_info timers[4];

timer_t* ack_timer_id;

// for the protocol
unsigned short nfs = WIN_SIZE; // WIN_SIZE and not 0 b/c first frame needs to be 0
unsigned short ack_expected = 0;
unsigned short frame_expected = 0;
struct packet** buffer;
unsigned short num_buffered = 0;
unsigned short buff_index = 0;

// potential variables to get two frames into the same packet
char* temp_packet = new char[sizeof(struct packet)];
bool packet_frame[] = {false, false};


#define PRINT_FRAME(f) cout << "{ is_ack: " << f->is_ack << ", seq_num: " << f->seq_num << ", split_packet: " << f->split_packet << ", end_of_packet: " << f->end_of_packet << ", packet_num: " << f->packet_num << "payload: '"; for(int i = 0; i < 150; i++) printf("%x", f->payload[i]); cout << "', crc: "; printf("%hhx%hhx", f->crc[0], f->crc[1]); cout << " }" << endl;

// also stops ack timer if there is one
#define START_TIMER(timers_index, secs, nsecs) \
{ \
  itimerspec its; \
  its.it_interval.tv_sec = 0; \
  its.it_interval.tv_nsec = 0; \
  its.it_value.tv_sec = secs; \
  its.it_value.tv_nsec = nsecs; \
  if (timer_settime(*(timers[timers_index].timer_id), 0, &its, NULL) < 0) \
    POST_ERR("DATA_LINK_LAYER: Could not start timer " << *(timers[timers_index].timer_id) << ": " << strerror(errno)); \
}

#define STOP_TIMER(timers_index) \
{ \
  itimerspec its; \
  its.it_interval.tv_sec = 0; \
  its.it_interval.tv_nsec = 0; \
  its.it_value.tv_sec = 0; \
  its.it_value.tv_nsec = 0; \
  timer_settime(*(timers[timers_index].timer_id), 0, &its, NULL); \
  timers[timers_index].assoc_with = -1; \
}

#define START_ACK_TIMER(secs, nsecs) \
{ \
  itimerspec itso; \
  timer_gettime(*ack_timer_id, &itso); \
  if (itso.it_value.tv_sec == 0 && itso.it_value.tv_nsec == 0) \
  { \
    itimerspec its; \
    its.it_interval.tv_sec = 0; \
    its.it_interval.tv_nsec = 0; \
    its.it_value.tv_sec = secs; \
    its.it_value.tv_nsec = nsecs; \
    if (timer_settime(*ack_timer_id, 0, &its, NULL) < 0) \
      POST_ERR("DATA_LINK_LAYER: Could not start ACK timer (timer " << *ack_timer_id << "): " << strerror(errno)); \
  } \
}

#define STOP_ACK_TIMER \
{ \
  itimerspec its; \
  its.it_interval.tv_sec = 0; \
  its.it_interval.tv_nsec = 0; \
  its.it_value.tv_sec = 0; \
  its.it_value.tv_nsec = 0; \
  timer_settime(*ack_timer_id, 0, &its, NULL); \
}

#define FIND_TIMER(seq_num, index) for (index = 0; index < 4; index++) if (timers[index].assoc_with == seq_num) break;
#define FIND_BLANK_TIMER(res) for (res = 0; res < 4; res++) if (timers[res].assoc_with == -1) break;

#define FIND_OR_CREATE_TIMER(res, target) \
{ \
  FIND_TIMER(target, res); \
  if (res > 3) \
  { \
    FIND_BLANK_TIMER(res); \
  } \
}


#define INC(seq_n) (seq_n == WIN_SIZE ? seq_n = 0 : seq_n++)
#define INC_UPTO(seq_n, max) (seq_n == max ? seq_n = 0 : seq_n++)

// borrowed from the example pseudo-code Go Back N implementation
#define BETWEEN(a, b, c) (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((c < c) && (c < a)))

// CRC source code thanks to pycrc, modified to be used as macros
#define __CRC_REFLECT(ret, datao, data_len) \
{ \
  short data = datao; \
  ret = data & 0x01; \
  for (unsigned int j = 1; j < data_len; j++) \
  { \
    data >>= 1; \
    ret = (ret << 1) | (data & 0x01); \
  } \
}

#define __CRC_UPDATE(crc, data, datalen) \
{ \
  bool bit; \
  unsigned char c; \
  unsigned int data_len = datalen; \
  while (data_len--) \
  { \
    data++; \
    __CRC_REFLECT(c, (*data), 8); \
    for (unsigned int i = 0; i < 8; i++) \
    { \
      bit = crc & 0x8000; \
      crc = (crc << 1) | ((c >> (7 - i)) & 0x01); \
      if (bit) crc ^= 0x8005; \
    } \
    crc &= 0xffff; \
  } \
  crc = crc & 0xffff; \
}
#define __CRC_FINALIZE(crc) \
{ \
  unsigned int i; \
  bool bit; \
  for (i=0; i < 16; i++) \
  { \
    bit = crc & 0x8000; \
    crc = (crc << 1) | 0x00; \
    if (bit) crc ^= 0x8005; \
  } \
  unsigned short res = crc; \
  __CRC_REFLECT(res, crc, 16); \
  crc = (res ^ 0x0000) & 0xffff; \
}

// -2 is so that crc bytes are not included in the crc
#define MAKE_CRC(fr) \
{ \
  unsigned short crc; \
  crc = 0x0000; \
  unsigned char* frp = (unsigned char*)fr; \
  __CRC_UPDATE(crc, frp, sizeof(fr)-2*sizeof(char)); \
  __CRC_FINALIZE(crc); \
  fr->crc[0] = *((char*)&crc); \
  fr->crc[1] = *(((char*)&crc)+sizeof(char)); \
}


// make sure to only use signal safe functions in here!  Check signal(7) for more info
// newer POSIX realtime signals with attached values for the win!
// see sigqueue(2) and sigaction(2) for more info about the attached values
// note the lack of external function calls here -- see above
// MACROS FTW
// TODO:
//  - deal with reassembling packets from split frames(done unless we convert the packet to only send as many frames as needed)
void handle_signals(int signum, siginfo_t* info, void* context)
{
  //POST_INFO("DATA_LINK_LAYER: got signal " << signum);
  if(signum == SIG_NEW_FRAME)
  {
    // we got a new frame from the physical layer
    // collect it from the signal value
    SHM_GRAB(struct frame, recv_frame, (info->si_value.sival_int));

    // check CRC
    // in order to compare, we make a temporary copy of the frame,
    // then run the crc on that, and then compare the resulting CRCs
    struct frame* temp_fr = new frame;
    memcpy(temp_fr, recv_frame, sizeof(struct frame));
    MAKE_CRC(temp_fr);
    if (temp_fr->crc[0] != recv_frame->crc[0] || temp_fr->crc[1] != recv_frame->crc[1]) // checksum err
    {
      short* calc_crc = (short*)temp_fr->crc;
      short* act_crc = (short*)recv_frame->crc;
      // ignore frames with the error
      POST_WARN("DATA_LINK_LAYER: Dropping frame, invalid CRC: expected 0x" << hex << *calc_crc << ", but was 0x" << hex << *act_crc);
    }
    else
    {
      if (recv_frame->seq_num == frame_expected)
      {
        SHM_GRAB_NEW(struct packet, pts, packid);
        memset(pts, 0, sizeof(struct packet));
        memcpy(pts, recv_frame->payload, 150);

        sigval v;
        v.sival_int = packid;
        sigqueue(app_layer_pid, SIG_NEW_PACKET, v);
        SHM_RELEASE(struct packet, pts);

        INC(ack_expected);
      }

      while(BETWEEN(ack_expected, recv_frame->ack_num, nfs))
      {
        num_buffered--;

        int timer_ind = 0;
        FIND_TIMER(ack_expected, timer_ind);
        STOP_TIMER(timer_ind);

        INC(ack_expected);
      }
    }  
    
    SHM_RELEASE(struct frame, recv_frame);
    SHM_DESTROY((info->si_value.sival_int));
  }
  else if(signum == SIG_NEW_PACKET)
  { 
    // we got a new packet from the app layer!
    SHM_GRAB(struct packet, recv_packet, (info->si_value.sival_int));

    SHM_GRAB_NEW(struct frame, fts, shmid);

    // TODO: deal with splitting packets over 2 frames/packet number

    memset(buffer[nfs], 0, sizeof(struct packet)); // clear out the old buffer
    memcpy(buffer[nfs], recv_packet, sizeof(struct packet)); // copy down for later uses
    num_buffered++;
    
    memset(fts, 0, sizeof(struct frame)); // clear out the frame
    memcpy(fts->payload, recv_packet, 150);
    fts->seq_num = nfs;
    fts->ack_num = (frame_expected + WIN_SIZE - 1) % WIN_SIZE;
    MAKE_CRC(fts);

    sigval v;
    v.sival_int = shmid;
    sigqueue(phys_layer_pid, SIG_NEW_FRAME, v);

    int tv1 = 0;
    FIND_OR_CREATE_TIMER(tv1, fts->seq_num);
    START_TIMER(tv1, TIMER_SECS, TIMER_NSECS);
    timers[tv1].assoc_with = fts->seq_num;

    SHM_RELEASE(struct frame, fts);

    SHM_RELEASE(struct packet, recv_packet);
    SHM_DESTROY((info->si_value.sival_int));

    INC(nfs);

    POST_INFO("sending " << nfs << " with ack " << (frame_expected == WIN_SIZE ? 0 : frame_expected+1));
  } 
  else if(signum == SIG_TIMER_ELAPSED)
  {
    nfs = ack_expected;
    for (int i = 0; i < num_buffered; i++)
    {
      SHM_GRAB_NEW(struct frame, fts, shmid);

      // TODO: deal with splitting packets over 2 frames/packet number
      memset(fts, 0, sizeof(struct frame)); // clear out the frame
      memcpy(fts->payload, buffer[nfs], 150);
      fts->seq_num = nfs;
      fts->ack_num = (frame_expected + WIN_SIZE - 1) % WIN_SIZE;
      MAKE_CRC(fts);

      sigval v;
      v.sival_int = shmid;
      sigqueue(phys_layer_pid, SIG_NEW_FRAME, v);

      int tv1 = 0;
      FIND_OR_CREATE_TIMER(tv1, nfs);
      START_TIMER(tv1, TIMER_SECS, TIMER_NSECS);
      timers[tv1].assoc_with = fts->seq_num;

      SHM_RELEASE(struct frame, fts);

      INC(nfs);
    }
  }
  else if (signum == SIG_FLOW_ON) // we are ready to go (from phys layer)
  {
    sigval v;
    v.sival_int = 1; // turn flow on
    sigqueue(app_layer_pid, SIG_FLOW_ON, v);
  }
  else if (signum == SIG_NEW_CLIENT)
  {
    sigqueue(app_layer_pid, SIG_NEW_CLIENT, info->si_value);
  }
  else if (signum == SIGSEGV)
  {
    POST_ERR("DATA_LINK_LAYER: Segfault!");
    exit(2);
  }

  if (signum == SIG_NEW_FRAME || signum == SIG_NEW_PACKET || signum == SIG_TIMER_ELAPSED)
  {
    if (num_buffered < WIN_SIZE-1)
    {
      sigval v;
      v.sival_int = 1;
      sigqueue(app_layer_pid, SIG_FLOW_ON, v);
    }
    else
    {
      sigval v;
      v.sival_int = 0;
      sigqueue(app_layer_pid, SIG_FLOW_ON, v);
    }
  }

}

void init_data_link_layer(bool is_server, pid_t app_layer, bool is_comm_process, int comm_sock)
{
  srandom(getpid());
  struct sigaction act;
  act.sa_sigaction = &handle_signals;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO | SA_RESTART;

  sigaction(SIG_NEW_FRAME, &act, 0);
  sigaction(SIG_NEW_PACKET, &act, 0);
  sigaction(SIG_TIMER_ELAPSED, &act, 0);
  sigaction(SIG_FLOW_ON, &act, 0);
  sigaction(SIG_NEW_CLIENT, &act, 0);
  sigaction(SIGSEGV, &act, 0);

  app_layer_pid = app_layer;

  pid_t my_pid = getpid();
  phys_layer_pid = fork();

  // initialize the timer object which are later started and stopped with 
  // timer_settime (see timer_settime(2))
  sigevent notify_method;
  notify_method.sigev_signo = SIG_TIMER_ELAPSED;
  notify_method.sigev_notify = SIGEV_SIGNAL;
  notify_method.sigev_value.sival_int = 0; // this is a frame timer 

  for (int i = 0; i < 4; i++)
  {
    timers[i].timer_id = new timer_t;
    timers[i].assoc_with = -1;
    if (timer_create(CLOCK_MONOTONIC, &notify_method, timers[i].timer_id) < 0)
    {
      POST_ERR("DATA_LINK_LAYER: Issue initializing timers");
    }
  }

  sigevent notify_ack;
  notify_ack.sigev_signo = SIG_TIMER_ELAPSED;
  notify_ack.sigev_notify = SIGEV_SIGNAL;
  notify_ack.sigev_value.sival_int = 1; // this is the ack timer

  ack_timer_id = new timer_t;
  if (timer_create(CLOCK_MONOTONIC, &notify_ack, ack_timer_id) < 0)
  {
    POST_ERR("DATA_LINK_LAYER: Issue initializing ACK timer");
  }

  buffer = (packet**) new char[sizeof(struct packet*)*WIN_SIZE];
  for (int i = 0; i < WIN_SIZE; i++)
  {
    buffer[i] = new packet;
  }

  if (phys_layer_pid == 0) // this will become the physical layer
  {
    srandom(getpid());
    if(is_server) prctl(PR_SET_NAME, (unsigned long) "prog1s_ps", 0, 0, 0);
    else prctl(PR_SET_NAME, (unsigned long) "prog1s_ps", 0, 0, 0);

    PhysicalLayer p(my_pid, is_server);
    if (is_server)
    {
      if (!is_comm_process)
      { 
        p.init_connection(0, 0, is_comm_process);
      }
      else
      {
        POST_INFO("DATA_LINK_LAYER: we are a server comm process");
        p.init_connection(0, (char*) &comm_sock, is_comm_process);
      }
    }
    else
    {
      p.init_connection("client a", "localhost", is_comm_process);
    }
  }

  while(1) waitpid(-1, NULL, 0);
}
