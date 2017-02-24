/*
 * DACP Daemon. This file is part of Funke Machine.
 * Copyright (c) Shane Gehring 2017
 * 
 * Code leveraged from: https://www.sugrsugr.com/index.php/airplay-prev-next
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

#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <locale.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/domain.h>
#include <avahi-common/llist.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include "ipc.h"

#define DACPD_PORT (3391)
#define GPIOD_PORT (3392)

static void *memdup(void *p, int size) {
  void *r = malloc(size);
  memcpy(r, p, size);
  return r;
}

typedef struct {
  char addr[32];
  int port;
} host_t;

typedef struct {
  AvahiSimplePoll *poll;
  AvahiClient *cli;
  AvahiServiceBrowser *br;
  int ref_nr;

  struct {
    char *name_prefix;
    char *type;
  } match;

  host_t *found;
} ctx_t;

static void client_callback(AvahiClient *cli, AvahiClientState state, void *ud);

static void ctx_find_service(ctx_t *c) {
  c->poll = avahi_simple_poll_new();
  if (c->poll == NULL) {
    fprintf(stderr, "Failed to create simple poll object.\n");
    goto cleanup;
  }
  c->ref_nr = 1;

  int err;
  c->cli = avahi_client_new(
    avahi_simple_poll_get(c->poll), 0, client_callback, 
    c, &err
  );
  if (c->cli == NULL) {
    fprintf(stderr, "Failed to create client object. error=%d\n", err);
    goto cleanup;
  }

  avahi_simple_poll_loop(c->poll);

cleanup:
  if (c->br)
    avahi_service_browser_free(c->br);
  if (c->cli)
    avahi_client_free(c->cli);
  if (c->poll) 
    avahi_simple_poll_free(c->poll);
}

static void ctx_cleanup(ctx_t *c) {
  if (c->found)
    free(c->found);
}

static void ctx_ref(ctx_t *c) {
  c->ref_nr++;
}

static void ctx_unref(ctx_t *c) {
  c->ref_nr--;
  if (c->ref_nr == 0)
    avahi_simple_poll_quit(c->poll);
}

typedef struct {
  ctx_t *c;
  AvahiServiceResolver *resolver;
} srvctx_t;

static srvctx_t *srvctx_new(ctx_t *c) {
  srvctx_t *sc = (srvctx_t *)malloc(sizeof(srvctx_t));
  sc->c = c;
  ctx_ref(c);
  return sc;
}

static srvctx_t *srvctx_unref(srvctx_t *sc) {
  ctx_unref(sc->c);
  if (sc->resolver)
    avahi_service_resolver_free(sc->resolver);
  free(sc);
}

static int startswith(char *s, char *pat) {
  int n = strlen(pat);
  return !strncmp(s, pat, n);
}

static int ctx_ismatch(ctx_t *c, const char *name) {
  if (c->match.name_prefix)  {
    // Strip off leading 0's of ID before comparison:
    // Example:  iTunes_Ctrl_0F44ADA81654B1C9 => iTunes_Ctrl_F44ADA81654B1C9
    char *tname = (char *)malloc(strlen(name) + 1);
    int ni = 0;
    int ti = 0;
    int skip = 0;
    while (name[ni] != '\0') {
      if (!(skip && (name[ni] == '0'))) {
        tname[ti] = name[ni];
        ti++;
      }
      if (skip && (name[ni] != '0')) {
        skip = 0;
      }
      if (name[ni] == '_') {
        skip = 1;
      } 
      ni++;
    }
    tname[ti] = '\0';
    ti = startswith(tname, c->match.name_prefix);
    free(tname);
    return ti;
  }
  return 1;
}

static void service_resolver_callback(
    AvahiServiceResolver *r,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *a,
    uint16_t port,
    AvahiStringList *txt,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void *ud) 
{

  srvctx_t *sc = (srvctx_t *)ud;

  switch (event) {
    case AVAHI_RESOLVER_FOUND: {
      char address[AVAHI_ADDRESS_STR_MAX];
      avahi_address_snprint(address, sizeof(address), a);

      fprintf(stderr, "found: name=%s type=%s addr=%s port=%d\n", name, type, address, port);

      ctx_t *c = sc->c;

      if (ctx_ismatch(c, name)) {
        if (c->found)
          free(c->found);

        c->found = (host_t *)malloc(sizeof(host_t));
        strcpy(c->found->addr, address);
        c->found->port = port;
      } else {
        fprintf(stderr, "resolve: name not match(%s vs %s)\n", name, c->match.name_prefix);
      }

      srvctx_unref(sc);
      break;
    }

    case AVAHI_RESOLVER_FAILURE: {
      srvctx_unref(sc);
      break;
    }
  }
}

static const char *browser_event_to_string(AvahiBrowserEvent event) {
    switch (event) {
        case AVAHI_BROWSER_NEW : return "NEW";
        case AVAHI_BROWSER_REMOVE : return "REMOVE";
        case AVAHI_BROWSER_CACHE_EXHAUSTED : return "CACHE_EXHAUSTED";
        case AVAHI_BROWSER_ALL_FOR_NOW : return "ALL_FOR_NOW";
        case AVAHI_BROWSER_FAILURE : return "FAILURE";
    }
    return "?";
}

static void service_browser_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AvahiLookupResultFlags flags,
    void *ud) 
{
  ctx_t *c = (ctx_t *)ud;
  fprintf(stderr, "resolve: browser event=%s if=%d prot=%d type=%s name=%s\n", 
    browser_event_to_string(event), 
    interface, protocol, type, name);

  switch (event) {
  case AVAHI_BROWSER_NEW: {
    srvctx_t *sc = srvctx_new(c);
    sc->resolver = avahi_service_resolver_new(c->cli,
      interface, protocol, name, type, domain, 
      AVAHI_PROTO_UNSPEC, 0, service_resolver_callback, sc
    );

    if (sc->resolver == NULL)
      srvctx_unref(sc);
    break;
  }

  case AVAHI_BROWSER_CACHE_EXHAUSTED:
    ctx_unref(c);
    break;
  }
}

static void client_callback(AvahiClient *cli, AvahiClientState state, void *ud) {
  ctx_t *c = (ctx_t *)ud;

  switch (state) {
  case AVAHI_CLIENT_FAILURE:
    ctx_unref(c);
    break;

  case AVAHI_CLIENT_S_REGISTERING:
  case AVAHI_CLIENT_S_RUNNING:
  case AVAHI_CLIENT_S_COLLISION: {
    fprintf(stderr, "resolve: browse service type=%s\n", c->match.type);
    c->br = avahi_service_browser_new(
        cli,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_INET,
        c->match.type,
        NULL,
        0,
        service_browser_callback,
        c);
    }
    break;
  }
}

static void run_dcap_cmd(host_t *host, char *msg, char *active_remote) {

  if ((host == NULL) || (msg == NULL) || (active_remote == NULL)) {
    return;
  }

  int rc;
  char *cmd = (char *)malloc(128 + strlen(active_remote));
  sprintf(cmd, "curl 'http://%s:%d/ctrl-int/1/%s' -H 'Active-Remote: %s' -H 'Host: starlight.local.'", 
      host->addr, host->port, msg, active_remote);
  fprintf(stderr, "cmd: %s\n", cmd);
  rc = system(cmd);
  free(cmd);

}

typedef struct {
  host_t *host;
  char *srv_name;
  char *active_remote;
} srv_t;

enum {
  INIT,
  RESOLVING,
  RESOLVED,
};

static void srv_free(srv_t *srv) {

  if (srv->srv_name) {
    free(srv->srv_name);
    srv->srv_name = NULL;
  }

  if (srv->active_remote) {
    free(srv->active_remote);
    srv->active_remote = NULL;
  }

  if (srv->host) {
    free(srv->host);
    srv->host = NULL;
  }

}

static int resolve_itunes_ctrl(srv_t *srv, char *srv_name, char *active_remote) {

  srv_free(srv);

  fprintf(stderr, "resolve: srv=%s active_remote=%s\n", srv_name, active_remote);

  ctx_t rsv = { .match = {.name_prefix = srv_name, .type = "_dacp._tcp"} };
  ctx_find_service(&rsv);

  if (rsv.found) {
    srv->srv_name = strdup(srv_name);
    srv->active_remote = strdup(active_remote);
    srv->host = (host_t *)memdup(rsv.found, sizeof(host_t));
    fprintf(stderr, "resolve: host=%s:%d srvname=%s\n", srv->host->addr, srv->host->port, srv->srv_name);
  } else {
    fprintf(stderr, "resolve: srv=%s failed\n", srv_name);
  }

  ctx_cleanup(&rsv);

  return !!rsv.found;
}

/* 
 * All of the UDP server/client communication takes place here (main).
 * The avahi lookup code and dacp client communication is handled above.
 */
