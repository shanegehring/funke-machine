# To install libs:
#  -> sudo apt-get install libavahi-core-dev
#  -> sudo apt-get install libavahi-client-dev

CC=gcc
CFLAGS=-D_REENTRANT
LDFLAGS=-lavahi-common -lavahi-client -lavahi-core

dacp_agent: dacp_agent.o
	$(CC) $< -o $@ $(LDFLAGS)

clean:
	$(RM) dacp_agent.o dacp_agent
