CC=gcc
CIMS_VERSION_DEFS=-DCIMS_VERSION_MAJOR=0 -DCIMS_VERSION_MINOR=1 \
				  -DCIMS_VERSION_CODENAME="\"basilisk\""

CFLAGS=-Wall -std=gnu99 -O0 -I ../include -g

SRC=main.c server.c cims.c

all: out/CIMS_server

run: all
	out/CIMS_server -verbose
clean: out
	rm -rf $^
out:
	mkdir $@

out/CIMS_server: out $(SRC)
	$(CC) $(CIMS_VERSION_DEFS) $(CFLAGS) $(SRC) -o $@

