#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

void startup();

using namespace std;

int main(int argc, char * argv[])
{
  startup();
}



void startup()
{
  const char * telecomNetwork = "vbisogno@ssh.enst.fr";
  const char * server = "c128-01";
  pid_t pid = fork(); /* Create a child process */

  switch (pid) {
      case -1: /* Error */
          std::cerr << "Uh-Oh! fork() failed.\n";
          exit(1);
      case 0: /* Child process */
          execlp("ssh", "ssh", "-t", telecomNetwork, "ssh ", server, "ls", "-la", (char*) NULL); /* Execute the program */
          std::cerr << "Uh-Oh! execl() failed!"; /* execl doesn't return unless there's an error */
          exit(1);
      default: /* Parent process */
          std::cout << "Process created with pid " << pid << "\n";
          int* status;
          while (!WIFEXITED(status)) {
              waitpid(pid, status, 0); /* Wait for the process to complete */
          }

          std::cout << "Process exited with " << WEXITSTATUS(status) << "\n";
  }
}
