#include <Timer.h>
#include "MsgDefs.h"

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
  bool send_subnet_msg = FALSE;
  uint16_t my_last_rssi = 0;
  uint16_t other_last_rssi = 0;
  message_t bcast_pkt; // for messages on the broadcast channel
  message_t comm_pkt;  // for messages on the subnet
  

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
    }
    else
    {
      call AMControl.start();
    }
  }

  event void AMControl.stopDone(error_t res) {}

  event void RadioConf.syncDone(error_t res) {}

  event void BeaconTimer.fired()
  {
    beacon_periods_passed++;

    if (beacon_periods_passed > 4)
    {
      // we have been disconnected from the base station
      call Leds.led2Off();
    }
  }

  void sendReportMsg()
  {
    if (!busy)
    {
      ReportMsg* rmpkt = (ReportMsg*)(call BroadcastPacket.getPayload(&bcast_pkt, sizeof(ReportMsg)));
      if (rmpkt == NULL) return;

      rmpkt->msg_type = REPORT_MSG;
      rmpkt->node_id = MY_ID;
      rmpkt->subnet_id = 7;
      rmpkt->report_time = call Time.get();

      if (call BroadcastSend.send(AM_BROADCAST_ADDR, &bcast_pkt, sizeof(ReportMsg)) == SUCCESS)
      {
        atomic busy = TRUE;
      }
    }
  }

  void sendSubnetMsg()
  {
    if (!busy)
    {
      SubnetMsg* smpkt = (SubnetMsg*)(call SubnetPacket.getPayload(&comm_pkt, sizeof(SubnetMsg)));
      if (smpkt == NULL) return;

      smpkt->my_rssi = my_last_rssi;

      if (call SubnetSend.send(AM_BROADCAST_ADDR, &comm_pkt, sizeof(SubnetMsg)) == SUCCESS)
      {
        atomic busy = TRUE;
      }
    }
  }

  void handleRssiChange()
  {
    bool am_new_near_node = FALSE;
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
      call Leds.led1On(); // green on
      call Leds.led0Off(); // red off
    }
    else
    {
      call Leds.led0On(); // red on
      call Leds.led1Off(); // green off
    }

    if (am_new_near_node && am_new_near_node != am_near_node)
    {
      atomic send_subnet_msg = TRUE;
      sendReportMsg();
      am_near_node = am_new_near_node;
    }
  } 

  event void BroadcastSend.sendDone(message_t* msg, error_t res)
  {
    if (&bcast_pkt == msg)
    {
      atomic busy = FALSE;
      if (send_subnet_msg == TRUE)
      {
        atomic send_subnet_msg = FALSE;
        sendSubnetMsg();
      }
    }
  }

  event void SubnetSend.sendDone(message_t* msg, error_t res)
  {
    if (&comm_pkt == msg)
    {
      atomic busy = FALSE;
    }
  }

  event message_t* BroadcastRecv.receive(message_t* msg, void* payload, uint8_t len)
  {
    if (len == sizeof(BeaconMsg)) // we have a beacon message
    {
      BeaconMsg* bmpkt = (BeaconMsg*)payload; 
      if (bmpkt->msg_type == BEACON_MSG) // make sure that this is actually a BeaconMsg
      {
        beacon_periods_passed = 0;        
        call Leds.led2On(); // we have a connection to the beacon

        if (bmpkt->subnet_id == 7)
        {
          if (am_near_node)
          {
            sendReportMsg(); 
          }
        }
      }
    }
    else if (len == sizeof(TargetMsg)) // we have a target msg
    {
      TargetMsg* tmpkt = (TargetMsg*)payload; 
      if (tmpkt->msg_type == TARGET_MSG) // make sure that this is actually a TargetMsg
      {
        my_last_rssi = call CC2420Packet.getRssi(msg);
        handleRssiChange();
      }
    }
    
    return msg;
  }
  
  event message_t* SubnetRecv.receive(message_t* msg, void* payload, uint8_t len)
  {
    if (len == sizeof(SubnetMsg))
    {
      SubnetMsg* smpkt = (SubnetMsg*)payload; 
      other_last_rssi = smpkt->my_rssi;
      handleRssiChange();
    }

    return msg;
  }
}
