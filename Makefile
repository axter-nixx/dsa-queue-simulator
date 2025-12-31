CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lSDL2 -lSDL2_ttf -lpthread

all: simulator traffic_generator

simulator: simulator.c
	$(CC) $(CFLAGS) -o simulator simulator.c $(LIBS)

traffic_generator: traffic_generator.c
	$(CC) $(CFLAGS) -o traffic_generator traffic_generator.c

clean:
	rm -f simulator traffic_generator vehicles.data
