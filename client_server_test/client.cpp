//
//  Hello World client in C++
//  Connects REQ socket to tcp://localhost:5555
//  Sends "Hello" to server, expects "World" back
//
#include <zmq.hpp>
#include <string>
#include <iostream>
#include <sstream> 
#include <string>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(n)    Sleep(n)
#endif

using namespace std;

void * receiveData(void * arg);
void mainCycle(zmq::socket_t * socket);
int getPortNumber(zmq::socket_t * socket);

int main (int argc, char * argv[])
{
    std::string servAddr;
    if(argc != 2){
        cout << "ERROR: please pass the server address as argument" << endl;
        return -1;
    }else{
        servAddr = "tcp://" + string(argv[1]) + ":5555";
    }
    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t handshakeSocket (context, ZMQ_REQ);

    cout << "Connecting to " << servAddr << endl;
    handshakeSocket.connect (servAddr);

    int portNbr = getPortNumber(&handshakeSocket);
    handshakeSocket.close();
    
    servAddr = "tcp://" + string(argv[1]) + ":" + to_string(portNbr);
    zmq::socket_t socket2 (context, ZMQ_PAIR);
    socket2.connect (servAddr);

    // Let's receive the incoming data
    pthread_t receiveDataThread;
    pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&socket2));

    mainCycle(&socket2);

    return 0;
}

int getPortNumber(zmq::socket_t * socket){
    cout << "ASKING SERVER FOR PORT NUMBER" << endl; 
    
    //  Ask the server for our port
    std::string requestStr = "firstHandShake";
    int size = requestStr.size();
    zmq::message_t request(size);
    memcpy(request.data (), requestStr.c_str(), size);
    socket->send(request);

    //  Wait for the server to respond
    zmq::message_t reply;
    socket->recv(&reply);
    std::string rpl = std::string(static_cast<char*>(reply.data()), reply.size());
    //std::cout << "Received \"" << rpl << "\"" << std::endl;
    int portNbr = stoi(rpl);

    cout << "RECEIVED PORT NUMBER " << portNbr << endl;
    return portNbr; 
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
            std::string objStr = "Photon/Ray " + to_string(i);
            int size = objStr.size();
            
            zmq::message_t message (size);
            memcpy (message.data (), objStr.c_str(), size);
            std::cout << i << " - Sending obj " << objStr << std::endl;
            socket->send (message);
        }
    }
}