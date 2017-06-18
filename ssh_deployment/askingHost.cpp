#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

int main(int argc, char* argv[])
{
  if (argc != 3)
  {
    cerr << "Please indicate host@network and server, i.e computer to connect to" << endl;
  }
  const char * telecomNetwork = argv[1];
  const char * server = argv[2];
  int stdOutput[2];
  int errOutput[2];
  char outputForstd[8];
  char outputForerr[2048];

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
      execlp("ssh", "ssh", "-t", telecomNetwork, "ssh", "-o StrictHostKeyChecking=no", server, "hostname", (char*) NULL);
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
          cerr << "Timeout Reached! Cannot connect to: " << server << endl;
          kill(pid, SIGKILL);
      }
      else if (ret < 0)
          cerr << "Error on asking Host." << endl;
      else
      {
        int nbytes = read(stdOutput[0], outputForstd, sizeof(outputForstd));
        read(errOutput[0], outputForerr, sizeof(outputForerr));
        if (nbytes != 0)
          cerr << "Machines ready to be used: " << server << endl;
      }
  }
}
