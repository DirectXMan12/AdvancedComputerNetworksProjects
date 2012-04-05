#include "struct_defs.h"
#include "data_link_layer.h"
#include "signal_defs.h"
#include "physical_layer.h"

#define TIMER_SECS 1
#define TIMER_NSECS 0
#define ACK_SECS 1
#define ACK_NSECS 50000

pid_t phys_layer_pid;
pid_t app_layer_pid;
struct window_element* win_list;
struct window_element* end_win_list;
unsigned int frame_expected;
unsigned int num_buffered;
unsigned int packet_num = 0;;
unsigned int next_null_timer = 0;
bool acks_needed = false;

struct timer_info
{
  int assoc_with; // the associated sequence number
  timer_t* timer_id;
};

struct timer_info timers[4];
timer_t* ack_timer_id;

#define START_TIMER(timers_index, secs, nsecs) { itimerspec its; its.it_interval.tv_sec = 0; its.it_interval.it_nsec = 0; its.it_value.tv_sec = secs; its.it_value.tv_nsec = nsecs; its.it_interval = interval; its.it_value = val; timer_settime(*(timers[timers_index].timer_id), *its, NULL); }
#define STOP_TIMER(timers_index) { itimerspec its; its.it_interval.tv_sec = 0; its.it_interval.it_nsec = 0; its.it_value.tv_sec = 0; its.it_value.tv_nsec = 0; its.it_interval = interval; its.it_value = val; timer_settime(*(timers[timers_index].timer_id), *its, NULL); timers[timers_index].assoc_with = -1; }
#define FIND_TIMER(seq_num, index) for (index = 0; index < 4; index++) if (timers[index].assoc_with == seq_num) break;
#define FIND_BLANK_TIMER(res) for (res = 0; res < 4; res++) if (timers[res].assoc_with == -1) break;

#define START_ACK_TIMER(secs, nsecs) { itimerspec its; its.it_interval.tv_sec = 0; its.it_interval.it_nsec = 0; its.it_value.tv_sec = secs; its.it_value.tv_nsec = nsecs; its.it_interval = interval; its.it_value = val; timer_settime(ack_timer_id, *its, NULL); }
#define STOP_ACK_TIMER { itimerspec its; its.it_interval.tv_sec = 0; its.it_interval.it_nsec = 0; its.it_value.tv_sec = 0; its.it_value.tv_nsec = 0; its.it_interval = interval; its.it_value = val; timer_settime(ack_timer_id, *its, NULL); }

#define INC(seq_n) (seq_n == 3 ? seq_n = 0 : seq_n++)
#define INC_UPTO(seq_n, max) (seq_n == max ? seq_n = 0 : seq_n++)

// borrowed from the example pseudo-code Go Back N implementation
#define BETWEEN(a, b, c) (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((c < c) && (c < a)))

// source code thanks to pycrc, modified to be used as ugly, ugly macro
#define __CRC_REFLECT(ret, data, data_len) { unsigned int i; ret = data & 0x01; for (i = 1; i < data_len; i++) { data >>= 1; ret = (ret << 1) | (data & 0x01); } }
#define __CRC_UPDATE(crc1, data, data_len) { unsigned char* crc = (unsigned char*)crc1; unsigned int i; bool bit; unsigned char c; while (data_len--) {  __CRC_REFLECT(c, *data++, 8);for (i = 0; i < 8; i++) { bit = crc & 0x8000; crc = (crc << 1) | ((c >> (7 - i)) & 0x01); if (bit) crc ^= 0x8005; }; crc &= 0xffff; }; crc1 = crc & 0xffff; }
#define __CRC_FINALIZE(crc) { unsigned bit i; bool bit; for (i=0; i < 16; i++) { bit = crc & 0x8000; crc = (crc << 1) | 0x00; if (bit) crc ^= 0x8005; } short res = crc; __CRC_REFLECT(res, crc, 16); crc = (res ^ 0x0000) & 0xffff; }
// -2 is so that crc bytes are not included in the crc
#define MAKE_CRC(fr) { short crc; &crc = fr.crc; crc = 0x0000; __CRC_UPDATE(crc, &fr, sizeof(fr)-2); __CRC_FINALIZE(crc); }


