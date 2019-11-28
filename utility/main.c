/*
 * tripled.c - userspace daemon CAN interface driver
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Send feedback to <linux-can@vger.kernel.org>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <termios.h>
#include <linux/tty.h>
#include <linux/sockios.h>
#include <linux/version.h>

#include "version.h"

/*====================================================================================*/

/*
 * Beform 3.1.0, the ldisc number is private define
 * in kernel, usrspace application cannot use it.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
#define N_TRIPLE (NR_LDISCS - 1)
#endif

#define   DAEMON_NAME      "tripled"
#define   TTYPATH_LENGTH   64

enum CAN_SPEED
{
  SPEED_10k = 10,
  SPEED_20k = 20,
  SPEED_33_3k = 33,
  SPEED_50k = 50,
  SPEED_62_5k = 62,
  SPEED_83_3k = 83,
  SPEED_100k  = 100,
  SPEED_125k  = 125,
  SPEED_250k  = 250,
  SPEED_500k  = 500,
  SPEED_1M  = 1000,
  SPEED_USR = 0,
};

enum CAN_FD_SPEED
{
  CAN_USR_SPEED = 0,
  CAN_125K_250K = 125250,
  CAN_125K_500K = 125500,
  CAN_125K_833K = 125833,
  CAN_125K_1M   = 1251000,
  CAN_125K_1M5  = 1251500,
  CAN_125K_2M   = 1252000,
  CAN_125K_3M   = 1253000,
  CAN_125K_4M   = 1254000,
  CAN_125K_5M   = 1255000,
  CAN_125K_6M7  = 1256700,
  CAN_125K_8M   = 1258000,
  CAN_125K_10M  = 1259999,

  CAN_250K_500K = 250500,
  CAN_250K_833K = 250833,
  CAN_250K_1M   = 2501000,
  CAN_250K_1M5  = 2501500,
  CAN_250K_2M   = 2502000,
  CAN_250K_3M   = 2503000,
  CAN_250K_4M   = 2504000,
  CAN_250K_5M   = 2505000,
  CAN_250K_6M7  = 2506700,
  CAN_250K_8M   = 2508000,
  CAN_250K_10M  = 2509999,

  CAN_500K_833K = 500833,
  CAN_500K_1M   = 5001000,
  CAN_500K_1M5  = 5001500,
  CAN_500K_2M   = 5002000,
  CAN_500K_3M   = 5003000,
  CAN_500K_4M   = 5004000,
  CAN_500K_5M   = 5005000,
  CAN_500K_6M7  = 5006700,
  CAN_500K_8M   = 5008000,
  CAN_500K_10M  = 5009999,

  CAN_1000K_1M5 = 10001500,
  CAN_1000K_2M  = 10002000,
  CAN_1000K_3M  = 10003000,
  CAN_1000K_4M  = 10004000,
  CAN_1000K_5M  = 10005000,
  CAN_1000K_6M7 = 10006700,
  CAN_1000K_8M  = 10008000,
  CAN_1000K_10M = 10009999,
};

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

bool USB2CAN_TRIPLE_GetFWVersion(int fd);
bool USB2CAN_TRIPLE_SendCANSpeed(unsigned int port, int speed, bool listen_only, int fd);
bool USB2CAN_TRIPLE_SendFDCANSpeed(int speed, bool listen_only, bool esi, bool iso_crc, int fd);
void USB2CAN_TRIPLE_SendTimeStampMode(bool mode, int fd);
void USB2CAN_TRIPLE_Init(int speed, int comport);

static int  tripled_running;
static int  exit_code;
static char ttypath [TTYPATH_LENGTH];

/* static function prototype */
static void print_version (char *prg);
static void print_usage (char *prg);
static void child_handler (int signum);
static const char *look_up_can_speed (int speed);

/* v2.1: change some variable to global (for end process) */
int             port;
int             ldisc;
int             fd;
speed_t         old_ispeed;
speed_t         old_ospeed;
struct termios  tios;

