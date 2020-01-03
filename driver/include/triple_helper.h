#ifndef __TRIPLE_HELPER_H__
#define __TRIPLE_HELPER_H__

#define   TRIPLE_MTU    100 //40
#define   TRIPLE_MAGIC  0x739A//0x729B

#define    ID_LEN           4
#define    DATA_LEN         8
#define    DATA_FD_LEN      64
#define    COM_BUF_LEN      100
#define    DATA_LEN_ERR     12
#define    TIME_CHAR_NUM    13

#define U2C_TR_FIRST_BYTE         0x0F
#define U2C_TR_LAST_BYTE          0xEF
#define U2C_TR_SPEC_BYTE          0x1F

#define  U2C_TR_CMD_TX_CAN          0x81
#define  U2C_TR_CMD_TX_CAN_TS       0x82
#define  U2C_TR_CMD_MARKER          0x87
#define  U2C_TR_CMD_SETTINGS        0x88
#define  U2C_TR_CMD_BITTIMING       0x89
#define  U2C_TR_CMD_STATUS          0x8A
#define  U2C_TR_CMD_TIMESTAMP       0x8B
#define  U2C_TR_CMD_FW_VER          0x90
#define  U2C_TR_CMD_SPEED_DOWN      0x91
#define  U2C_TR_CMD_SPEED_UP        0x92

enum
{
  TRIPLE_SID = 0,
  TRIPLE_EID = 1
};

/*--------------------------------------------------------------*/
typedef struct
{
  int      magic;

  /* Various fields. */
  struct tty_struct  *tty;              /* ptr to TTY structure      */
  struct net_device  *devs[3];          /* easy for intr handling    */
  spinlock_t          lock;
  struct work_struct  tx_work;          /* Flushes transmit buffer   */

  atomic_t            ref_count;        /* reference count           */
  int                 gif_channel;      /* index for SIOCGIFNAME     */

  unsigned char       current_channel;  /* Record current channel: for fixing tx_packet bug (v2.2) */
  int                 can_fd;
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
  USB2CAN_TRIPLE  *adapter;
} TRIPLE_PRIV;

/*--------------------------------------*/
typedef struct
{
  int            CAN_port;
  int            id_type;
  int            rtr;
  int            dlc;//dlc code value (can 2.0 0-8 for CAN FD values are encoded to intervals 0-15)
  int            data_len; //can fd len

  bool            fd_br_switch;//bitrate switch
  bool            fd_esi;//error state indicator
  bool            fd; /// fdf

  unsigned char  id      [ID_LEN];
  unsigned char  data    [DATA_FD_LEN]; // up 64 bytes
  unsigned char  comm_buf [COM_BUF_LEN]; // up 100 bytes
  
} TRIPLE_CAN_FRAME;

inline static void escape_memcpy(void *dest, void *src, size_t n)
{
  // Typecast src and dest addresses to (char *)
  bool escape = false;
  int offset = 0;
  char *csrc = (char *)src;
  char *cdest = (char *)dest;

  // Copy contents of src[] to dest[]
  for (int i = 0; i < n; i++)
  {
    if (csrc[i] == U2C_TR_SPEC_BYTE && !escape)
    {
      escape = true;
      offset++;
    }
    else
    {
      cdest[i - offset] = csrc[i];
      escape = false;
    }
  }
}

inline static unsigned char USB2CAN_TRIPLE_PushByte(const unsigned char value, unsigned char *buffer)
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

inline static unsigned char USB2CAN_TRIPLE_PushByteClear(const unsigned char value, unsigned char *buffer)
{
  buffer[0] = value;
  return 1;
}
inline static bool USB2CAN_TRIPLE_DLCFromLength(unsigned char *dlc, const unsigned char length)
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

inline static unsigned char USB2CAN_TRIPLE_LengthFromDLC(const unsigned char dlc)
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

inline static bool USB2CAN_TRIPLE_CANFD_DLCFromLength(unsigned char *dlc, const unsigned char length)
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
  case 12: *dlc |= 0x09; return true;
  case 16: *dlc |= 0x0A; return true;
  case 20: *dlc |= 0x0B; return true;
  case 24: *dlc |= 0x0C; return true;
  case 32: *dlc |= 0x0D; return true;
  case 48: *dlc |= 0x0E; return true;
  case 64: *dlc |= 0x0F; return true;
  }
  return false;
}
inline static unsigned char USB2CAN_TRIPLE_CANFD_LengthFromDLC(const unsigned char dlc)
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
  case 0x09: return 12;
  case 0x0A: return 16;
  case 0x0B: return 20;
  case 0x0C: return 24;
  case 0x0D: return 32;
  case 0x0E: return 48;
  case 0x0F: return 64;
  }
  return 0;
}


#endif //__TRIPLE_HELPER_H__