// make sure to only use signal safe functions in here!  Check signal(7) for more info
// newer POSIX realtime signals with attached values for the win!
// see sigqueue(2) and sigaction(2) for more info about the attached values
// note the lack of external function calls here -- see above
// MACROS FTW
// TODO:
//  - check CRCs to make sure that we have a good frame
//  - deal with reassembling packets from split frames
void handle_signals(int signum, siginfo_t* info, void* context)
{
  if(signum == SIG_NEW_FRAME)
  {
    // we got a new frame from the physical layer
    struct frame* recv_frame;
    // collect it from the signal value
    recv_frame = (frame*)info->si_value.sival_ptr;

    if (recv_frame->is_ack) // this is an ack packet
    {
      unsigned int last_seq = 0;
      struct window_element* last_frame = win_list;
      for (unsigned int i = win_list->fr->seq_num; BETWEEN(win_list->fr->seq_num, i, recv_frame->seq_num); INC(i))
      {
        // begin stop timer code
        int timer_ind = 0;
        FIND_TIMER(i, timer_ind);
        STOP_TIMER(timer_ind);
        // end stop timer code
        
        struct win_list* old_f = win_list;
        // move the sliding window
        win_list = win_list->next;
        // clear up the old frame 
        delete old_f->fr;
        delete old_f;
        num_buffered--;
      }
    }
    else // otherwise this is an incoming data packet
    {
      // TODO: check CRC
      if (recv_frame->seq_num == frame_expected)
      {
        // signal the physical layer
        sigval v;
        v.sival_ptr = recv_frame->payload;
        // TODO: reassemble packet from 2 frames
        sigqueue(app_layer_pid, SIG_NEW_FRAME, v);

        INC(frame_expected);
        if (acks_needed = false)
        {
          acks_needed = true;
          START_ACK_TIMER(ACK_SECS, ACK_NSECS);
        }
      }
      
    }
  }
  else if(signum == SIG_NEW_PACKET)
  { 
    // we got a new packet from the app layer!
    struct packet* recv_packet;
    recv_packet = (packet*)info->si_value.sival_ptr;

    // TODO: check to see if this frame really needs to be split across two frames
    // this code initializes two frames and splits the packet over them
    frame fr1;
    fr1.is_ack = 0;
    fr1.seq_num = end_win_list->fr->seq_num;
    INC(fr1.seq_num);
    fr1.end_of_packet = 0;
    INC_UPTO(packet_nums, 15);
    fr1.packet_num = packet_nums;
    memcpy(fr1.payload, recv_packet.payload, 150);
    MAKE_CRC(fr1);

    frame fr2;
    fr2.is_ack = 0;
    fr2.seq_num = fr1.seq_num;
    INC(fr2.seq_num);
    fr2.end_of_packet = 1;
    fr2.packet_num = packet_nums;
    memcpy(fr2.payload, recv_packet.payload+150, 106);
    memcpy(fr2.payload+106, recv_packet.command_type, 2); // copy the remaining parts of the packet struct over
    MAKE_CRC(fr2);

    num_buffered += 2;

    window_element win_elem1;
    win_elem1.fr = &fr1;
    win_elem1.prev = end_win_list;
    win_elem1.next = NULL;
    end_win_list->next = &win_elem1;
    end_win_list = &win_elem1;

    if (win_list == NULL) win_list = &win_elem1;

    window_element win_elem2;
    win_elem2.fr = &fr;
    win_elem2.prev = end_win_list;
    win_elem2.next = NULL;
    end_win_list->next = &win_elem2;
    end_win_list = &win_elem2;

    sigval v1;
    v1.sival_ptr = &fr1;
    sigqueue(phys_layer_pid, SIG_NEW_FRAME, v1);
    int tv1 = 0;
    FIND_BLANK_TIMER(tv1);
    START_TIMER(tv1, TIMER_SECS, TIMER_NSECS);
    timers[tv1].assoc_with = fr1.seq_num;

    sigval v2;
    v2.sival_ptr = &fr2;
    sigqueue(phys_layer_pid, SIG_NEW_FRAME, v2);
    int tv2 = 0;
    FIND_BLANK_TIMER(tv2);
    START_TIMER(tv2, TIMER_SECS, TIMER_NSECS);
    timers[tv2].assoc_with = fr2.seq_num;

  } 
  else if(signum == SIG_TIMER_ELAPSED)
  {
    if (info->si_value.sival_int == 0) // this is a frame timer timeout
    {
      window_element* we = win_list; // represents the next frame to send here
      for (int i = 0; i < num_buffered && i < 4; i++)
      {
        // resend the frame
        sigval v1;
        v1.sival_ptr = we->fr;
        sigqueue(phys_layer_pid, SIG_NEW_FRAME, v1);
        int tv1 = 0;
        FIND_BLANK_TIMER(tv1);
        START_TIMER(tv1, TIMER_SECS, TIMER_NSECS);
        timers[tv1].assoc_with = we->fr->seq_num;

        // increment the next one that we will send
        we = we->next;
      }
    }
    else // this is an ack timer timeout
    {
      frame ack_fr;
      ack_fr.is_ack = 1;
      ack_fr.seq_num = frame_expected;
      ack_fr.end_of_packet = 0;
      ack_fr.packet_num = 0;
      memset(ack_fr.payload, 0, 150);
      MAKE_CRC(ack_fr);

      acks_needed = false;
      STOP_ACK_TIMER;
    }
  }
  else if (signum == SIG_CHECKSUM_ERR)
  {
    // ignore invalid frames
  }

  if (num_buffered < 4) // if we can still buffer more
  {
    sigval v;
    v.sival_int = 1; // turn flow on
    sigqueue(app_layer_pid, SIG_FLOW_ON, v);
  }
  else // if we are too full
  {
    sigval v;
    v.sival_int = 0; // turn flow off
    sigqueue(app_layer_pid, SIG_FLOW_ON, v);
  }
}