/*----------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
  int             sp;
  int             opt;
  int             channel;
  int             run_as_daemon = 1;
  char           *pch;
  char           *tty = NULL;
  char           *name[3];
  int           speed = 250;
  char            buf[IFNAMSIZ + 1];
  char const     *devprefix = "/dev/";


  ldisc = N_TRIPLE;


  name[0] = NULL;
  name[1] = NULL;
  name[2] = NULL;
  ttypath[0] = '\0';
  while ((opt = getopt(argc, argv, "s:n:r:dvwh?fc:")) != -1)
  {
    switch (opt)
    {
    case 's'://set speed
      speed = atoi(optarg);
      break;
    case 'F':
      run_as_daemon = 0;
      break;
    case 'v':
      if (argc == 3)
      {
        print_version(argv[2]);
      }

    case 'h':
    case 'l': //listen only

      break;
    case '?':
    default:
      print_usage(argv[0]);
      break;
    }
  }

  /* Initialize the logging interface */
  openlog(DAEMON_NAME, LOG_PID, LOG_LOCAL5);

  /* Parse serial device name and optional can interface name */
  tty = argv[optind];
  if (NULL == tty)
    print_usage(argv[0]);

  name[0] = argv[optind + 1];
  if (name[0])
    name[1] = argv[optind + 2];
  if (name[1])
    name[2] = argv[optind + 3];

  /* Prepare the tty device name string */
  pch = strstr(tty, devprefix);
  if (pch != tty)
    snprintf(ttypath, TTYPATH_LENGTH, "%s%s", devprefix, tty);
  else
    snprintf(ttypath, TTYPATH_LENGTH, "%s", tty);

  syslog(LOG_INFO, "starting on TTY device %s", ttypath);

  fd = open(ttypath, O_RDWR | O_NONBLOCK | O_NOCTTY);

  if (fd < 0)
  {
    syslog(LOG_NOTICE, "failed to open TTY device %s\n", ttypath);
    perror(ttypath);
    exit(EXIT_FAILURE);
  }
  /****************************************************************************************************/
  /* Configure baud rate */
  memset(&tios, 0, sizeof(struct termios));
  if (tcgetattr(fd, &tios) < 0)
  {
    syslog(LOG_NOTICE, "failed to get attributes for TTY device %s: %s\n", ttypath, strerror(errno));
    exit(EXIT_FAILURE);
  }

  /* Get old values for later restore */
  old_ispeed = cfgetispeed(&tios);
  old_ospeed = cfgetospeed(&tios);

  /* Reset UART settings */
  cfmakeraw(&tios);

  //tios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXOFF);
  //tios.c_oflag &= ~(OPOST);
  //tios.c_cflag |= (CS8);
  //tios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  /* Reset UART settings */
  tios.c_iflag &= ~IXOFF;
  tios.c_cflag &= ~CRTSCTS;

  /* Baud Rate */
  cfsetispeed(&tios, B115200);
  cfsetospeed(&tios, B115200);

  /* apply changes */
  if (tcsetattr(fd, TCSADRAIN, &tios) < 0)
    syslog(LOG_NOTICE, "Cannot set attributes for device \"%s\": %s!\n", ttypath, strerror(errno));

  /*Set speed on can port, timestamp mode and get FW version*/
  //USB2CAN_TRIPLE_Init(speed, fd);

  USB2CAN_TRIPLE_SendTimeStampMode(false, fd);
  sleep(1);
  USB2CAN_TRIPLE_SendCANSpeed(1, speed, false, fd);
  sleep(1);
  USB2CAN_TRIPLE_SendCANSpeed(2, speed, false, fd);
  sleep(1);
  USB2CAN_TRIPLE_SendFDCANSpeed(250500, false, false, false, fd);
  sleep(1);
  USB2CAN_TRIPLE_GetFWVersion(fd);
  sleep(2);

  if (ioctl(fd, TIOCSETD, &ldisc) < 0)
  {
    perror("ioctl TIOCSETD");
    exit(EXIT_FAILURE);
  }
  /****************************************************************************************************/
  /************* try to rename the created netdevice **************************************************/
  for (channel = 0; channel < 3; channel++)
  {

    if (ioctl(fd, SIOCGIFNAME, buf) < 0)
    {
      perror("ioctl SIOCGIFNAME");
      exit(EXIT_FAILURE);
    }

    //rename of interfaces
    if (name[channel])
    {
      struct ifreq ifr;
      int s = socket(PF_INET, SOCK_DGRAM, 0);

      if (s < 0)
      {
        perror("socket for interface rename");
      }
      else
      {
        strncpy(ifr.ifr_name, buf, IFNAMSIZ);
        strncpy(ifr.ifr_newname, name[channel], IFNAMSIZ);
      }
      if (ioctl(s, SIOCSIFNAME, &ifr) < 0)
      {
        perror("ioctl SIOCSIFNAME rename");
        exit(EXIT_FAILURE);
      }
      else
      {
        syslog(LOG_NOTICE, "netdevice %s renamed to %s\n", buf, name[channel]);
      }

      close(s);
    }
  }
  /* Daemonize */
  if (run_as_daemon)
  {
    if (daemon(0, 0))
    {
      syslog(LOG_ERR, "failed to daemonize");
      exit(EXIT_FAILURE);
    }
  }
  /* Trap signals that we expect to receive */
  /* End process */
  signal(SIGINT, child_handler);
  signal(SIGTERM, child_handler);

  tripled_running = 1;
  /* The Big Loop */
  while (tripled_running)
  {
    sleep(1); /* wait 1 second */
  }
  /* Reset line discipline */
  syslog(LOG_INFO, "stopping on TTY device %s", ttypath);
  ldisc = N_TTY;
  if (ioctl(fd, TIOCSETD, &ldisc) < 0)
  {
    perror("ioctl TIOCSETD");
    exit(EXIT_FAILURE);
  }

  /* Reset old rates */
  cfsetispeed(&tios, old_ispeed);
  cfsetospeed(&tios, old_ospeed);

  /* apply changes */
  if (tcsetattr(fd, TCSADRAIN, &tios) < 0)
    syslog(LOG_NOTICE, "Cannot set attributes for device \"%s\": %s!\n", ttypath, strerror(errno));

  /* Finish up */
  syslog(LOG_NOTICE, "terminated on %s", ttypath);
  closelog();

  return exit_code;
  /*--------------------------------------------------------------*/
} /* END: main() */
/*----------------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------------*/
static void print_version (char *prg)
{
  int          com_port;
  char        *pch;
  char const  *devprefix = "/dev/";

  pch = strstr(prg, devprefix);
  if (pch != prg)
  {
    snprintf(ttypath, TTYPATH_LENGTH, "%s%s", devprefix, prg);
  }
  else
  {
    snprintf(ttypath, TTYPATH_LENGTH, "%s", prg);
  }
  exit(EXIT_SUCCESS);

} /* END: print_version() */
/*------------------------------------------------------------------------------------*/
static void print_speed (char *prg)
{
  fprintf(stderr, "\nUsage: %s \n", prg);
  fprintf(stderr, "--------------------CAN 2.0--------------------\n");
  fprintf(stderr, "               1 -> 10: 10 KBPS\n");
  fprintf(stderr, "               2 -> 20: 20 KBPS\n");
  fprintf(stderr, "               3 -> 33: 33 KBPS\n");
  fprintf(stderr, "               5 -> 50 : 50 KBPS\n");
  fprintf(stderr, "               6 -> 62: 62 KBPS\n");
  fprintf(stderr, "               7 -> 83: 83 KBPS\n");
  fprintf(stderr, "               8 -> 100: 100 KBPS\n");
  fprintf(stderr, "               9 -> 125: 125 KBPS\n");
  fprintf(stderr, "               A -> 250: 250 KBPS\n");
  fprintf(stderr, "               B -> 500: 500 KBPS\n");
  fprintf(stderr, "               C -> 1000: 1 MBPS\n");
  fprintf(stderr, "-----------------------------------------------\n");

  fprintf(stderr, "-----------------CAN FD spped------------------\n");
  fprintf(stderr, "               0  -> CAN_USR_SPEED = 0,\n");
  fprintf(stderr, "               1  -> CAN_125K_250K = 125250\n");
  fprintf(stderr, "               2  -> CAN_125K_500K = 125500\n");
  fprintf(stderr, "               3  -> CAN_125K_833K = 125833\n");
  fprintf(stderr, "               4  -> CAN_125K_1M   = 1251000\n");
  fprintf(stderr, "               5  -> CAN_125K_1M5  = 1251500\n");
  fprintf(stderr, "               6  -> CAN_125K_2M   = 1252000\n");
  fprintf(stderr, "               7  -> CAN_125K_3M   = 1253000\n");
  fprintf(stderr, "               8  -> CAN_125K_4M   = 1254000\n");
  fprintf(stderr, "               9  -> CAN_125K_5M   = 1255000\n");
  fprintf(stderr, "               A  -> CAN_125K_6M7  = 1256700\n");
  fprintf(stderr, "               B  -> CAN_125K_8M   = 1258000\n");
  fprintf(stderr, "               C  -> CAN_125K_10M  = 1259999\n");
  fprintf(stderr, "               D  -> CAN_250K_500K = 250500\n");
  fprintf(stderr, "               E  -> CAN_250K_833K = 250833\n");
  fprintf(stderr, "               F  -> CAN_250K_1M   = 2501000\n");
  fprintf(stderr, "              10  -> CAN_250K_1M5  = 2501500\n");
  fprintf(stderr, "              11  -> CAN_250K_2M   = 2502000\n");
  fprintf(stderr, "              12  -> CAN_250K_3M   = 2503000\n");
  fprintf(stderr, "              13  -> CAN_250K_4M   = 2504000\n");
  fprintf(stderr, "              14  -> CAN_250K_5M   = 2505000\n");
  fprintf(stderr, "              15  -> CAN_250K_6M7  = 2506700\n");
  fprintf(stderr, "              16  -> CAN_250K_8M   = 2508000\n");
  fprintf(stderr, "              17  -> CAN_250K_10M  = 2509999\n");
  fprintf(stderr, "              18  -> CAN_500K_833K = 500833\n");
  fprintf(stderr, "              19  -> CAN_500K_1M   = 5001000\n");
  fprintf(stderr, "              1A  -> CAN_500K_1M5  = 5001500\n");
  fprintf(stderr, "              1B  -> CAN_500K_2M   = 5002000\n");
  fprintf(stderr, "              1C  -> CAN_500K_3M   = 5003000\n");
  fprintf(stderr, "              1D  -> CAN_500K_4M   = 5004000\n");
  fprintf(stderr, "              1E  -> CAN_500K_5M   = 5005000\n");
  fprintf(stderr, "              1F  -> CAN_500K_6M7  = 5006700\n");
  fprintf(stderr, "              20  -> CAN_500K_8M   = 5008000\n");
  fprintf(stderr, "              21  -> CAN_500K_10M  = 5009999\n");
  fprintf(stderr, "              22  -> CAN_1000K_1M5 = 10001500\n");
  fprintf(stderr, "              23  -> CAN_1000K_2M  = 10002000\n");
  fprintf(stderr, "              24  -> CAN_1000K_3M  = 10003000\n");
  fprintf(stderr, "              25  -> CAN_1000K_4M  = 10004000\n");
  fprintf(stderr, "              26  -> CAN_1000K_5M  = 10005000\n");
  fprintf(stderr, "              27  -> CAN_1000K_6M7 = 10006700\n");
  fprintf(stderr, "              28  -> CAN_1000K_8M  = 10008000\n");
  fprintf(stderr, "              29  -> CAN_1000K_10M = 10009999\n");
  fprintf(stderr, "-----------------------------------------------\n");
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);

} /* END: print_usage() */

