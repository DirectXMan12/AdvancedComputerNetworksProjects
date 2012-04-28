#ifndef PROGRAM3_H
#define PROGRAM3_H

#define DEFAULT_FREQ_CHANNEL 26
#define BEACON_PERIOD 1000
#define TARGET_PERIOD 500

typedef enum{
	BEACON_MSG = 0,
	TARGET_MSG = 1, 
	REPORT_MSG = 2, 
}MSG_TYPE;

typedef struct{
	nx_uint8_t msg_type;
	nx_uint64_t base_clock;
	nx_uint8_t subnet_id; // 0 if no request, >0 specifies a request to a subnet ID
}BeaconMsg;

typedef struct{
	nx_uint8_t msg_type;
}TargetMsg;

typedef struct{
	nx_uint8_t msg_type;
	nx_uint8_t node_id;
	nx_uint8_t subnet_id;
	nx_uint64_t report_time;
}ReportMsg;

typedef struct
{
  nx_uint16_t my_rssi;
} SubnetMsg;

#define MY_ID 71
#define OTHER_ID 72

#endif
