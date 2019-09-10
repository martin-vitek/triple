#ifndef __TRANSCEIVE_H__
#define __TRANSCEIVE_H__


#include <linux/can.h>
#include <linux/workqueue.h>
#include <linux/netdevice.h>


#include "triple_parse.h"


/* maximum rx buffer len: exstended CAN frame with timestamp */
#define   TRIPLE_MTU    40 //35
#define   TRIPLE_MAGIC  0x739A//0x729B

/*--------------------------------------------------------------*/
typedef struct
{
  int      magic;

  /* Various fields. */
  struct tty_struct  *tty;              /* ptr to TTY structure      */
  struct net_device  *devs;          /* easy for intr handling    */
  spinlock_t          lock;
  struct work_struct  tx_work;          /* Flushes transmit buffer   */

  atomic_t            ref_count;        /* reference count           */
  int                 gif_channel;      /* index for SIOCGIFNAME     */

  unsigned char       current_channel;  /* Record current channel: for fixing tx_packet bug (v2.2) */

  /* These are pointers to the malloc()ed frame buffers. */
  unsigned char       rbuff[TRIPLE_MTU];  /* receiver buffer           */
  int                 rcount;           /* received chars counter    */
  unsigned char       xbuff[TRIPLE_MTU];  /* transmitter buffer        */
  unsigned char      *xhead;            /* pointer to next XMIT byte */
  int                 xleft;            /* bytes left in XMIT queue  */
  unsigned long       flags;            /* Flag values/ mode etc     */

#define  SLF_INUSE  0                 /* Channel in use            */
#define  SLF_ERROR  1                 /* Parity, etc. error        */

} USB2CAN_TRIPLE;

/*--------------------------------------------------------------*/
typedef struct
{
  int             magic;
  USB2CAN_TRIPLE  *adapter;    /* just ptr to emuc_info */
} TRIPLE_PRIV;

/*--------------------------------------------------------------*/
void triple_unesc   (USB2CAN_TRIPLE *adapter, unsigned char s);
void triple_bump    (USB2CAN_TRIPLE *adapter);
void triple_encaps  (USB2CAN_TRIPLE *adapter, struct can_frame *cf);
void triple_transmit(struct work_struct *work);



#endif