int main(int argc, char *argv[]) {
  
  srv_t srv = { .host = NULL, .srv_name = NULL, .active_remote = NULL };
  char msg[256];

  ipc_srv_t *ipc_srv = ipc_srv_new(DACPD_PORT);
  ipc_cli_t *ipc_cli = ipc_cli_new(GPIOD_PORT);

  fprintf(stderr, "DACPD listening for messages on port %d\n", DACPD_PORT);

  while(1) {

    ipc_srv_recv(ipc_srv, msg, 256);
    fprintf(stderr, "msg: %s\n", msg);

    /* Shutdown message */
    if (!strcmp(msg, "exit")) {
      fprintf(stderr, "Received 'exit' message\n");
      break;
    /* Messages from UI (playback controls) */
    } else if (
      (!strcmp(msg, "volumeup"))   ||
      (!strcmp(msg, "volumedown")) ||
      (!strcmp(msg, "mutetoggle")) ||
      (!strcmp(msg, "nextitem"))   ||
      (!strcmp(msg, "previtem"))   ||
      (!strcmp(msg, "playpause"))  ){
        run_dcap_cmd(srv.host, msg, srv.active_remote);
    /* Messages from shairport (DACP sessions) */
    } else if (!strcmp(msg, "dacp_close")) {
      fprintf(stderr, "Received 'dacp_close' message\n");
      srv_free(&srv);
      ipc_cli_send(ipc_cli, "dacp_close");
    } else {
      size_t n = strlen(msg);
      char *srv_name = (char *)malloc(n);
      char *active_remote = (char *)malloc(n);
      if (sscanf(msg, "dacp_open,%[^,],%[^,]", srv_name, active_remote) == 2) {
        int n = 10;
        while (n > 0 && !resolve_itunes_ctrl(&srv, srv_name, active_remote)) {
          sleep(1);
          n--;
        }
        ipc_cli_send(ipc_cli, "dacp_open");
      }
      free(srv_name);
      free(active_remote);
    }

  }

  fprintf(stderr, "DACPD exiting\n");

  return 0;

}
