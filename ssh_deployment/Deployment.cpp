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
#include <cstdio>
#include <sstream>

void startup(const char *);
void rmAvailableFile(string);
void launchRouter(string, string, string , int);
void launchClient(string, string, int);
void copyScriptAddress(string);
void copySubscene(string, string, int);
void rmPreviousSubscene(string, string, int);
string executeScript(string script);

/* Global variables */
string FILENAME = "ssh_deployment/AvailableComputers";
bool DEBUG = true;
string scriptName = "getAddress.sh";
string scriptPath = "src/client_server/getAddress.sh";
string pathToRouter = "src/client_server/router";
string pathToClient = "binaries/mitsuba";
string pathToSubsceneInClient = "/tmp/subscene/";
string pathToSubsceneInServer = "/tmp/subscene/";
string destAddressScript = "/cal/homes/";
string deployFile = "ssh_deployment/deploy";

using namespace std;

/* Just change the telecomNetwork to yourName@ssh.enst.fr for the program to work. */
int main(int argc, char * argv[])
{
  const char * telecomNetwork;
  int nbClients = 0;
	if(argc != 3){
	    cout << "ERROR: please pass your ssh id as an argument: telecom_login@ssh.enst.fr as well as the number of clients" << endl;
	    return -1;
	} else {
	    telecomNetwork = argv[1];
      nbClients = atoi(argv[2]); //Char * to int.
	    cout << "TelNet: " << telecomNetwork << endl;
      cout << "number of clients to fetch: " << nbClients << endl;
	}
  string serverAddress = executeScript("./" + scriptPath);
  serverAddress.erase(remove(serverAddress.begin(), serverAddress.end(), '\n'), serverAddress.end());
  /* Let's remove the previous file containing the available machines (machines we can connect to) if it exist. */
  rmAvailableFile(FILENAME);
  /* asking via ssh through all the network which machines are available. */
  startup(telecomNetwork);
  printf("All child processes finished.\n");
  /* Copying of the script getAdress.sh for the router to work properly. */
  copyScriptAddress(telecomNetwork);
  /* All available computers are now in the AvailableComputers file.
  ** We can now launch router.
  */
  char buffer[64];
  sprintf(buffer, "%.64s", FILENAME.c_str());
  ifstream f(buffer);
  string machine;
  f >> machine;
  //getline(f,machine);

  cout << "Launching router on computer: " << machine << endl;
  /* Launch router on 'machine'. We give serverAddress and nbClients as requested by the router process. */
  launchRouter(telecomNetwork, machine, serverAddress, nbClients);

  for (int i = 0 ; i < nbClients ; i++) {
    /* copy of subscene_i on client_i. */
    if (!f.eof()) {
      f >> machine;
      rmPreviousSubscene(telecomNetwork, machine, i);
      cout << "Copying sub" << i << " on computer: " << machine << endl;
      copySubscene(telecomNetwork, machine, i);
      /* We can then launch client who needs subscene_i to work properly. */
      cout << "Launching client num: " << i << " on computer: " << machine << endl;
      launchClient(telecomNetwork, machine, i);
    }
  }
}

void rmAvailableFile(string file) {
  pid_t pid = fork();
  switch (pid) {
    case -1: //Error
      cerr << "Uh-Oh! fork() failed.\n";
      exit(1);
    case 0: //Child process
      cout << "[BEGIN REMOVING] file: " + file << endl;
      char buffer[64];
      sprintf(buffer, "%.64s", file.c_str());
      execlp("rm", "rm", "-f", buffer, (char *)NULL);
      cerr << "Exec failed!" << endl;
      exit(1);
    default: //Parent process
      wait(NULL);
      cout << "[END REMOVING] file: " + file << endl;
  }
}

void rmPreviousSubscene(string telecomNetwork, string machine, int numClient) {
  string cmd = "";
  pid_t pid = fork();
  switch (pid) {
    case -1: //Error
      cerr << "Uh-Oh! fork() failed.\n";
      exit(1);
    case 0: //Child process
      cmd = "ssh " + telecomNetwork + " ssh " + machine + " rm -rf " + pathToSubsceneInClient;
      char buffer[256];
      sprintf(buffer, "%.256s", cmd.c_str());
      execlp("bash", "bash", "-c", buffer, (char *)NULL);
      cerr << "Exec failed!" << endl;
      exit(1);
    default: //Parent process
      wait(NULL);
  }
}

