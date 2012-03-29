#ifndef __FRAME_STRUCT__
#define __FRAME_STRUCT__

struct frame
{
  unsigned int is_ack : 1; 
  unsigned int seq_num : 2; // max 4 -> 2 bits
  unsigned int end_of_packet : 1; // is end of packet?
  unsigned int packet_num : 4; // ids packet to which this frame belongs, b/c packets can be split across multiple frames
  char payload[150]; // should be up to 150
  char crc[2];
};

#endif

#ifndef __PACKET_STRUCT__
#define __PACKET_STRUCT__

struct packet
{
  unsigned int command_type : 4; // indicates the type of command that this represents
  unsigned int seq_num : 4;  // TODO: what was this for?
  char payload[256]; // up to 256 chars 
};

#endif