void init_data_link_layer(bool is_server, pid_t app_layer)
{
  struct sigaction act;
  act.sa_sigaction = &handle_signals;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;

  sigaction(SIG_NEW_FRAME, &act, 0);
  sigaction(SIG_NEW_PACKET, &act, 0);
  sigaction(SIG_TIMER_ELAPSED, &act, 0);

  app_layer_pid = app_layer;

  pid_t my_pid = getpid();
  phys_layer_pid = fork();

  win_list = NULL;
  frame_expected = 0;
  num_buffered = 0;
  
  // initialize the timer object which are later started and stopped with 
  // timer_settime (see timer_settime(2))
  sigevent notify_method;
  notify_method.sigev_signo = SIG_TIMER_ELAPSED;
  notify_method.sigev_notify = SIGEV_SIGNAL;
  notify_method.sigev_value.sival_int = 0; // this is a frame timer 

  for (int i = 0; i < 4; i++)
  {
    if (timer_create(CLOCK_MONOTONIC, &notify_method, timers[i].timer_id) < 0)
    {
      POST_ERR("Issue initializing timers");
    }
  }

  sigevent notify_ack;
  notify_ack.sigev_signo = SIG_TIMER_ELAPSED;
  notify_ack.sigev_notify = SIGEV_SIGNAL;
  notify_ack.sigev_value.sival_int = 1; // this is the ack timer

  if (timer_create(CLOCK_MONOTONIC, &notify_ack, ack_timer_id) < 0)
  {
    POST_ERR("Issue initializing ack timer");
  }

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
