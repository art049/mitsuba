#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fstream>
#include <algorithm>
#include "AskingHost.h"

string FILENAME = "AvailableComputers";
bool DEBUG = true;
void startup(const char *);
void rmAvailableFile(string);
void launchRouter(string, string, string , int);

string executeScript(string script);
string scriptName = "getAddress.sh";
string pathToRouter = "./mitsuba-dosppm/client_server_test/router";

using namespace std;

/* Just change the telecomNetwork to yourName@ssh.enst.fr for the program to work. */
int main(int argc, char * argv[])
{
  const char * telecomNetwork;
  int nbClients = 0;
	if(argc != 3){
	    cout << "ERROR: please pass your ssh id as an argument: telecom_login@ssh.enst.fr as well as the number of clients" << endl;
	    return -1;
	}else{
	    telecomNetwork = argv[1];
      nbClients = atoi(argv[2]); //Char * to int.
	    cout << "TelNet: " << telecomNetwork << endl;
      cout << "number of clients to fetch: " << nbClients << endl;
	}
  string serverAddress = executeScript("./" + scriptName);
  serverAddress.erase(remove(serverAddress.begin(), serverAddress.end(), '\n'), serverAddress.end());
  /* Let's remove the previous file containing the available machines (machines we can connect to) if it exist. */
  rmAvailableFile(FILENAME);
  /* asking via ssh through all the network which machines are available. */
  startup(telecomNetwork);
  printf("All child processes finished.\n");
  /* All available computers are now in the AvailableComputers file.
  ** We can now launch router.
  */
  char buffer[32];
  sprintf(buffer, "%.32s", FILENAME.c_str());
  ifstream f(buffer);
  string machine;
  f >> machine;
  cout << "Launching router on computer: " << machine << endl;
  /* Launch router on 'machine'. We give serverAddress and nbClients as requested by the router process. */
  launchRouter(telecomNetwork, machine, serverAddress, nbClients);

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

void startup(const char * telecomNetwork)
{
  ifstream f("deploy");
  char line[8] = {};
  pid_t wait_pid;
  int status;
  AskingHost * ask = new AskingHost(FILENAME);
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
}

void launchRouter(string telecomNetwork, string machine, string serverAddress, int nbClients)
{
  pid_t pid = fork();
  string cmd = "";
  switch (pid) {
      case -1: /* Error */
          cerr << "Uh-Oh! fork() failed.\n";
          exit(1);
      case 0: /* Child process */
          cmd = "ssh " + telecomNetwork + " ssh " + machine + " " + pathToRouter + " " + serverAddress + " " + to_string(nbClients);
          char buffer[128];
          sprintf(buffer, "%.128s", cmd.c_str());
          execlp("bash", "bash", "-c", buffer, (char*)NULL);  /* Execute the program ssh telNet@ssh.enst.fr ssh machine pathToRouter servAddr nbClients */
          exit(1);
      default:
        wait(NULL);
  }
}

string executeScript(string script){
    FILE *lsofFile_p = popen(script.c_str(), "r");

    if (!lsofFile_p)
    {
        return string("-1");
    }

    char buffer[1024];
    char *line_p = fgets(buffer, sizeof(buffer), lsofFile_p);
    pclose(lsofFile_p);
    return string(line_p);
}
