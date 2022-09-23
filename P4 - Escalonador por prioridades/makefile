# Guilherme Carbonari Boneti GRR20196478

CFLAGS = -Wall -std=c99

# object-file
objects = ppos_core.o queue.o

all: pingpong-scheduler

pingpong-scheduler: pingpong-scheduler.c $(objects)
	gcc pingpong-scheduler.c -o pingpong-scheduler $(objects) $(CFLAGS)
ppos_core.o: ppos_core.c ppos_data.h
	gcc -c ppos_core.c $(CFLAGS)
queue.o: queue.c queue.h
	gcc -c queue.c $(CFLAGS)
clean:
	-rm -f pingpong-scheduler $(objects)