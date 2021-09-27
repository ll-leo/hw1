CC=gcc
CFLAGS=-g -Wno-unused-variable -no-pie

all: timeGetpid timeRead sigHandler

TimeSysCall: timeGetpid.c
	$(CC) $(CFLAGS) timeGetpid.c -o timeGetpid

TimeFork: timeRead.c
	$(CC) $(CFLAGS) timeGetRead.c -o timeGetRead

TimeSignal: TimeSignal.c
	$(CC) $(CFLAGS) sigHandler.c -o sigHandler

clean:
	rm -f timeGetpid timeRead sigHandler