/*------------------------------------------------------------------------------------*/
static void print_usage (char *prg)
{
  fprintf(stderr, "\nUsage: %s [options] <tty>\\n\n", prg);
  fprintf(stderr, "         -d         (stay in foreground; no daemonize)\n");
  fprintf(stderr, "         -h         (show this help page)\n");
  fprintf(stderr, "         -v         (show version info)\n");
  fprintf(stderr, "         -w         (show FW version info <requires connected device>)\n");
  fprintf(stderr, "         -s         (show version info)\n");
  fprintf(stderr, "         -n[name]:[name]:[name]         (show version info)\n");
  fprintf(stderr, "         -r[t/f]:[t/f]:[t/f] read-only mode (t-true, f-false)\n");
  fprintf(stderr, "         -f         (CAN FD on port 3)\n");
  fprintf(stderr, "         -c         (User defined CAN FD bittiming)\n");
  fprintf(stderr, "\nExamples:\n");
  fprintf(stderr, "Deamon can be used in interactive mode or you can use parametrs\n");
  fprintf(stderr, "Interactive mode : ./tripled_64 -i\n\n");
  fprintf(stderr, "tripled_64 -s[port1]:[port2]:[port3] /dev/ttyACM0\n");
  fprintf(stderr, "tripled_64 -s /dev/ttyACM0\n");
  fprintf(stderr, "tripled_64 -s250 /dev/ttyACM0 can0 \n");
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);

} /* END: print_usage() */
/*------------------------------------------------------------------------------------*/
static void run_interactive ()
{


}

