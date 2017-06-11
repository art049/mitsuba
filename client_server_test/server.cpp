#include <zmq.hpp>
#include <string>
#include <iostream>
#include <cstdio>
#include <sstream> 

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(n)    Sleep(n)
#endif

using namespace std;

std::string executeScript(std::string script);
void * receiveData(void * arg);
void computeStats();

int main () {

    std::string servAdress = executeScript("./getServerAddress.sh");
    cout << "servAdress " << servAdress << endl; 

    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_PAIR);
    socket.bind ("tcp://*:5555");

    // Let's receive the incoming data
    pthread_t receiveDataThread;
    pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&socket));

    // Let's compute the stats
    computeStats();

    return 0;
}

std::string executeScript(std::string script){
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
    zmq::socket_t * socket((zmq::socket_t *)arg);
    while (true) {
        zmq::message_t request;

        //  Wait for next request from client
        socket->recv(&request);
        std::string rpl = std::string(static_cast<char*>(request.data()), request.size());
        std::cout << "Received \"" << rpl << "\"" << std::endl;

        //  Send reply back to client
        std::ostringstream oss;
        oss << "Obj received";
        std::string replyStr = oss.str();
        int size = replyStr.size();
        zmq::message_t reply(size);
        memcpy(reply.data (), replyStr.c_str(), size);
        socket->send(reply);
    }
    pthread_exit (NULL);
    return NULL;
}

void computeStats(){
    while(1){
        cout << "Crunching numbers, grr..." << endl;
        sleep(5);
    }
}