void startup(const char * telecomNetwork)
{
  ifstream f(deployFile);
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
            cerr << "fork() failed.\n";
            exit(1);
        case 0: /* Child process */
            ask->testAvailableComputer(telecomNetwork, computer);  /* Execute the program */
            exit(1);

    }
  }
  while ((wait_pid = wait(&status)) > 0);
}

string executeScript(string script) {
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

void copyScriptAddress(string telecomNetwork) {
  pid_t pid = fork();
  string cmd = "";
  string userName = "";
  switch (pid) {
      case -1: /* Error */
          cerr << "fork() failed.\n";
          exit(1);
      case 0: /* Child process */
          userName = telecomNetwork.substr(0, telecomNetwork.find("@"));
          cmd = string("scp ") + "./" + scriptName + " " + telecomNetwork + ":" + destAddressScript + userName + "/";
          char buffer[64];
          sprintf(buffer, "%.64s", cmd.c_str());
          execlp("bash", "bash", "-c", buffer, (char*)NULL);  /* Execute the program ssh telNet@ssh.enst.fr ssh machine pathToRouter servAddr nbClients */
          cerr << "Copy of the getAdress script file failed. Exiting ..." << endl;
          exit(1);
      default:
        wait(NULL);
  }
}

void copySubscene(string telecomNetwork, string machine, int numClient) {
  pid_t pid = fork();
  string cmd = "";
  string userName = "";
  switch (pid) {
      case -1: /* Error */
          cerr << "fork() failed.\n";
          exit(1);
      case 0: /* Child process */
          userName = telecomNetwork.substr(0, telecomNetwork.find("@"));
          cmd = string("scp -r -o ProxyCommand=\"ssh ") + telecomNetwork + " nc %h %p\" -o StrictHostKeyChecking=no " + pathToSubsceneInServer + "sub" +
          to_string(numClient) + " " + userName + "@" + machine + ":/tmp/subscene/";
          cout << "Mycmd: " << cmd << endl;
          char buffer[256];
          sprintf(buffer, "%.256s", cmd.c_str());
          execlp("bash", "bash", "-c", buffer, (char*)NULL);  /* Execute the program ssh telNet@ssh.enst.fr ssh machine pathToRouter servAddr nbClients */
          cerr << "Copy of the subscene folder failed. Exiting ..." << endl;
          exit(1);
      default:
        wait(NULL);
  }
}

void launchRouter(string telecomNetwork, string machine, string serverAddress, int nbClients)
{
  pid_t pid = fork();
  string cmd = "";
  switch (pid) {
      case -1: /* Error */
          cerr << "fork() failed.\n";
          exit(1);
      case 0: /* Child process */
          string dir = executeScript("pwd");
          dir.erase(remove(dir.begin(), dir.end(), '\n'), dir.end());
          cmd = "xterm -e ssh " + machine + " cd " + dir + "/" + " && ./" + pathToRouter + " " + serverAddress + " " + to_string(nbClients);
          cout << "routerCmd: " << cmd << endl;
          char buffer[128];
          sprintf(buffer, "%.128s", cmd.c_str());
          execlp("bash", "bash", "-c", buffer, (char*)NULL);  /* Execute the program ssh telNet@ssh.enst.fr ssh machine pathToRouter servAddr nbClients */
          cerr << "Launching router failed. Exiting ..." << endl;
          exit(1);
  }
}

void launchClient(string telecomNetwork, string machine, int numClient) {
  pid_t pid = fork();
  string cmd = "";
  switch (pid) {
      case -1: /* Error */
          cerr << "fork() failed.\n";
          exit(1);
      case 0: /* Child process */
          string dir = executeScript("pwd");
          dir.erase(remove(dir.begin(), dir.end(), '\n'), dir.end());
          cmd = "xterm -e ssh " + telecomNetwork + " ssh " + machine + " cd " + dir + "/" + " && ./" + pathToClient + " " + pathToSubsceneInClient + "scene.xml";
          char buffer[256];
          sprintf(buffer, "%.256s", cmd.c_str());
          execlp("bash", "bash", "-c", buffer, (char*)NULL);  /* Execute the program ssh telNet@ssh.enst.fr ssh machine pathToRouter servAddr nbClients */
          cerr << "Launching client failed. Exiting ..." << endl;
          exit(1);
  }
}