/*------------------------------------------------------------------------------------*/
static void child_handler (int signum)
{
  switch (signum)
  {
  case SIGUSR1:
    /* exit parent */
    exit(EXIT_SUCCESS);
    break;
  case SIGALRM:
  case SIGCHLD:
    syslog(LOG_NOTICE, "received signal %i on %s", signum, ttypath);
    exit_code = EXIT_FAILURE;
    tripled_running = 0;
    break;
  case SIGINT:
  case SIGTERM:
    syslog(LOG_NOTICE, "received signal %i on %s", signum, ttypath);
    exit_code = EXIT_SUCCESS;
    tripled_running = 0;
    break;
  }

} /* END: child_handler() */



/*------------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------------*/
static const char *look_up_can_speed (int speed)
{
  switch (speed)
  {

  case 10:   return "10 KBPS";
  case 20:   return "20 KBPS";
  case 33:   return "33 KBPS";
  case 50:   return "50 KBPS";
  case 62:   return "62 KBPS";
  case 83:   return "83 KBPS";
  case 100:   return "100 KBPS";
  case 125:   return "125 KBPS";
  case 250:   return "250 KBPS";
  case 500:   return "500 KBPS";
  case 1000:   return "1   MBPS";

  default:  return "unknown";
  }

} /* END: look_up_can_speed() */

