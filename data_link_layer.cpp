#include "struct_defs.h"
#include "data_link_layer.h"
#include "signal_defs.h"
#include "physical_layer.h"
#include "err_macros.h"
#include "sys/wait.h"
#include "sys/prctl.h"
#include "stdlib.h"

#define TIMER_SECS 10
#define TIMER_NSECS 0
#define ACK_SECS 5
#define ACK_NSECS 0

pid_t phys_layer_pid;
pid_t app_layer_pid;
struct window_element* win_list;
struct window_element* end_win_list;
unsigned int frame_expected;
unsigned int num_buffered;
unsigned int packet_num = 0;;
unsigned int next_null_timer = 0;
bool acks_needed = false;

// potential variables to get two frames into the same packet
char* temp_packet = new char[300];
bool packet_frame[] = {false, false};

struct timer_info
{
  int assoc_with; // the associated sequence number
  timer_t* timer_id;
};

struct timer_info timers[4];
timer_t* ack_timer_id;

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

#define FIND_TIMER(seq_num, index) for (index = 0; index < 4; index++) if (timers[index].assoc_with == seq_num) break;
#define FIND_BLANK_TIMER(res) for (res = 0; res < 4; res++) if (timers[res].assoc_with == -1) break;

#define START_ACK_TIMER(secs, nsecs) \
{ \
  itimerspec its; \
  its.it_interval.tv_sec = 0; \
  its.it_interval.tv_nsec = 0; \
  its.it_value.tv_sec = secs; \
  its.it_value.tv_nsec = nsecs; \
  if (timer_settime(*ack_timer_id, 0, &its, NULL) < 0) \
    POST_ERR("DATA_LINK_LAYER: Could not start ACK timer (timer " << *ack_timer_id << "): " << strerror(errno)); \
}

#define STOP_ACK_TIMER \
{ \
  itimerspec its; \
  its.it_interval.tv_sec = 0; \
  its.it_interval.tv_nsec = 0; \
  its.it_value.tv_sec = 0; \
  its.it_value.tv_nsec = 0; \
  timer_settime(ack_timer_id, 0, &its, NULL); \
}

#define INC(seq_n) (seq_n == 3 ? seq_n = 0 : seq_n++)
#define INC_UPTO(seq_n, max) (seq_n == max ? seq_n = 0 : seq_n++)

// borrowed from the example pseudo-code Go Back N implementation
#define BETWEEN(a, b, c) (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((c < c) && (c < a)))

// CRC source code thanks to pycrc, modified to be used as macros
#define __CRC_REFLECT(ret, data, data_len) \
{ \
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
  short res = crc; \
  __CRC_REFLECT(res, crc, 16); \
  crc = (res ^ 0x0000) & 0xffff; \
}

// -2 is so that crc bytes are not included in the crc
#define MAKE_CRC(fr) \
{ \
  short crc; \
  crc = 0x0000; \
  char* frp = (char*)&fr; \
  __CRC_UPDATE(crc, frp, sizeof(fr)-2); \
  __CRC_FINALIZE(crc); \
  fr.crc[0] = *((char*)&crc); \
  fr.crc[1] = *(((char*)&crc)+sizeof(char)); \
}


