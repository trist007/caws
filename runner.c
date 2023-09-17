#include <stdio.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>

const int BUFSIZE = 128;

int main(int argc, char *argv[]) {

  //extern char **environ;
  char buffer[BUFSIZE];
  static const char delimiters[] = " \n";
  char *token;
  char *saveptr1;
  char *environ[] = { "TERM=xterm", NULL };
  char *bin = "/usr/bin/aws";

  // input from stdin
  if(argc == 1) {
    int i = 0;
    int num_tok = 0;
    argv[0] = bin;
    i++;
    fgets(buffer, BUFSIZE, stdin);
      token = strtok_r(buffer, delimiters, &saveptr1);
      argv[i] = token;
      i++;
      num_tok++;
      while((token = strtok_r(NULL, delimiters, &saveptr1)) != NULL)
      {
        argv[i] = token;
        i++;
        num_tok++;
      }
      argv[i] = '\0';
      execvpe(bin, argv, environ);
  }
  else
    execvp(bin, argv);

return 0;
}
