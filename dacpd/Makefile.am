# Note, to install lib deps:
#  -> sudo apt-get install libavahi-core-dev
#  -> sudo apt-get install libavahi-client-dev

AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS = shairport-dacpd
shairport_dacpd_SOURCES = dacpd.c ../ipc/ipc.c
shairport_dacpd_CFLAGS = -I../ipc
shairport_dacpd_LDADD = -lavahi-common -lavahi-client -lavahi-core

