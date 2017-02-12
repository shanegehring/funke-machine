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
	if (c->match.name_prefix) 
		return startswith((char *)name, c->match.name_prefix);
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

static void run_dcap_cmd(host_t *host, char *op, char *active_remote) {
	char *cmd = (char *)malloc(128 + strlen(active_remote));
	sprintf(cmd, "curl 'http://%s:%d/ctrl-int/1/%s' -H 'Active-Remote: %s' -H 'Host: starlight.local.'", 
			host->addr, host->port, op, active_remote);
	fprintf(stderr, "cmd: %s\n", cmd);
	system(cmd);
	free(cmd);
}

typedef struct {
	int stat;
	host_t *host;
	char *srv_name;
	char *active_remote;
	int p_cmd[2];
} srv_t;

enum {
	INIT,
	RESOLVING,
	RESOLVED,
};

static int sockfd_new() {
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	int port = 3391;

	struct sockaddr_in si;
	memset(&si, 0, sizeof(si));
	si.sin_family = AF_INET;
	si.sin_port = htons(port);
	si.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(fd, (struct sockaddr *)&si, sizeof(si)) < 0)
		return -1;

	fprintf(stderr, "binded :%d\n", port);

	return fd;
}

static void sockfd_recv(int fd, char *buf, int len) {
	struct sockaddr sa; 
	socklen_t salen = sizeof(sa);
	int i = recvfrom(fd, buf, len, 0, &sa, &salen);
	if (i > 0)
		buf[i] = 0;
}

static int resolve_itunes_ctrl(srv_t *srv, char *srv_name, char *active_remote) {
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

static void run_cmd(srv_t *srv, char *s) {
	int n = strlen(s);

	char *srv_name = (char *)malloc(n);
	char *active_remote = (char *)malloc(n);

	fprintf(stderr, "cmd: %s\n", s);

	if (sscanf(s, "resolve,%[^,],%[^,]", srv_name, active_remote) == 2) {
		int n = 10;
		while (n > 0 && !resolve_itunes_ctrl(srv, srv_name, active_remote)) {
			sleep(1);
			n--;
		}
	} else if (srv->host != NULL) {
		run_dcap_cmd(srv->host, s, srv->active_remote);
	}

cleanup:
	free(srv_name);
	free(active_remote);
}

static void poll_loop(srv_t *s) {
	int sockfd = sockfd_new();

	for (;;) {
		struct pollfd pfds[] = {
			{.fd = sockfd, .events = POLLIN|POLLERR|POLLHUP|POLLNVAL},
		};
		int r = poll(pfds, 1, -1);
		if (r == 1) {
			if (pfds[0].revents & POLLIN) {
				char buf[2048] = {};
				sockfd_recv(sockfd, buf, sizeof(buf));
				run_cmd(s, buf);
			}
			if (pfds[0].revents & (POLLERR|POLLHUP|POLLNVAL))
				break;
		}
	}
}

int main(int argc, char *argv[]) {
	srv_t s = {};
	poll_loop(&s);
	return 0;
}
