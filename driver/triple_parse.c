#include <linux/string.h>
#include <linux/module.h>

#include "triple_parse.h"

extern bool show_debug_pars;
extern bool trace_func_pars;
extern void print_func_trace (bool is_trace, int line, const char *func);

int TripleSendHex(TRIPLE_CAN_FRAME *frame)
{
  unsigned char *p;
  int length = 0;

  p = frame->comm_buf;

  memset(p, 0, sizeof(frame->comm_buf));

  /* byte 0 - FIRST BYTE */
  *p = U2C_TR_FIRST_BYTE;
  length++;
  /* byte 1 - ?? */
  *(p + length) = 1;
  length++;
  /* byte 2  - COMMAND TX to CAN*/
  length += USB2CAN_TRIPLE_PushByte(U2C_TR_CMD_TX_CAN, (p + length));

  //*(p + length) = U2C_TR_CMD_TX_CAN;
  /* byte 3- 6  //ID */
  length += USB2CAN_TRIPLE_PushByte(frame->id[0], (p + length));
  length += USB2CAN_TRIPLE_PushByte(frame->id[1], (p + length));
  length += USB2CAN_TRIPLE_PushByte(frame->id[2], (p + length));
  length += USB2CAN_TRIPLE_PushByte(frame->id[3], (p + length));
  /* byte 7 - DLC*/
  unsigned char dlc = 0;
/////////
  USB2CAN_TRIPLE_CANFD_DLCFromLength(&dlc, frame->dlc);
  if (frame->id_type)
    dlc |= 0x80;
  if (frame->fd)
  {
    dlc |= 0x20;//priznak CAN FD
    if (frame->rtr)
      dlc |= 0x40;
    if (frame->fd_br_switch)
      dlc |= 0x10;
  }
  else
  {
    if (frame->rtr)
      dlc |= 0x40;
  }

  /*byte 8 WRITE DLC */
  length += USB2CAN_TRIPLE_PushByte(dlc, (p + length));
  /*byre 9 PORT -> channel */
  length += USB2CAN_TRIPLE_PushByte(frame->CAN_port , (p + length));
  /* byte 10 - 17 DATA */
  for (int i = 0; i < frame->dlc; i++)
  {
    length += USB2CAN_TRIPLE_PushByte(frame->data[i], (p + length));
  }
  /* byte 18 -  LAST BYTE*/
  length += USB2CAN_TRIPLE_PushByteClear(U2C_TR_LAST_BYTE, (p + length));

  USB2CAN_TRIPLE_PushByte(length, (p + 1));
  return length;

}

int TripleRecvHex(TRIPLE_CAN_FRAME *frame)
{

  unsigned char *p;
  int offset = 2;

  p = frame->comm_buf;

  if (*(p + offset) == U2C_TR_CMD_STATUS)
  {
    return 1;
  }

  if (*(p + offset) == U2C_TR_CMD_FW_VER)
  {
    return 2;
  }

  //if (*(p + offset) != U2C_TR_CMD_TX_CAN)
  /* func - byte 1 */
  frame->CAN_port = *(p + offset + 6) & 0x0F;
  frame->CAN_port = frame->CAN_port - 1;
///////////
  if (*(p + offset + 5) & 0x80)
  {
    frame->id_type  =  (int) true;
  }
///////////
  //FD CAN
  if (*(p + offset + 5) & 0x20)
  {
    frame->fd = true;
    if (*(p + offset + 5) & 0x10)
      frame->fd_br_switch = true;
    if (*(p + offset + 5) & 0x40)
      frame->rtr = (int)true;
  }
  else
  {
    if (*(p + offset + 5) & 0x40)
      frame->rtr =  (int) true;

  }
  if (*(p + offset + 6) &  0x80)
    frame->fd_esi = true;
  unsigned char dlc = 0;
 
  frame->dlc = USB2CAN_TRIPLE_CANFD_LengthFromDLC((*(p + offset + 5) & 0x0F));
  /* id - byte 2 ~ byte 5 */
  memcpy(frame->id, p + offset + 1, ID_LEN);

  /* data - byte 6  */
  memcpy(frame->data, p + offset + 7, frame->dlc);

  return 0;

}

