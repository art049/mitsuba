#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sys/wait.h>
#include <signal.h>
#include <sys/socket.h>
#include "AskingHost.h"

using namespace std;

void AskingHost::testAvailableComputer(const char * telNet, const char * c)
{
  const char * telecomNetwork = telNet;
  const char * computer = c;
  int stdOutput[2];
  int errOutput[2];
  char outputForstd[4096];
  if (pipe(stdOutput) == -1 || pipe(errOutput) == -1)
  {
    cerr << "Error while openning pipe!" << endl;
    exit(1);
  }
  pid_t pid = fork();

  switch (pid) {
    case -1: //Fork error
      cerr << "Error in fork creation!" << endl;
      exit(1);

    case 0: //Child process
      /* child process will write into the two pipes both the standard and error output streams.
      We can then close both pipes.
      And we can execute execlp.
      */
      dup2 (stdOutput[1], STDOUT_FILENO);
      dup2 (errOutput[1], STDERR_FILENO);
      close(stdOutput[0]);
      close(errOutput[0]);
      close (stdOutput[1]);
      close (errOutput[1]);
      execlp("ssh", "ssh", telecomNetwork, "ssh", "-o StrictHostKeyChecking=no", computer, "hostname", (char*) NULL);
      cerr << "execl() failed!"; /* execl doesn't return unless there's an error */
      exit(1);

    default: //Parent process
      /* Parent will read into both pipes, so we can close the reading part of both pipes. */
      close (stdOutput[1]);
      close (errOutput[1]);
      fd_set set;
      struct timeval timeout;
      /* Initialize the file descriptor set. */
      FD_ZERO(&set);
      FD_SET(stdOutput[0], &set);
      /* Initialize the timeout data structure. */
      timeout.tv_sec = 20;
      timeout.tv_usec = 0;

      /*select can return three possibilites:
        0 -> We reached the timeout, so no data was read in the amounted time.
        < 0 -> Error in reading
        else -> There is something to read, so let's do it! */
      int ret = select(stdOutput[0] + 1, &set, NULL, NULL, &timeout);
      if (ret == 0)
      {
          cout << "Timeout Reached! Cannot connect to: " << computer << endl;
      }
      else if (ret < 0) {
          cout << "Error on asking Host." << endl;
        }
      else
      {
        int nbytes = read(stdOutput[0], outputForstd, sizeof(outputForstd));
        if (nbytes != 0)
        {
          close(stdOutput[0]);
          close(errOutput[0]);
          cout << "Machines ready to be used: " << computer << endl;
          char cmd[64];
          cmd[0] = 'e', cmd[1] = 'c', cmd[2] = 'h', cmd[3] = 'o', cmd[4] = ' ';
          for (int i = 0 ; i < 7 ; i++) cmd[5+i] = computer[i];
          cmd[12] = ' ', cmd[13] = '>', cmd[14] = '>', cmd[15] = ' ';
          char buffer[32];
          sprintf(buffer, "%.32s", fileName.c_str());
          unsigned int i = 0;
          for (; i < fileName.length() ; i++) {cout << buffer[i];cmd[16+i] = buffer[i];}
          execlp("bash", "bash", "-c", cmd, (char *)NULL);
          cerr << "execl() failed!"; /* execl doesn't return unless there's an error */
          exit(1);
        }
      }
      close(stdOutput[0]);
      close(errOutput[0]);
      exit(1);
  }
}
