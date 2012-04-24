#include <Timer.h>
#include "printf.h"
#include "LightSensorComm.h"

configuration LightSensorCommAppC {}

implementation
{
  components MainC, LedsC;
  components PrintfC, SerialStartC;
  components LightSensorCommC as App;
  components new TimerMilliC() as PollTimer;
  components new TimerMilliC() as UselessTimer;
  components ActiveMessageC;
  components new AMSenderC(AM_LIGHTSENSORCOMMIO);
  components new AMReceiverC(AM_LIGHTSENSORCOMMIO);
  components new HamamatsuS10871TsrC() as TSR;

  App.Boot -> MainC;
  App.Leds -> LedsC;
  App.SensorPollTimer -> PollTimer;
  App.LookICanUseATimer -> UselessTimer;

  App.LightSensor -> TSR;

  App.Packet -> AMSenderC;
  App.AMPacket -> AMSenderC;
  App.AMControl -> ActiveMessageC;
  App.AMSend -> AMSenderC;
  App.Receive -> AMReceiverC;
}
