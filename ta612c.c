#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "serial.h"
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#include <pthread.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/un.h>

// Estrutura de escrita para o aparelho
char writeOut[5];

// Estrutura de leitura do aparelho
char readIn[13];

// Temperaturas convertidas para decimal (Celsius)
float T[4];

// mutex de acesso às temperaturas
pthread_mutex_t temps_mut = PTHREAD_MUTEX_INITIALIZER;

// Exibição contínua
char monitor;

// Porta para comunicação serial
int port_probe;

// Intervalo entre comunicações na porta serial (microsegundos)
#define RX_PAUSE 1e+3
#define TX_PAUSE 1e+3

// Release memory used to parse the command line
void tokens_cleanup(char **tokens) {
  for (int i = 0; tokens[i] != NULL; i++) {
    free(tokens[i]);
  }
  free(tokens);
}

// Parse and run cmdline. Returns
// zero value to keep the shell running
// or non-zero value to end the shell.
int exec(char *cmdline) {

  // Parse command line
  char **tokens = (char **)NULL;
  char *token = (char *)NULL;
  int t = 0;
  token = strtok(cmdline, " \t");
  while (token != NULL) {
    t++;
    tokens = (char **)realloc(tokens, t * sizeof(char *));
    tokens[t - 1] = (char *)malloc((1 + strlen(token)) * sizeof(char));
    strcpy(tokens[t - 1], token);
    token = strtok(NULL, " \t");
  }
  tokens = (char **)realloc(tokens, (1 + t) * sizeof(char *));
  tokens[t] = (char *)NULL;

  if (t == 0) {
    // Empty command line
    return 0;
  }

  /*
   * Builtin commands
   */

  // celsius: leitura das temperaturas em Celsius
  if (!strcmp("celsius", tokens[0])) {

    pthread_mutex_lock(&temps_mut);
    printf("%.1f\t%.1f\t%.1f\t%.1f\n", T[0], T[1], T[2], T[3]);
    pthread_mutex_unlock(&temps_mut);

    tokens_cleanup(tokens);
    return 0;
  }

  // monitor: Exibição contínua das temperaturas em stderr
  if (!strcmp("monitor", tokens[0])) {

    monitor = ! monitor;

    tokens_cleanup(tokens);
    return 0;
  }

  tokens_cleanup(tokens);
  return 0;
}


// Laço para enviar o comando para o
// aparelho e obter as temperaturas
void *pthread_probe(void *arg) {

  // Comando de leitura
  writeOut[0] = 0xAA;
  writeOut[1] = 0x55;
  writeOut[2] = 0x01;
  writeOut[3] = 0x03;
  writeOut[4] = 0x03;

  int rx_bytes = 0;

  while (rx_bytes >= 0) {

    if (port_probe >= 0)
      write(port_probe, writeOut, 5);

    usleep(TX_PAUSE);

    if (port_probe >= 0)
      rx_bytes = read(port_probe, readIn, 13);

    // Guardar leitura
    pthread_mutex_lock(&temps_mut);

    T[0] = (float)((readIn[5] << 8) | (readIn[4] & 0xff)) / 10.0;
    T[1] = (float)((readIn[7] << 8) | (readIn[6] & 0xff)) / 10.0;
    T[2] = (float)((readIn[9] << 8) | (readIn[8] & 0xff)) / 10.0;
    T[3] = (float)((readIn[11] << 8) | (readIn[10] & 0xff)) / 10.0;

    if ( monitor )
      fprintf(stderr, "%.1f\t%.1f\t%.1f\t%.1f\n", T[0], T[1], T[2], T[3]);

    pthread_mutex_unlock(&temps_mut);

    usleep(RX_PAUSE);
  }

  pthread_exit((void *)NULL);
}

int main(int argc, char **argv) {

  // Exibição contínua
  monitor = 0;

  const char serial_probe[] = "/dev/ttyUSB0";

  port_probe = open(serial_probe, O_RDWR);

  int init_tty_return = init_tty(port_probe, B9600);
  if (init_tty_return != 0) {
    fprintf(stderr, "ERROR; return code from init_tty() on %s is %d\n",
            port_probe, init_tty_return);
  }

  // Probe thread
  pthread_t pthread_probe_id;

  int pthread_return = pthread_create(&pthread_probe_id, NULL, &pthread_probe,
                                  (void *)NULL);
  if (pthread_return != 0) {
    fprintf(stderr, "ERROR; return code from pthread_create() is %d\n",
            pthread_return);
    exit(-1);
  }

  /*
   * Shell
   */

  // The username from evironment variable USER
  char *user = getenv("USER");

  // The workdir from evironment variable PWD
  // char *pwd = getenv("PWD");

  // The command prompt
  char *prompt = malloc(1024);
  // snprintf(prompt, 1024, "{%s@%s} ", user, pwd);
  snprintf(prompt, 1024, "{%s@vapomatic} ", user);

  // Command line returned by the user
  char *cmdline;

  // Stop the main loop if end != 0
  int end = 0;

  // The main loop for handling commands
  while (!end) {
    cmdline = readline(prompt);

    if (cmdline == NULL) {
      // EOF in an empty line, break prompt loop
      printf("exit\n");
      break;
    }

    // Add cmdline to the command history
    if (cmdline && *cmdline) {
      add_history(cmdline);
    }

    // Run the command
    end = exec(cmdline);

    // Release cmdline memory
    free(cmdline);
  }

  // End serial communication with probe
  close(port_probe);
  pthread_return = pthread_join(pthread_probe_id, NULL);
  if (pthread_return != 0) {
    fprintf(stderr, "ERROR; return code from pthread_join() is %d\n",
            pthread_return);
  }

  // Released aloccated memory
  free(prompt);

  return 0;
}
