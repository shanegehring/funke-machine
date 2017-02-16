/*
 * GPIO monitor. This file is part of Funke Machine.
 * Copyright (c) Shane Gehring 2017
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h> 
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

/* DACP Daemon Port */
#define DACPD_PORT 3391

/* By default, suppress anything under X ms */
#define DEBOUNCE_NSEC (250*1000*1000)

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
  button_t *vup;
  button_t *vdown;
  button_t *mute;
  button_t *next;
  button_t *prev;
  button_t *pause;
} buttons_t;

/* Buttons */
static buttons_t buttons;

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

  printf("%s\n", cmd); 
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
  if (!((deltatime.tv_sec == 0) && (deltatime.tv_nsec < DEBOUNCE_NSEC))) {
    send_cmd(button->cmd);
  }
}

/* GPIO ISRs */
static void isr_vup(void)   { debounce(buttons.vup);   }
static void isr_vdown(void) { debounce(buttons.vdown); }
static void isr_mute(void)  { debounce(buttons.mute);  }
static void isr_next(void)  { debounce(buttons.next);  }
static void isr_prev(void)  { debounce(buttons.prev);  }
static void isr_pause(void) { debounce(buttons.pause); }

/* Create a new button */
static button_t* button_new(int id, const char* cmd, button_isr isr) {

  /* Allocate new button */
  button_t *button = (button_t *)malloc(sizeof(button_t));
  if (button == NULL) {
    printf("FATAL: Cannot allocate button struct\n");
    exit(1);
  }

  /* Init data */
  button->id = id;
  button->isr = isr;
  button->time.tv_sec = 0;
  button->time.tv_nsec = 0;
  strncpy(button->cmd, cmd, 32);

  /* Enable pull up resistor */
  pullUpDnControl(button->id, PUD_UP);

  /* Register ISR */
  if (wiringPiISR(button->id, INT_EDGE_FALLING, button->isr) != 0) { 
    printf("FATAL: Cannot setup ISR on button %d (%s)\n", button->id, button->cmd); 
    exit(1);
  }

  /* Debug */
  printf("New button %-11s on gpio %2d @ isr %p\n", button->cmd, button->id, button->isr);

  return button;

}

/* Main */
int main (void) {

  /* Use GPIO numbering scheme */
  wiringPiSetupGpio();

  /* Create our buttons */
  buttons.vdown = button_new(23, "volumeup",   isr_vdown);
  buttons.vup   = button_new(24, "volumedown", isr_vup);
  buttons.mute  = button_new(18, "mutetoggle", isr_mute);
  buttons.next  = button_new(25, "nextitem",   isr_next);
  buttons.prev  = button_new( 7, "previtem",   isr_prev);
  buttons.pause = button_new( 8, "playpause",  isr_pause);

  /* Start monitor - sleep since we have nothing to do here */
  printf("Monitoring GPIO activity...\n");
  fflush(stdout);
  while(1) {
    pause();
  }

  return 0;
}
