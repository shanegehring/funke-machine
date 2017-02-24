/*
 * GPIO Daemon. This file is part of Funke Machine.
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

//#include <poll.h> 
#include <stdio.h> 
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
//#include <sys/socket.h>
//#include <net/if.h>
//#include <netinet/in.h>
//#include <netinet/tcp.h>
//#include <arpa/inet.h>
#include <time.h>
//#include <unistd.h>

#include "ipc.h"

/* DACP/GPIO Daemon Ports */
#define DACPD_PORT (3391)
#define GPIOD_PORT (3392)

/* By default, suppress anything under X ms */
#define DEBOUNCE_NSEC (250*1000*1000)

/* DACPD comm channel pointer (global) */
static ipc_cli_t *g_dacpd;

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

/* Buttons (global) */
static buttons_t g_buttons;

/* LED */
typedef struct {
  int id;           /* GPIO number */
  char color[32];   /* Color (green/white/etc) */
  int status;       /* 0=OFF, 1=ON */
} led_t;

typedef struct {
  led_t *white;
  led_t *green;
} leds_t;

/* LEDs (global) */
static leds_t g_leds;

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
    ipc_cli_send(g_dacpd, button->cmd);
  }
}

/* GPIO ISRs */
static void isr_vup(void)   { debounce(g_buttons.vup);   }
static void isr_vdown(void) { debounce(g_buttons.vdown); }
static void isr_mute(void)  { debounce(g_buttons.mute);  }
static void isr_next(void)  { debounce(g_buttons.next);  }
static void isr_prev(void)  { debounce(g_buttons.prev);  }
static void isr_pause(void) { debounce(g_buttons.pause); }

/* Create a new button */
static button_t* button_new(int id, const char* cmd, button_isr isr) {

  /* Allocate new button */
  button_t *button = (button_t *)malloc(sizeof(button_t));
  if (button == NULL) {
    fprintf(stderr, "FATAL: Cannot allocate button struct\n");
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
    fprintf(stderr, "FATAL: Cannot setup ISR on button %d (%s)\n", button->id, button->cmd); 
    exit(1);
  }

  /* Debug */
  fprintf(stderr, "New button %-11s on gpio %2d @ isr %p\n", button->cmd, button->id, button->isr);

  return button;

}

/* Create a new button */
static led_t* led_new(int id, const char* color, int status) {

  /* Allocate new button */
  led_t *led = (led_t *)malloc(sizeof(led_t));
  if (led == NULL) {
    fprintf(stderr, "FATAL: Cannot allocate LED struct\n");
    exit(1);
  }

  /* Init data */
  led->id = id;
  led->status = status;
  strncpy(led->color, color, 32);

  /* Set mode to output */
  pinMode(led->id, OUTPUT);

  /* Drive on or OFF */
  digitalWrite(led->id, status);

  /* Debug */
  fprintf(stderr, "New %s LED on gpio %2d: %s\n", led->color, led->id, led->status ? "ON" : "OFF");

  return led;

}

/* Main */
int main (void) {

  /* Use GPIO numbering scheme */
  wiringPiSetupGpio();

  /* Create our DACPD comm channel */
  g_dacpd = ipc_cli_new(DACPD_PORT);

  /* Create our LEDs */
  g_leds.white = led_new(24, "white", 1);
  g_leds.green = led_new(23, "green", 0);

  /* Create our buttons */
  g_buttons.vdown = button_new(13, "volumeup",   isr_vdown);
  g_buttons.vup   = button_new(26, "volumedown", isr_vup);
  g_buttons.mute  = button_new( 6, "mutetoggle", isr_mute);
  g_buttons.next  = button_new(12, "nextitem",   isr_next);
  g_buttons.prev  = button_new( 5, "previtem",   isr_prev);
  g_buttons.pause = button_new(16, "playpause",  isr_pause);

  /* Open channel for messages */
  ipc_srv_t *gpiod = ipc_srv_new(GPIOD_PORT);
  if (gpiod == NULL) {
    fprintf(stderr, "FATAL: Cannot create IPC server\n");
    exit(1);
  }

  /* Service */
  fprintf(stderr, "GPIOD listening for messages on port %d\n", GPIOD_PORT);
  char msg[256];
  while(1) {

    ipc_srv_recv(gpiod, msg, 256);
    fprintf(stderr, "msg: %s\n", msg);

    /* Shutdown message */
    if (!strcmp(msg, "exit")) {
      fprintf(stderr, "Received 'exit' message\n");
      break;
    /* DACP session status messages */
    } else if (!strcmp(msg, "dacp_open")) {
       digitalWrite(g_leds.green->id, 1);
    } else if (!strcmp(msg, "dacp_close")) {
       digitalWrite(g_leds.green->id, 0);
    }

  }

  fprintf(stderr, "GPIOD exit\n");

  return 0;
}
