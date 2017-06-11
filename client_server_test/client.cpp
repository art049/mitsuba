//
//  Hello World client in C++
//  Connects REQ socket to tcp://localhost:5555
//  Sends "Hello" to server, expects "World" back
//
#include <zmq.hpp>
#include <string>
#include <iostream>
#include <sstream> 

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(n)    Sleep(n)
#endif

using namespace std;

void * receiveData(void * arg);
void mainCycle(zmq::socket_t * socket);

int main ()
{
    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_PAIR);

    cout << "Connecting to server…" << endl;
    socket.connect ("tcp://localhost:5555");

    // Let's receive the incoming data
    pthread_t receiveDataThread;
    pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&socket));

    mainCycle(&socket);

    return 0;
}

void * receiveData(void * arg){
    zmq::socket_t * socket((zmq::socket_t *)arg);
    while (true) {
        zmq::message_t request;

        //  Wait for next request from client
        socket->recv(&request);
        std::string rpl = std::string(static_cast<char*>(request.data()), request.size());
        std::cout << "Received \"" << rpl << "\"" << std::endl;

    }
    pthread_exit (NULL);
    return NULL;
}

void mainCycle(zmq::socket_t * socket){
    while(1){
        cout << "Computing photons, grr..." << endl;
        sleep(5);
        cout << "Computing rays, grr..." << endl;
        sleep(5);
        cout << "Sending things over:" << endl;
        for (int i = 0; i != 10; i++) {
            std::ostringstream oss;
            oss << "Photon/Ray " << i;
            std::string objStr = oss.str();
            int size = objStr.size();
            
            zmq::message_t message (size);
            memcpy (message.data (), objStr.c_str(), size);
            std::cout << i << " - Sending obj " << objStr << std::endl;
            socket->send (message);
        }
    }
}