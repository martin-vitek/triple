#ifndef __TRIPLE_PARSE_H__
#define __TRIPLE_PARSE_H__



#define    ID_LEN           4
#define    DATA_LEN         8
#define    COM_BUF_LEN      40
#define    DATA_LEN_ERR     12
#define    TIME_CHAR_NUM    13


enum
{
	TRIPLE_SID = 1,
	TRIPLE_EID
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
void TripleSendHex(TRIPLE_CAN_FRAME *frame);
int  TripleRecvHex (TRIPLE_CAN_FRAME *frame);

#endif