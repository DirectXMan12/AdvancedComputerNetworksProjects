#ifndef __FRAME_STRUCT__
#define __FRAME_STRUCT__

#define SEQ_BITS 2

struct frame
{
  unsigned int ack_num : SEQ_BITS; 
  unsigned int seq_num : SEQ_BITS; // max 4 -> 2 bits
  unsigned int split_packet : 1; // is this packet split over multiple frames
  unsigned int end_of_packet : 1; // is end of packet?
  unsigned int padding : 2; // fill up remaining space if 8 does not divide 2*SEQ_BITS + 2 
  char payload[150]; // should be up to 150
  char crc[2];
};

#endif

#ifndef __PACKET_STRUCT__
#define __PACKET_STRUCT__

struct packet
{
  unsigned int command_type : 4; // indicates the type of command that this represents
  unsigned int is_split : 1; 
  unsigned int padding : 3; // fill up remain space if need
  unsigned char pl_data_len;
  char payload[256]; // up to 256 chars 
};

#endif

#ifndef __WINDOW_LL_STRUCT__
#define __WINDOW_LL_STRUCT__

struct window_element
{
  struct window_element* prev;
  struct window_element* next;
  struct frame* fr;
};

#endif