/*|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||*/
/*|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||*/
unsigned char USB2CAN_TRIPLE_PushByte(const unsigned char value, unsigned char *buffer)
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

bool USB2CAN_TRIPLE_GetFWVersion(int fd)
{
  unsigned char buffer[16];
  int i = 0;
  buffer[0] = U2C_TR_FIRST_BYTE;
  buffer[1] = 4;
  USB2CAN_TRIPLE_PushByte(U2C_TR_CMD_FW_VER, &buffer[2]);
  buffer[3] = U2C_TR_LAST_BYTE;

// printf("USB2CAN_TRIPLE_GetFWVersion\n");

  if (write(fd, buffer, 4) <= 0)
  {
    perror("write");
    exit(EXIT_FAILURE);
  }
  return true;
}

bool USB2CAN_TRIPLE_SendCANSpeed(unsigned int port, int speed, bool listen_only, int fd)
{
  unsigned char buffer[16];
  unsigned char length = 3;
  int i = 0;
  buffer[0] = U2C_TR_FIRST_BYTE;
  buffer[1] = 1;
  buffer[2] = U2C_TR_CMD_SETTINGS;
  length += USB2CAN_TRIPLE_PushByte(port, &buffer[length]);
  u_int16_t s = speed;
  length += USB2CAN_TRIPLE_PushByte(s >> 8, &buffer[length]);
  length += USB2CAN_TRIPLE_PushByte(s >> 0, &buffer[length]);
  length += USB2CAN_TRIPLE_PushByte(listen_only ? 1 : 0, &buffer[length]);
  buffer[length] = U2C_TR_LAST_BYTE;
  length++;
  buffer[1] = length;

  //printf("USB2CAN_TRIPLE_SendCANSpeed\n");

  if (write(fd, buffer, length) <= 0)
  {
    perror("write");
    exit(EXIT_FAILURE);
  }
  return true;
}

