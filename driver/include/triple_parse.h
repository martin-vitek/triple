#ifndef __TRIPLE_PARSE_H__
#define __TRIPLE_PARSE_H__



#define    ID_LEN           4
#define    DATA_LEN         8
#define    COM_BUF_LEN      40
#define    DATA_LEN_ERR     12
#define    TIME_CHAR_NUM    13

#define U2C_TR_FIRST_BYTE  				0x0F
#define U2C_TR_LAST_BYTE  				0xEF
#define U2C_TR_SPEC_BYTE  				0x1F

#define  U2C_TR_CMD_TX_CAN    			0x81
#define  U2C_TR_CMD_TX_CAN_TS   		0x82
#define  U2C_TR_CMD_MARKER    			0x87
#define  U2C_TR_CMD_SETTINGS    		0x88
#define  U2C_TR_CMD_BITTIMING   		0x89
#define  U2C_TR_CMD_STATUS        		0x8A
#define  U2C_TR_CMD_TIMESTAMP   		0x8B
#define  U2C_TR_CMD_FW_VER    			0x90
#define  U2C_TR_CMD_SPEED_DOWN   		0x91
#define  U2C_TR_CMD_SPEED_UP     		0x92

enum
{
	TRIPLE_SID = 0,
	TRIPLE_EID = 1
};

/*--------------------------------------*/
typedef struct
{
	int            CAN_port;
	int            id_type;
	int            rtr;
	int            dlc;

	unsigned char  id      [ID_LEN];
	unsigned char  data    [DATA_LEN];
	unsigned char  comm_buf [COM_BUF_LEN];

} TRIPLE_CAN_FRAME;


/*--------------------------------------*/
int TripleSendHex(TRIPLE_CAN_FRAME *frame);
int  TripleRecvHex (TRIPLE_CAN_FRAME *frame);

#endif