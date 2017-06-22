#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fstream>
#include "AskingHost.h"

string fileName = "AvailableComputers";
void startup(const char *);
void rmAvailableFile(string);

using namespace std;

/* Just change the telecomNetwork to yourName@ssh.enst.fr for the program to work. */
int main(int argc, char * argv[])
{
  const char * telecomNetwork;
	if(argc != 2){
	    cout << "ERROR: please pass your ssh id as an argument: telecom_login@ssh.enst.fr" << endl;
	    return -1;
	}else{
	    telecomNetwork = argv[1];
	    cout << "TelNet: " << telecomNetwork << endl;
	}
  rmAvailableFile(fileName);
  startup(telecomNetwork);
}

void startup(const char * telecomNetwork)
{
  ifstream f("./deploy");
  char line[8] = {};
  pid_t wait_pid;
  int status;
  AskingHost * ask = new AskingHost(fileName);
  while (f >> line)
  {
    const char * computer = line;
    pid_t pid = fork(); /* Create a child process */
    switch (pid) {
        case -1: /* Error */
            cerr << "Uh-Oh! fork() failed.\n";
            exit(1);
        case 0: /* Child process */
            ask->testAvailableComputer(telecomNetwork, computer);  /* Execute the program */
            exit(1);

    }
  }
  while ((wait_pid = wait(&status)) > 0);
  printf("All child processes finished.\n");
  /* All available computers are in the AvailableComputers file. Let's just read it! */
}

void rmAvailableFile(string file) {
  pid_t pid = fork();
  switch (pid) {
    case -1: //Error
      cerr << "Uh-Oh! fork() failed.\n";
      exit(1);
    case 0: //Child process
      cout << "[BEGIN REMOVING] file: " + file << endl;
      char buffer[32];
      sprintf(buffer, "%.32s", file.c_str());
      execlp("rm", "rm", "-f", buffer, (char *)NULL);
      cerr << "Exec failed!" << endl;
      exit(1);
    default: //Parent process
      wait(NULL);
      cout << "[END REMOVING] file: " + file << endl;
  }
}
