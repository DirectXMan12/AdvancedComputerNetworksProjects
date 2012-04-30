#include <Timer.h>
#include "MsgDefs.h"
#include "printf.h"

module SubnetC
{
  uses interface Boot;
  uses interface Leds;
  
  uses interface CC2420Packet;

  uses interface SplitControl as AMControl;

  uses interface Packet as SubnetPacket;
  uses interface AMPacket as SubnetAMPacket;
  uses interface AMSend as SubnetSend;
  uses interface Receive as SubnetRecv;

  uses interface Packet as BroadcastPacket;
  uses interface AMPacket as BroadcastAMPacket;
  uses interface AMSend as BroadcastSend;
  uses interface Receive as BroadcastRecv;

  uses interface CC2420Config as RadioConf;

  uses interface Timer<TMilli> as BeaconTimer;

  uses interface LocalTime<TMicro> as Time;
}

implementation
{
  uint8_t beacon_periods_passed = 0;
  bool am_near_node = FALSE;
  bool busy = FALSE;
  int8_t my_last_rssi = 0;
  int8_t other_last_rssi = 0;
  message_t bcast_pkt; // for messages on the broadcast channel
  message_t comm_pkt;  // for messages on the subnet
  uint16_t coming_from_sync = NONE; 

  void changeChannel(uint16_t coming_from)
  {
    //printf("change to %d\n", coming_from);
    atomic
    {
      coming_from_sync = coming_from;
      switch(coming_from)
      {
        case INIT_CHANGE:
          call RadioConf.setChannel(DEFAULT_FREQ_CHANNEL);
          break;
        case BACK_TO_WAITING:
          call RadioConf.setChannel(DEFAULT_FREQ_CHANNEL);
          break;
        case RUN_SEND_REPORT:
          call RadioConf.setChannel(DEFAULT_FREQ_CHANNEL);
          break;
        case RUN_SEND_SUBNET:
          call RadioConf.setChannel(SUBNET_COMM_CHANNEL);
          break;
        case RUN_SEND_SUBNET_THEN_REPORT:
          call RadioConf.setChannel(SUBNET_COMM_CHANNEL);
          break;
      } 
      call RadioConf.sync();
    }
  }
  

  event void Boot.booted()
  {
    call AMControl.start();
  }

  event void AMControl.startDone(error_t res)
  {
    if (res == SUCCESS)
    {
      // we are good to go!
      call BeaconTimer.startPeriodic(BEACON_PERIOD);
      changeChannel(INIT_CHANGE);
    }
    else
    {
      call AMControl.start();
    }
  }

  event void AMControl.stopDone(error_t res) {}

  void sendReportMsg()
  {
    if (!busy)
    {
      ReportMsg* rmpkt = (ReportMsg*)(call BroadcastPacket.getPayload(&bcast_pkt, sizeof(ReportMsg)));
      printf("I like reports1\n");
      if (rmpkt == NULL) return;
      printf("I like reports2\n");

      rmpkt->msg_type = REPORT_MSG;
      rmpkt->node_id = MY_ID;
      rmpkt->subnet_id = 7;
      rmpkt->report_time = call Time.get();

      if (call BroadcastSend.send(AM_BROADCAST_ADDR, &bcast_pkt, sizeof(ReportMsg)) == SUCCESS)
      {
        atomic busy = TRUE;
      }
    }
    else printf("busy1\n");
  }

  void sendSubnetMsg()
  {
    if (!busy)
    {
      SubnetMsg* smpkt = (SubnetMsg*)(call SubnetPacket.getPayload(&comm_pkt, sizeof(SubnetMsg)));
      if (call RadioConf.getChannel() != SUBNET_COMM_CHANNEL) printf("beluga!\n");
      if (smpkt == NULL) return;

      smpkt->my_rssi = my_last_rssi;
      smpkt->is_subnet_msg = 1;

      if (call SubnetSend.send(AM_BROADCAST_ADDR, &comm_pkt, sizeof(SubnetMsg)) == SUCCESS)
      {
        atomic busy = TRUE;
      }
    }
    else printf("busy2\n");
  }

  event void RadioConf.syncDone(error_t res)
  {
    if (res == SUCCESS)
    {
      if (coming_from_sync == RUN_SEND_SUBNET)
      {
        sendSubnetMsg();
      }
      else if (coming_from_sync == RUN_SEND_REPORT)
      {
        sendReportMsg();
      }
      else if (coming_from_sync == RUN_SEND_SUBNET_THEN_REPORT)
      {
        sendSubnetMsg();
      }
      else
      {
        atomic coming_from_sync = NONE;
      }
    }
    else
    {
      call RadioConf.sync();
    }
  }

  event void BeaconTimer.fired()
  {
    beacon_periods_passed++;

    if (beacon_periods_passed > 4)
    {
      // we have been disconnected from the base station
      call Leds.led2Off();
      printf("no beacon");
    }
  }

  void handleRssiChange(bool was_from_subnet)
  {
    bool am_new_near_node = FALSE;
    bool delta = FALSE;
    if (my_last_rssi > other_last_rssi)
    {
      am_new_near_node = TRUE;
    }
    else if (my_last_rssi < other_last_rssi)
    {
      am_new_near_node = FALSE;
    }
    else
    {
      printf("tie  %d %d, %d %d\n", am_new_near_node, am_near_node, my_last_rssi, other_last_rssi);
      if (MY_ID < OTHER_ID)
      {
        am_new_near_node = TRUE;
      }
      else
      {
        am_new_near_node = FALSE;
      }
    }

    if (am_new_near_node)
    {
      printf("near %d %d, %d %d\n", am_new_near_node, am_near_node, my_last_rssi, other_last_rssi);
      call Leds.led1On(); // green on
      call Leds.led0Off(); // red off
    }
    else
    {
      printf("far  %d %d, %d %d\n", am_new_near_node, am_near_node, my_last_rssi, other_last_rssi);
      call Leds.led0On(); // red on
      call Leds.led1Off(); // green off
    }

    delta = am_new_near_node && !am_near_node;
    am_near_node = am_new_near_node;

    if (delta)
    {
      //printf("delta\n");
      if (was_from_subnet)
      {
        changeChannel(RUN_SEND_REPORT);
      }
      else
      {
        changeChannel(RUN_SEND_SUBNET_THEN_REPORT);
      }
    }
    else if (!was_from_subnet)
    {
      changeChannel(RUN_SEND_SUBNET);
    }
  } 

  event void BroadcastSend.sendDone(message_t* msg, error_t res)
  {
    if (&bcast_pkt == msg)
    {
      atomic busy = FALSE;

      if (coming_from_sync == RUN_SEND_REPORT)
      {
        atomic coming_from_sync = NONE;
      }
    }
    else printf("what1\n");
  }

  event void SubnetSend.sendDone(message_t* msg, error_t res)
  {
    if (&comm_pkt == msg)
    {
      atomic busy = FALSE;
      //printf("coming from sync %d\n", coming_from_sync);
      if (coming_from_sync == RUN_SEND_SUBNET_THEN_REPORT)
      {
        changeChannel(RUN_SEND_REPORT);
      }
      else if (coming_from_sync == RUN_SEND_SUBNET)
      {
        changeChannel(BACK_TO_WAITING);  
      }
    }
    else printf("what2\n");
  }

  event message_t* BroadcastRecv.receive(message_t* msg, void* payload, uint8_t len)
  {
    if (len == sizeof(BeaconMsg)) // we have a beacon message
    {
      BeaconMsg* bmpkt = (BeaconMsg*)payload; 
      //printf("crumpets");
      if (bmpkt->msg_type == BEACON_MSG) // make sure that this is actually a BeaconMsg
      {
        beacon_periods_passed = 0;        
        call Leds.led2On(); // we have a connection to the beacon

        if (bmpkt->subnet_id == 7)
        {
          if (am_near_node)
          {
            changeChannel(RUN_SEND_REPORT);
          }
        }
      }
    }
    else if (len == 1) // we have a target msg
    {
      TargetMsg* tmpkt = (TargetMsg*)payload; 
      if (tmpkt->msg_type == TARGET_MSG) // make sure that this is actually a TargetMsg
      {
        //printf("crackers");
        my_last_rssi = call CC2420Packet.getRssi(msg);
        handleRssiChange(FALSE);
      }
    }
    else
    {
      TargetMsg* tmpkt = (TargetMsg*)payload;
      if (tmpkt->msg_type != REPORT_MSG) printf("cheese %d %d\n", tmpkt->msg_type, len);
    }
    
    return msg;
  }
  
  event message_t* SubnetRecv.receive(message_t* msg, void* payload, uint8_t len)
  {
    if (len == sizeof(SubnetMsg))
    {
      SubnetMsg* smpkt = (SubnetMsg*)payload; 
      if (!smpkt->is_subnet_msg)
      {
        printf("ohes noze!\n");
        return msg;
      }
      //printf("subnet msg\n");
      other_last_rssi = smpkt->my_rssi;
      handleRssiChange(TRUE);
    }
    else
    {
      printf("%d %d\n", len, sizeof(SubnetMsg));
    }

    return msg;
  }
}
