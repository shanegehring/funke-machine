/*
 * Simple UDP IPC Utilities. This file is part of Funke Machine.
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
#include <unistd.h>


#include <poll.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "ipc.h"

ipc_srv_t *ipc_srv_new(int port) {

  ipc_srv_t *srv = (ipc_srv_t *)malloc(sizeof(ipc_srv_t));
  if (srv == NULL) {
    fprintf(stderr, "ERROR: Cannot allocate ipc_srv_t struct\n");
    return NULL;
  }

  srv->port = port;
  srv->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (srv->sockfd < 0) {
    fprintf(stderr, "ERROR: Cannot create socket fd\n");
    return NULL;
  }

  memset(&srv->si, 0, sizeof(srv->si));
  srv->si.sin_family = AF_INET;
  srv->si.sin_port = htons(port);
  srv->si.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(srv->sockfd, (struct sockaddr *)&srv->si, sizeof(srv->si)) < 0) {
    fprintf(stderr, "ERROR: Cannot bind port %d\n", port);
    return NULL;
  }

  return srv;

}

int ipc_srv_recv(const ipc_srv_t *srv, char *msg) {

  if (srv == NULL)  {
     return -1;
  }

  if (msg == NULL)  {
     return -1;
  }

  for (;;) {
    struct pollfd pfds[] = {
      {.fd = srv->sockfd, .events = POLLIN|POLLERR|POLLHUP|POLLNVAL},
    };
    int r = poll(pfds, 1, -1);
    if (r == 1) {
      if (pfds[0].revents & POLLIN) {
        struct sockaddr sa;
        socklen_t salen = sizeof(sa);
        int i = recvfrom(srv->sockfd, msg, sizeof(msg), 0, &sa, &salen);
        if (i > 0) {
           msg[i] = 0;
        }
      }
      if (pfds[0].revents & (POLLERR|POLLHUP|POLLNVAL)) {
        break;
      }
    }
  }
  return 0;
}

ipc_cli_t *ipc_cli_new(int port) {

  ipc_cli_t *cli = (ipc_cli_t *)malloc(sizeof(ipc_cli_t));
  if (cli == NULL) {
    fprintf(stderr, "FATAL: Cannot allocate ipc_cli_t struct\n");
    return NULL;
  }

  cli->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  memset(&cli->si, 0, sizeof(cli->si));
  cli->si.sin_family = AF_INET;
  cli->si.sin_port = htons(port);
  inet_aton("127.0.0.1", &cli->si.sin_addr);

  return cli;

}

int ipc_cli_send(const ipc_cli_t *cli, const char *msg) {
  sendto(cli->sockfd, msg, strlen(msg)+1, 0, (struct sockaddr *)&cli->si, sizeof(cli->si));
  return 0;
}

#ifdef IPC_TEST_SERVER
int main(void) {
   ipc_srv_t *srv = ipc_srv_new(12345);
   char* msg;
   while(1) {
     ipc_srv_recv(srv, msg);
     fprintf(stderr, "MSG: %s\n", msg);
   }
   return 0;
}
#endif
#ifdef IPC_TEST_CLIENT
int main(void) {
   ipc_cli_t *cli = ipc_cli_new(12345);
   ipc_cli_send(cli, "test 1");
   ipc_cli_send(cli, "test 2");
   ipc_cli_send(cli, "test 3");
   return 0;
}
#endif


