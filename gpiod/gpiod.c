#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/* DACP Daemon Port */
#define DACPD_PORT 3391

/* GPIO Pin Mappings */
#define GPIO_VOLUME_MUTE  18
#define GPIO_VOLUME_UP    23
#define GPIO_VOLUME_DOWN  24
#define GPIO_PLAY_NEXT    25
#define GPIO_PLAY_PAUSE   8
#define GPIO_PLAY_PREV    7

/* Sends message to DACPD */
void doCmd(const char* cmd) {

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

/* Handlers */
void doVolumeMute (void) { doCmd("mutetoggle"); }
void doVolumeUp   (void) { doCmd("volumeup");   }
void doVolumeDown (void) { doCmd("volumedown"); }
void doPlayNext   (void) { doCmd("nextitem");   }
void doPlayPrev   (void) { doCmd("previtem");   }
void doPlayPause  (void) { doCmd("playpause");  }

/* Main */
int main (void)
{
  wiringPiSetupGpio();

  wiringPiISR(GPIO_VOLUME_MUTE, INT_EDGE_FALLING, &doVolumeMute);
  wiringPiISR(GPIO_VOLUME_UP,   INT_EDGE_FALLING, &doVolumeUp);
  wiringPiISR(GPIO_VOLUME_DOWN, INT_EDGE_FALLING, &doVolumeDown);
  wiringPiISR(GPIO_PLAY_NEXT,   INT_EDGE_FALLING, &doPlayNext);
  wiringPiISR(GPIO_PLAY_PREV,   INT_EDGE_FALLING, &doPlayPrev);
  wiringPiISR(GPIO_PLAY_PAUSE,  INT_EDGE_FALLING, &doPlayPause);

  printf("Monitoring GPIO activity...\n");
  fflush(stdout);
  while(1);

  return 0;
}
