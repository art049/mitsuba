#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fstream>

void startup(const char * telecomNetwork);

using namespace std;

/* Just change the telecomNetwork to yourName@ssh.enst.fr for the program to work. */

int main(int argc, char * argv[])
{
  const char * telecomNetwork;
	if(argc != 2){
	    cout << "ERROR: please pass your ssh id as an argument" << endl;
	    return -1;
	}else{
	    telecomNetwork = argv[1];
	}

  startup(telecomNetwork);
}

void startup(const char * telecomNetwork)
{
  ifstream f("./deploy");
  char line[8] = {};
  while (f >> line)
  {
    const char * server = line;
    pid_t pid = fork(); /* Create a child process */

    switch (pid) {
        case -1: /* Error */
            std::cerr << "Uh-Oh! fork() failed.\n";
            exit(1);
        case 0: /* Child process */
            execlp("askingHost", "askingHost", telecomNetwork, server, (char*) NULL); /* Execute the program */
            std::cerr << "execl() failed!"; /* execl doesn't return unless there's an error */
            exit(1);
        /*default: //Parent process
            std::cout << "Process created with pid " << pid << "\n";
            int* status;
            while (!WIFEXITED(status)) {
                waitpid(pid, status, 0);
            }

            std::cout << "Process exited with " << WEXITSTATUS(status) << "\n";*/
    }
  }
}
