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
 *
 * Notes:
 *   The use case for this API is for very simple one way client to
 *   server string message passing via UDP.  UDP is nice for this 
 *   use case because I just want the clients to broadcast updates 
 *   to anyone who happens to be listening.  If the server isn't 
 *   up and listening, the messages just get lost.  This is actually
 *   what I want... lightweight, simple, forgiving.
 */

#ifndef IPC_H
#define IPC_H

/* Server */
typedef struct {
  int port;
  int sockfd;
  struct sockaddr_in si;
} ipc_srv_t;

/* Creates a new server on the specified port */
ipc_srv_t *ipc_srv_new(int port);

/* Blocks until a new message is received from the client.
 * Note: The msg param must be allocated by the caller and
 * be large enough to accomodate the largest message length
 * (maxlen) */
int ipc_srv_recv(const ipc_srv_t *srv, char *msg, int maxlen);

/* Client */
typedef struct {
  int port;
  int sockfd;
  struct sockaddr_in si;
} ipc_cli_t;

/* Creates a new client connection to the server on the specified port */
ipc_cli_t *ipc_cli_new(int port);

/* Sends message to the server */
int ipc_cli_send(const ipc_cli_t *cli, const char *msg);

#endif /* IPC_H */
