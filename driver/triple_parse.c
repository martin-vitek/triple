#include <linux/string.h>
#include <linux/module.h>

#include "triple_parse.h"

extern bool show_debug_pars;
extern bool trace_func_pars;
extern void print_func_trace (bool is_trace, int line, const char *func);

/* static function prototype */
static unsigned char USB2CAN_TRIPLE_PushByte(const unsigned char value, unsigned char *buffer);
static bool USB2CAN_TRIPLE_DLCFromLength(unsigned char *dlc, const unsigned char length);
static unsigned char USB2CAN_TRIPLE_LengthFromDLC(const unsigned char dlc);

const unsigned char U2C_TR_FIRST_BYTE = 0x0F;
const unsigned char  U2C_TR_LAST_BYTE = 0xEF;

const unsigned char U2C_TR_SPEC_BYTE = 0x1F;

const unsigned char   U2C_TR_CMD_TX_CAN   = 0x81;
const unsigned char   U2C_TR_CMD_TX_CAN_TS  = 0x82;
const unsigned char   U2C_TR_CMD_MARKER   = 0x87;
const unsigned char   U2C_TR_CMD_SETTINGS   = 0x88;
const unsigned char   U2C_TR_CMD_BITTIMING  = 0x89;
const unsigned char   U2C_TR_CMD_STATUS       = 0x8A;
const unsigned char   U2C_TR_CMD_TIMESTAMP  = 0x8B;
const unsigned char   U2C_TR_CMD_FW_VER   = 0x90;
const unsigned char   U2C_TR_CMD_SPEED_DOWN  = 0x91;
const unsigned char   U2C_TR_CMD_SPEED_UP    = 0x92;

void TripleSendHex(TRIPLE_CAN_FRAME *frame)
{
  unsigned char *p;
  int i = 0;
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
  USB2CAN_TRIPLE_DLCFromLength(&dlc, frame->dlc);
  /* extended id , rtr FLAGY */
  if (frame->id_type)
    dlc |= 0x80;
  if (frame->rtr)
    dlc |= 0x40;
  /*byte 8 WRITE DLC */
  length += USB2CAN_TRIPLE_PushByte(dlc, (p + length));
  /*byre 9 PORT -> channel */
  length += USB2CAN_TRIPLE_PushByte(1 , (p + length));
  /* byte 10 - 17 DATA */
  for (int i = 0; i < frame->dlc; i++)
  {
    length += USB2CAN_TRIPLE_PushByte(frame->data[i], (p + length));
  }
  /* byte 18 -  LAST BYTE*/
  length += USB2CAN_TRIPLE_PushByte(U2C_TR_LAST_BYTE, (p + length));
  /* byte 18 - 22 - LENGTH*/
  USB2CAN_TRIPLE_PushByte(length, (p + 1));
  //*(p + 1) = length;


}

int TripleRecvHex(TRIPLE_CAN_FRAME *frame)
{

  int i;
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

  if (*(p + offset) != U2C_TR_CMD_TX_CAN)
    /* func - byte 1 */
    frame->CAN_port = 1;

  if (*(p + offset + 5) & 0x80)
  {
    frame->id_type  =  (int) true;
  }
  if (*(p + offset + 5) & 0x40)
  {
    frame->rtr =  (int) true;
  }

  frame->dlc = USB2CAN_TRIPLE_LengthFromDLC(*(p + offset + 5) & 0x0F);

  /* id - byte 2 ~ byte 5 */
  memcpy(frame->id, p + offset + 1, ID_LEN);

  /* data - byte 6 ~ byte 13 */
  memcpy(frame->data, p + offset + 7, DATA_LEN);

  return 0;

}

static unsigned char USB2CAN_TRIPLE_PushByte(const unsigned char value, unsigned char *buffer)
{
  if ((value == U2C_TR_FIRST_BYTE)
      || (value == U2C_TR_LAST_BYTE)
      || (value == U2C_TR_SPEC_BYTE))
  {
    buffer[0] = U2C_TR_SPEC_BYTE;
    buffer[1] = value;
    return 2;
  }
  else
  {
    buffer[0] = value;
    return 1;
  }
}

static bool USB2CAN_TRIPLE_DLCFromLength(unsigned char *dlc, const unsigned char length)
{
  //nuluji bity kde je delka
  *dlc = *dlc & 0xF0;
  switch (length)
  {
  case 0: return true;
  case 1: *dlc |= 0x01; return true;
  case 2: *dlc |= 0x02; return true;
  case 3: *dlc |= 0x03; return true;
  case 4: *dlc |= 0x04; return true;
  case 5: *dlc |= 0x05; return true;
  case 6: *dlc |= 0x06; return true;
  case 7: *dlc |= 0x07; return true;
  case 8: *dlc |= 0x08; return true;
  }
  return false;
}

static unsigned char USB2CAN_TRIPLE_LengthFromDLC(const unsigned char dlc)
{
  switch (dlc & 0x0F)
  {
  case 0x00: return 0;
  case 0x01: return 1;
  case 0x02: return 2;
  case 0x03: return 3;
  case 0x04: return 4;
  case 0x05: return 5;
  case 0x06: return 6;
  case 0x07: return 7;
  case 0x08: return 8;
  }
  return 0;
}

