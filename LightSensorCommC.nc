#include <Timer.h>
#include "printf.h"
#include "LightSensorComm.h"

module LightSensorCommC
{
  uses interface Boot;
  uses interface Leds;
  uses interface Timer<TMilli> as SensorPollTimer;

  uses interface Read<uint16_t> as LightSensor;

  uses interface Packet;
  uses interface AMPacket;
  uses interface AMSend;
  uses interface Receive;
  uses interface SplitControl as AMControl;
}
implementation {

  message_t pkt;
  bool busy = FALSE;

  bool my_on = FALSE;
  bool other_on = FALSE;

  event void Boot.booted()
  {
    call AMControl.start();
  }

  event void AMControl.startDone(error_t res)
  {
    if (res == SUCCESS)
    {
      call SensorPollTimer.startPeriodic(1000 + 1000*MY_LED);
    }
    else
    {
      call AMControl.start();
    }
  }

  event void AMControl.stopDone(error_t err) { }

  event void SensorPollTimer.fired()
  {
    if (!busy)
    {
      call LightSensor.read();
    }
  }

  void setLeds()
  {
    uint16_t leds = 0;
    if (my_on)
    {
      leds |= 1 << MY_LED; // led 0 or led 1, depending

      if (other_on)
      {
        leds |= 1 << 2; // led 2
      }
    }

    call Leds.set(leds);
  }

  event void LightSensor.readDone(error_t res, uint16_t val)
  {
    if (res == SUCCESS)
    {
      bool isLight = (val > 40);
      if (my_on != isLight) // only need to do stuff on a delta
      {
        LightSensorCommMsg* lscpkt = (LightSensorCommMsg*)(call Packet.getPayload(&pkt, sizeof(LightSensorCommMsg)));
        if (lscpkt == NULL) return;

        lscpkt->nodeid = TOS_NODE_ID;
        lscpkt->intensity = val;

        if (call AMSend.send(AM_BROADCAST_ADDR, &pkt, sizeof(LightSensorCommMsg)) == SUCCESS)
        {
          busy = TRUE;
        }

        my_on = isLight;

        setLeds();
      }
    }
  }

  event void AMSend.sendDone(message_t* msg, error_t err)
  {
    if (&pkt == msg)
    {
      busy = FALSE;
    }
  }

  event message_t* Receive.receive(message_t* msg, void* payload, uint8_t len)
  {
    if (len == sizeof(LightSensorCommMsg))
    {
      LightSensorCommMsg* lscpkt = (LightSensorCommMsg*)payload;
      printf("received %u\n", lscpkt->intensity);

      if (lscpkt->intensity < 40)
      {
        other_on = FALSE;
      }
      else
      {
        other_on = TRUE;
      }

      setLeds();
    }
    return msg;
  }
}
