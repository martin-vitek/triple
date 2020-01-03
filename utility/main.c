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
 * Send feedback to <koupy@canlab.cz>
 */

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
#include "tripled_helper.h"
/*
 * Before 3.1.0, the ldisc number is private define
 * in kernel, userspace application cannot use it.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
#define N_TRIPLE (NR_LDISCS - 1)
#endif

#define   DAEMON_NAME      "tripled"
#define   TTYPATH_LENGTH   64

static int  tripled_running;
static int  exit_code;
static char ttypath [TTYPATH_LENGTH];

/* static function prototype */
static void print_version (char *prg);
static void print_usage (char *prg);
static void child_handler (int signum);
static const char *look_up_can_speed (int speed);
static void run_interactive ();
static void print_bittiming();
static void parse_bittiming();
static void print_speed();


/* v2.1: change some variable to global (for end process) */
int             port;
int             ldisc;
int             fd;
speed_t         old_ispeed;
speed_t         old_ospeed;
struct termios  tios;

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
  while ((opt = getopt(argc, argv, "s:n:l:iduvwh?fc:")) != -1)
  {
    switch (opt)
    {
    case 's'://set speed
      speed = atoi(optarg);
      break;
    case 'n'://set names
      break;
    case 'l'://set listen-only
      break;
    case 'i': //interactive mode
      break;
    case 'd'://run a deamon
      run_as_daemon = 0;
      break;
    case 'v':// print version
      if (argc == 3)
      {
        print_version(argv[2]);
      }
    case 'w':// print FW version
      break;
    case 'f':// CAN FD on
      break;
    case 'c':// User defined CAND FD bittiming
      parse_bittiming();
      break;
    case 'u':
      print_bittiming();
      break;
    case 't':// print Speeds
      print_speed();
      break;
    case 'h'://help
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
  USB2CAN_TRIPLE_SendFDCANSpeed(2502000, false, false, false, fd);
  sleep(1);
  USB2CAN_TRIPLE_GetFWVersion(fd);
  sleep(2);

  if (ioctl(fd, TIOCSETD, &ldisc) < 0)
  {
    perror("ioctl TIOCSETD");
    exit(EXIT_FAILURE);
  }
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
} /* END: main() */

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

}

static void run_interactive ()
{
  int c;

  printf("Should this deamon run in foreground ? (y/n)");
  c = getchar();
  if (c != 0 && c == 'y')
  {
    printf("YOU PUT IN ");
    putchar(c);
  }
}

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
/* END: print_version() */
/*------------------------------------------------------------------------------------*/
static void parse_bittiming()
{
 //TODO
}
static void print_bittiming()
{
  fprintf(stderr, "For detailed information see MCP2517FD datasheet\n");
  fprintf(stderr, "<bittiming options> --> ./tripled_64 -c[NBRP]:[NTSEG1]:[NTSEG2]:[NSJW]:[DBRP]:[DTSEG1]:[DTSEG2]:[DSJW]:[TDCO]:[TDCV]:[TDCMOD]\n");
  fprintf(stderr, "Nominal bittiming\n");
  fprintf(stderr, "NBRP - Nominal Baud Rate Prescaler\n");
  fprintf(stderr, "NTSEG1 - Time Segment 1\n");
  fprintf(stderr, "NTSEG2 - Time Segment 2\n");
  fprintf(stderr, "NSJW - Synchronization Jump Width \n");
  fprintf(stderr, "Data bittiming\n");
  fprintf(stderr, "DBRP - Data Baud Rate Prescaler\n");
  fprintf(stderr, "DTSEG1 - Time Segment 1\n");
  fprintf(stderr, "DTSEG2 - Time Segment 2\n");
  fprintf(stderr, "DSJW - Synchronzation Jump Width\n");
  fprintf(stderr, "TDCO- Transmitter Delay Compensation Offset\n");
  fprintf(stderr, "TDCV - Transmitter Delay Compensation Value\n");
  fprintf(stderr, "TDCMOD - Transmitter Delay Compensation Mode\n");
  fprintf(stderr, "TDCMOD options: 0 -> off, 1 -> manual, 2 -> auto\n");
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}
static void print_speed()
{
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
  fprintf(stderr, "         -d                          (stay in foreground; no daemonize)\n");
  fprintf(stderr, "         -h                          (show this help page)\n");
  fprintf(stderr, "         -v                          (show version info)\n");
  fprintf(stderr, "         -t                          (show supported CAN 2.0 and CAN FD speeds)\n");
  fprintf(stderr, "         -u                          (show bittimng options and format)\n");
  fprintf(stderr, "         -i                          (interactive mode))\n");
  fprintf(stderr, "         -s[port1]:[port2]:[port3]   (set speed: see CAN, see ./tripled_64 -t for available speeds)\n");
  fprintf(stderr, "         -n[port1]:[port2]:[port3]   (set name of interface)\n");
  fprintf(stderr, "         -l[1/0]:[1/0]:[1/0]         (listen-only mode )\n");
  fprintf(stderr, "         -f                          (CAN FD on port 3)\n");
  fprintf(stderr, "         -c[bittiming options]       (User defined CAN FD bittiming, see ./tripled_64 -u)\n");
  fprintf(stderr, "\nExamples:\n");
  fprintf(stderr, "Deamon can be used in interactive mode or you can use parametrs\n");
  fprintf(stderr, "Interactive mode : ./tripled_64 -i\n\n");
  fprintf(stderr, "tripled_64 -s1:2:3 /dev/ttyACM0\n");
  fprintf(stderr, "tripled_64 /dev/ttyACM0\n");
  fprintf(stderr, "tripled_64 dev/ttyACM0 -ncan0:can1:can2\n");
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);

} /* END: print_usage() */