# To install libs:
#  -> sudo apt-get install libavahi-core-dev
#  -> sudo apt-get install libavahi-client-dev

CC=gcc
CFLAGS=-D_REENTRANT
LDFLAGS=-lavahi-common -lavahi-client -lavahi-core

avahi_client: avahi_client.o
	$(CC) $< -o $@ $(LDFLAGS)

clean:
	$(RM) avahi_client.o avahi_client
