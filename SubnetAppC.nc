#include <Timer.h>
#include "MsgDefs.h"

configuration SubnetAppC {}
implementation
{
  components MainC, LedsC;
  components SubnetC as App;

  components ActiveMessageC;
  components CC2420ActiveMessageC;

  components new AMSenderC(MY_ID) as SubnetSender;
  components new AMReceiverC(OTHER_ID) as SubnetReceiver;

  components new AMSenderC(DEFAULT_FREQ_CHANNEL) as BroadcastSender;
  components new AMReceiverC(DEFAULT_FREQ_CHANNEL) as BroadcastReceiver;

  components new TimerMilliC() as BeaconTimer;
  components LocalTimeMicroC as LTime;

  components CC2420ControlC as FullRadioControl;

  App.Boot -> MainC;
  App.Leds -> LedsC;
  App.BeaconTimer -> BeaconTimer;
  App.Time -> LTime;

  App.RadioConf -> FullRadioControl;
  App.AMControl -> ActiveMessageC;
  App.CC2420Packet -> CC2420ActiveMessageC;

  App.BroadcastAMPacket -> BroadcastSender;
  App.BroadcastPacket -> BroadcastSender;
  App.BroadcastSend -> BroadcastSender;
  App.BroadcastRecv -> BroadcastReceiver;

  App.SubnetAMPacket -> SubnetSender;
  App.SubnetPacket -> SubnetSender;
  App.SubnetSend -> SubnetSender;
  App.SubnetRecv -> SubnetReceiver;

}