bool USB2CAN_TRIPLE_SendFDCANSpeed(int speed, bool listen_only, bool esi, bool iso_crc, int fd)
{
  unsigned char buffer[16];
  unsigned char length = 3;
  int i = 0;
  buffer[0] = U2C_TR_FIRST_BYTE;
  buffer[1] = 1;
  buffer[2] = U2C_TR_CMD_SETTINGS;
  length += USB2CAN_TRIPLE_PushByte(3, &buffer[length]);
  u_int32_t s = (u_int32_t)speed;
  length += USB2CAN_TRIPLE_PushByte(s >> 24, &buffer[length]);
  length += USB2CAN_TRIPLE_PushByte(s >> 16, &buffer[length]);
  length += USB2CAN_TRIPLE_PushByte(s >> 8, &buffer[length]);
  length += USB2CAN_TRIPLE_PushByte(s >> 0, &buffer[length]);
  length += USB2CAN_TRIPLE_PushByte(listen_only ? 1 : 0, &buffer[length]);
  length += USB2CAN_TRIPLE_PushByte(iso_crc ? 1 : 0, &buffer[length]);
  length += USB2CAN_TRIPLE_PushByte(esi ? 1 : 0, &buffer[length]);
  buffer[length] = U2C_TR_LAST_BYTE;
  length++;
  buffer[1] = length;

  //printf("USB2CAN_TRIPLE_SendFDCANSpeed\n");
  if (write(fd, buffer, length) <= 0)
  {
    perror("write");
    exit(EXIT_FAILURE);
  }
  return true;
}

void USB2CAN_TRIPLE_SendTimeStampMode(bool mode, int fd)
{
  unsigned char buffer[16];
  unsigned char l = 3;
  int i = 0;
  buffer[0] = U2C_TR_FIRST_BYTE;
  buffer[2] = U2C_TR_CMD_TIMESTAMP;
  l += USB2CAN_TRIPLE_PushByte((unsigned char)mode, &buffer[l]);
  buffer[l] = U2C_TR_LAST_BYTE;
  buffer[1] = l + 1;

// printf("USB2CAN_TRIPLE_SendTimeStampMode\n");

  if (write(fd, buffer, l + 1) <= 0)
  {
    perror("write");
    exit(EXIT_FAILURE);
  }
  return;
}

int KBaud2Int(int speed)
{
  switch (speed)
  {
  case 10:   return 0;
  case 20:   return 1;
  case 33:   return 2;
  case 50:   return 3;
  case 62:   return 4;
  case 83:   return 5;
  case 100:  return 6;
  case 125:  return 7;
  case 250:  return 8;
  case 500:  return 9;
  case 1000: return 10;
  }
  return -1;
}
