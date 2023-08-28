CC=gcc
CIMS_VERSION_DEFS=-DCIMS_VERSION_MAJOR=0 -DCIMS_VERSION_MINOR=0 \
				  -DCIMS_VERSION_CODENAME="\"basilisk\""

CFLAGS=-Wall -O0 -I ../include -g
SRC=main.c server.c cims.c

all: out/CIMS_server
clean: out
	rm -rf $^
out:
	mkdir $@

out/CIMS_server: out $(SRC)
	$(CC) $(CIMS_VERSION_DEFS) $(CFLAGS) $(SRC) -o $@

