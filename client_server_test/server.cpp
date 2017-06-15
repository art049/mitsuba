#include <zmq.hpp>
#include <string>
#include <iostream>
#include <cstdio>
#include <sstream> 

#include "messagesAndPorts.h"

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(n)    Sleep(n)
#endif

#define NB_CHUNKS 2

using namespace std;

string executeScript(string script);
void * receiveData(void * arg);
void computeStats();

string getAddressScript = "getAddress.sh";
string launchRouterClientScript = "launchEverything.sh";

int main () {
	
	string tmp = executeScript("chmod +x " + getAddressScript + " && echo ok");
    string servAdress = executeScript("./" + getAddressScript);
    servAdress.erase(remove(servAdress.begin(), servAdress.end(), '\n'), servAdress.end());
    cout << "servAdress " << servAdress << endl;

    //  Prepare our context and socket
    cout << "Creating socket at tcp://*:" << serverPortNumber << endl;
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_PAIR);
    ostringstream oss;
    oss << "tcp://*:" << serverPortNumber;
    socket.bind (oss.str());
    oss.str("");

    // Launching router and clients
    oss << "./" << launchRouterClientScript << " " << servAdress << " " << NB_CHUNKS;
    tmp = executeScript(oss.str());
    oss.str("");

    cout << tmp << endl;

    // Wait for router to send its address
    cout << "Waiting for router to send its address" << endl;
    zmq::message_t message;
    socket.recv(&message);
    string routerAddress = string(static_cast<char*>(message.data()), message.size());
    routerAddress.erase(remove(routerAddress.begin(), routerAddress.end(), '\n'), routerAddress.end());
    cout << "Received router address: " << routerAddress << endl;
    
    // Reply with "Connected!"
    string replyStr = "Connected!";
    int size = replyStr.size();
    zmq::message_t reply(size);
    memcpy(reply.data (), replyStr.c_str(), size);
    socket.send(reply);

    cout << "Waiting for others to connect" << endl;
    zmq::message_t goSignal;
    socket.recv(&goSignal);
    std::string signalStr = std::string(static_cast<char*>(goSignal.data()), goSignal.size());
    cout << "Let's go!" << "\n" << endl;

    if(signalStr.compare("GO")!=0){
        cout << "ERROR: something went wrong on the server" << endl;
        return -1;
    }else{
        // Let's receive some data and compute the stats in parallel
        pthread_t receiveDataThread;
        pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&socket));
        computeStats();
    }      

    //close all sockets 

    return 0;
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

void * receiveData(void * arg){
    
    zmq::socket_t * socket = (zmq::socket_t *)arg;
    
    // Poll through the messages
    while (1) {
        // Check if we received a message
        zmq::message_t message;
        socket->recv(&message);
        string messageStr = string(static_cast<char*>(message.data()), message.size());
        cout << "Received \"" << messageStr << "\"" << endl;
    }

    pthread_exit (NULL);
    return NULL;
}

void computeStats(){
    while(1){
        cout << "Crunching numbers, grr...\n" << endl;
        sleep(5);
    }
}
