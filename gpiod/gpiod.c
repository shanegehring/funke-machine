#include <stdio.h> 
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <time.h>
#include <unistd.h>

/* DACP Daemon Port */
#define DACPD_PORT 3391

/* By default, suppress anything under 10 ms */
#define DBOUNCE_THRESHOLD_NSEC 10*1000*1000

/* Button ISR callback signature */
typedef void (*button_isr)(void);

/* Single button data structure */
typedef struct {
  int id;               /* GPIO number */
  char cmd[32];         /* Associated DACP command */
  struct timespec time; /* Time of last recorded event */
  button_isr isr;       /* ISR callback */
} button_t;

/* Group of all buttons */
typedef struct {
  button_t vup;
  button_t vdown;
  button_t mute;
  button_t next;
  button_t prev;
  button_t pause;
} buttons_t;

/* Prototypes */
static void isr_vup(void);
static void isr_vdown(void);
static void isr_mute(void);
static void isr_next(void);
static void isr_prev(void);
static void isr_pause(void);
static void debounce(button_t *button);
static void send_cmd(const char* cmd);
static void init_button(const button_t* button);

/* Button configuration structure init */
static buttons_t buttons = {
  .vdown = { .id=23, .cmd="volumeup",   .isr=isr_vdown, .time={ .tv_sec=0, .tv_nsec=0 } },
  .vup   = { .id=24, .cmd="volumedown", .isr=isr_vup,   .time={ .tv_sec=0, .tv_nsec=0 } },
  .mute  = { .id=18, .cmd="mutetoggle", .isr=isr_mute,  .time={ .tv_sec=0, .tv_nsec=0 } },
  .next  = { .id=25, .cmd="nextitem",   .isr=isr_next,  .time={ .tv_sec=0, .tv_nsec=0 } },
  .prev  = { .id=7,  .cmd="previtem",   .isr=isr_prev,  .time={ .tv_sec=0, .tv_nsec=0 } },
  .pause = { .id=8,  .cmd="playpause",  .isr=isr_pause, .time={ .tv_sec=0, .tv_nsec=0 } }
};
   
/* Sends message to DACPD */
static void send_cmd(const char* cmd) {

  static int sockfd;
  if (sockfd == 0)
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (cmd == NULL)
    return;

  struct sockaddr_in si;
  memset(&si, 0, sizeof(si));
  si.sin_family = AF_INET;
  si.sin_port = htons(DACPD_PORT);
  inet_aton("127.0.0.1", &si.sin_addr);

  printf("CMD: %s\n", cmd); 
  fflush(stdout);
  sendto(sockfd, cmd, strlen(cmd)+1, 0, (struct sockaddr *)&si, sizeof(si));

}

/* Debounces button press events */
static void debounce(button_t *button) {

  struct timespec deltatime;
  struct timespec nowtime;

  /* MONTONIC has a  68 year rollover time - don't worry about it */
  clock_gettime(CLOCK_MONOTONIC, &nowtime);

  /* Calculate delta time since last press event */
  if ((nowtime.tv_nsec - button->time.tv_nsec) < 0) {
    deltatime.tv_sec  = nowtime.tv_sec  - button->time.tv_sec  - 1;
    deltatime.tv_nsec = nowtime.tv_nsec - button->time.tv_nsec + 1000000000;
  } else {
    deltatime.tv_sec  = nowtime.tv_sec  - button->time.tv_sec;
    deltatime.tv_nsec = nowtime.tv_nsec - button->time.tv_nsec;
  }

  /* Store new time stamp in button */
  button->time.tv_sec  = nowtime.tv_sec;
  button->time.tv_nsec = nowtime.tv_nsec;

  /* Filter if less than our threshold */
  if (!((deltatime.tv_sec == 0) && (deltatime.tv_nsec < DBOUNCE_THRESHOLD_NSEC))) {
    send_cmd(button->cmd);
  }
}

/* GPIO ISRs */
static void isr_vup(void)   { debounce(&buttons.vup);   }
static void isr_vdown(void) { debounce(&buttons.vdown); }
static void isr_mute(void)  { debounce(&buttons.mute);  }
static void isr_next(void)  { debounce(&buttons.next);  }
static void isr_prev(void)  { debounce(&buttons.prev);  }
static void isr_pause(void) { debounce(&buttons.pause); }

/* Registers button ISR */
static void init_button(const button_t* button) {

  /* Enable pull up resistor */
  pullUpDnControl(button->id, PUD_UP);

  /* Register ISR */
  if (wiringPiISR(button->id, INT_EDGE_FALLING, button->isr) != 0) { 
    printf("FATAL: Cannot setup ISR on button %d (%s)\n", button->id, button->cmd); 
    exit(1);
  }
}

/* Main */
int main (void)
{
  /* Use GPIO numbering scheme */
  wiringPiSetupGpio();

  /* Setup our buttons with wiringPi */
  init_button(&buttons.vup);
  init_button(&buttons.vdown);
  init_button(&buttons.mute);
  init_button(&buttons.next);
  init_button(&buttons.prev);
  init_button(&buttons.pause);

  /* Go! */
  printf("Monitoring GPIO activity...\n");
  fflush(stdout);
  while(1);

  return 0;
}
