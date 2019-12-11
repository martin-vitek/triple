#ifndef __TRANSCEIVE_H__
#define __TRANSCEIVE_H__


#include <linux/can.h>
#include <linux/workqueue.h>
#include <linux/netdevice.h>

#include "triple_helper.h"
#include "triple_parse.h"

void triple_unesc   (USB2CAN_TRIPLE *adapter, unsigned char s);
void triple_bump    (USB2CAN_TRIPLE *adapter);
void triple_encaps  (USB2CAN_TRIPLE *adapter, int channel, struct can_frame *cf);
void triple_encaps_fd  (USB2CAN_TRIPLE *adapter, int channel, struct canfd_frame *cf);
void triple_transmit(struct work_struct *work);

#endif