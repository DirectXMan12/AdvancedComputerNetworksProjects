// $Id: BlinkToRadio.h,v 1.4 2006-12-12 18:22:52 vlahan Exp $

#ifndef BLINKTORADIO_H
#define BLINKTORADIO_H

enum {
  AM_LIGHTSENSORCOMMIO = 6
};

typedef nx_struct LightSensorCommMsg {
  nx_uint16_t nodeid;
  nx_uint16_t intensity;
} LightSensorCommMsg;

#define MY_LED 1

#endif
