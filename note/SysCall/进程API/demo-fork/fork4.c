#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int count = 0;

int main(void) {
  fork();
  fork();

  for (int i = 0 ; i < 100 ; i++)
  {
     count++;
  
     printf("process (%d): %d\n", getpid(), count);
  }
  while (wait(NULL) > 0);
  return 0;
}