#ifndef SERIAL_h
#define SERIAL_h

#include <termios.h>

int init_tty(int port, speed_t speed);

#endif
