CC = gcc
#FLAGS = -Wall
FLAGS = 

all : ta612c

ta612c : ta612c.o serial.o
	$(CC) -o $@ $^ -lreadline -pthread -lm $(FLAGS)

ta612c.o : ta612c.c
	$(CC) -c -o $@ $^ $(FLAGS)

serial.o : serial.c
	$(CC) -c -o $@ $^ $(FLAGS)


clean :
	rm ta612c ta612c.o serial.o