// make sure to only use signal safe functions in here!  Check signal(7) for more info
// newer POSIX realtime signals with attached values for the win!
// see sigqueue(2) and sigaction(2) for more info about the attached values
// note the lack of external function calls here -- see above
// MACROS FTW
// TODO:
//  - check CRCs to make sure that we have a good frame(seems to be done)
//  - deal with reassembling packets from split frames(done unless we convert the packet to only send as many frames as needed)
void handle_signals(int signum, siginfo_t* info, void* context)
{
  POST_INFO("DATA_LINK_LAYER: got signal " << signum);
  if(signum == SIG_NEW_FRAME)
  {
    // we got a new frame from the physical layer
    // collect it from the signal value
    SHM_GRAB(struct frame, recv_frame, (info->si_value.sival_int));

    if (recv_frame->is_ack == 1) // this is an ack packet
    {
      POST_INFO("DATA_LINK_LAYER: got ack!");
      unsigned int last_seq = 0;
      struct window_element* last_frame = win_list;
      
      for (unsigned int i = win_list->fr->seq_num; BETWEEN(win_list->fr->seq_num, i, recv_frame->seq_num); INC(i))
      {
        // begin stop timer code
        int timer_ind = 0;
        FIND_TIMER(i, timer_ind);
        STOP_TIMER(timer_ind);
        // end stop timer code
        
        struct window_element* old_f = win_list;
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
      // check CRC
      // in order to compare, we make a temporary copy of the frame,
      // then run the crc on that, and then compare the resulting CRCs
      POST_INFO("DATA_LINK_LAYER: new incoming frame!");
      struct frame temp_fr;
      memcpy(&temp_fr, recv_frame, sizeof(struct frame));
      MAKE_CRC(temp_fr);
      if (temp_fr.crc[0] != recv_frame->crc[0] || temp_fr.crc[1] != recv_frame->crc[1]) // checksum err
      {
        // ignore frames with the error
        POST_WARN("DATA_LINK_LAYER: Dropping frame, invalid CRC: expected " << (short)temp_fr.crc[0] << (short)temp_fr.crc[1] << ", but was " << (short)recv_frame->crc[0] << (short)recv_frame->crc[1]);
        return;
      }
      else if (recv_frame->seq_num == frame_expected) //crc checked out and the frame is ok
      {
        // reassemble packet from 2 frames
        packet_frame[recv_frame->end_of_packet]=true;
        //cool math that will offset the payload onto the packet based on the frame number without a check
        // line immediately below this does not work
        //((150*recv_frame->end_of_packet)+ recv_frame->payload)temp_packet;
        
        //check to see if both halves have been sent in
        if((packet_frame[0] && packet_frame[1]) || recv_frame->split_packet == 0)
        {
          //set the frame checks to false for the next packet
          packet_frame[0]=false;
          packet_frame[1]=false;
          
          SHM_GRAB_NEW(struct packet, packet2trans, packetid);
          memcpy(packet2trans, temp_packet, sizeof(struct packet));
          // signal the physical layer
          sigval v;
          //dereference the temp_packet to give a pointer
          v.sival_int = packetid;
          sigqueue(app_layer_pid, SIG_NEW_FRAME, v);
          INC(frame_expected);
          // need to clear the temp_packet
          temp_packet = 0;
          SHM_RELEASE(struct packet, packet2trans);
          if (acks_needed == false)
          {
            acks_needed = true;
            START_ACK_TIMER(ACK_SECS, ACK_NSECS);
            POST_INFO("DATA_LINK_LAYER: ACK timer started...");
          }
          
        }
      }
      else
      {
        POST_WARN("DATA_LINK_LAYER: Dropping frame: expected " << frame_expected << ", got " << recv_frame->seq_num);
      }
      
    }
    SHM_RELEASE(struct frame, recv_frame);
    SHM_DESTROY((info->si_value.sival_int));
  }
  else if(signum == SIG_NEW_PACKET)
  { 
    POST_INFO("DATA_LINK_LAYER: Beginning processing of new packet from application layer...");
    // we got a new packet from the app layer!
    SHM_GRAB(struct packet, recv_packet, (info->si_value.sival_int));
    // TODO: check to see if this frame really needs to be split across two frames
    //// haven't checked the requirements but the way the packet is assembled it works better 
    //// with always receiving two frames
    // this code initializes two frames and splits the packet over them
    SHM_GRAB_NEW(struct frame, fr1, fr1id);
    fr1->is_ack = 0;
    if (end_win_list != NULL) fr1->seq_num = end_win_list->fr->seq_num;
    else fr1->seq_num = 0;
    fr1->split_packet = 0;
    INC(fr1->seq_num);
    fr1->end_of_packet = 0;
    INC_UPTO(packet_num, 8); // 3 bits = 8
    fr1->packet_num = packet_num;
    memcpy(fr1->payload, recv_packet->payload, 150);
    MAKE_CRC((*fr1));
    num_buffered += 1;

    frame *fr2q = NULL;
    int fr2id = -1;
    
    //checking if the packet needs a second frame
    if(recv_packet->pl_data_len > 148)
    {
      SHM_GRAB_NEW(struct frame, fr2, fr2idtmp);
      fr2->is_ack = 0;
      fr2->seq_num = fr1->seq_num;
      INC(fr2->seq_num);
      fr2->end_of_packet = 1;
      fr2->packet_num = packet_num;
      memcpy(fr2->payload, recv_packet->payload+150, 106);
      unsigned int ct = recv_packet->command_type;
      memcpy(fr2->payload+106, &ct, 1); // copy the remaining parts of the packet struct over
      MAKE_CRC((*fr2));
      //sets both frames to say that there should be 2 frames
      fr1->split_packet = 1;
      fr2->split_packet = 1;
      num_buffered += 1;
      fr2q = fr2;
      fr2id = fr2idtmp;
    }

    window_element win_elem1;
    win_elem1.fr = new frame;
    memcpy(win_elem1.fr, fr1, sizeof(struct frame));  // we need to create a new one since we free the shared memory later
    win_elem1.prev = end_win_list;
    win_elem1.next = NULL;
    if (end_win_list != NULL) end_win_list->next = &win_elem1;
    end_win_list = &win_elem1;

    if (win_list == NULL) win_list = &win_elem1;

    if (fr2q != NULL)
    {
      window_element win_elem2;
      win_elem2.fr = new frame;
      memcpy(win_elem2.fr, fr2q, sizeof(struct frame));
      win_elem2.prev = end_win_list;
      win_elem2.next = NULL;
      end_win_list->next = &win_elem2;
      end_win_list = &win_elem2;
    }

    sigval v1;
    v1.sival_int = fr1id;
    sigqueue(phys_layer_pid, SIG_NEW_FRAME, v1);
    int tv1 = 0;
    FIND_BLANK_TIMER(tv1);
    START_TIMER(tv1, TIMER_SECS, TIMER_NSECS);
    timers[tv1].assoc_with = fr1->seq_num;
    SHM_RELEASE(struct frame, fr1);

    if (fr2q != NULL)
    {
      sigval v2;
      v2.sival_int = fr2id;
      sigqueue(phys_layer_pid, SIG_NEW_FRAME, v2);
      int tv2 = 0;
      FIND_BLANK_TIMER(tv2);
      START_TIMER(tv2, TIMER_SECS, TIMER_NSECS);
      timers[tv2].assoc_with = fr2q->seq_num;
      SHM_RELEASE(struct frame, fr2q);
    }

    SHM_RELEASE(struct packet, recv_packet);
    SHM_DESTROY((info->si_value.sival_int));
  } 
  else if(signum == SIG_TIMER_ELAPSED)
  {
    if (info->si_value.sival_int == 0) // this is a frame timer timeout
    {
      POST_INFO("DATA_LINK_LAYER: Frame timer went off...");
      window_element* we = win_list; // represents the next frame to send here
      for (int i = 0; i < num_buffered && i < 4; i++)
      {
        // resend the frame
        SHM_GRAB_NEW(struct frame, fr2trans, frid);
        memcpy(fr2trans, we->fr, sizeof(struct frame));
        sigval v1;
        v1.sival_int = frid;
        sigqueue(phys_layer_pid, SIG_NEW_FRAME, v1);
        int tv1 = 0;
        FIND_BLANK_TIMER(tv1);
        START_TIMER(tv1, TIMER_SECS, TIMER_NSECS);
        timers[tv1].assoc_with = we->fr->seq_num;

        // increment the next one that we will send
        we = we->next;
        SHM_RELEASE(struct frame, fr2trans);
      }
      POST_INFO("DATA_LINK_LAYER: Resent frames...");
    }
    else // this is an ack timer timeout
    {
      POST_INFO("DATA_LINK_LAYER: ACK timer went off...");

      SHM_GRAB_NEW(struct frame, ack_fr, shmid);

      memset(ack_fr->payload, 0, sizeof(ack_fr->payload));
      ack_fr->is_ack = 1;
      ack_fr->seq_num = frame_expected;
      ack_fr->end_of_packet = 0;
      ack_fr->packet_num = 0;
      //MAKE_CRC((*ack_fr));
      ack_fr->crc[0] = 0;
      ack_fr->crc[1] = 0;

      union sigval v;
      v.sival_int = shmid;
      sigqueue(phys_layer_pid, SIG_NEW_FRAME, v);
      SHM_RELEASE(struct frame, ack_fr);
      acks_needed = false;
      STOP_ACK_TIMER;
      POST_INFO("DATA_LINK_LAYER: Sent ACK...");
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

void init_data_link_layer(bool is_server, pid_t app_layer, bool is_comm_process, int comm_sock)
{
  srand(time(NULL));
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

  if (phys_layer_pid == 0) // this will become the physical layer
  {
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
