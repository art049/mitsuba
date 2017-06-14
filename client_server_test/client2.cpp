#include <zmq.hpp>
#include <string>
#include <iostream>
#include <sstream>
#include <string>

#include "portNumber.h"

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
void sendMessage(zmq::socket_t * socket, string messageStr);

string id;

int main (int argc, char * argv[])
{
    string servAddr, routerAddr;
    if(argc != 4){
        cout << "ERROR: please pass the server and router address and id as arguments" << endl;
        return -1;
    }else{
        ostringstream oss;
        oss << "tcp://" << argv[1] << ":" << serverPortNumber;
        routerAddr = oss.str();
        oss.str("");
        oss << "tcp://" << argv[2] << ":" << handshakePortNumber;
        servAddr = oss.str(); 
        id = argv[3];
        cout << "serv: " << servAddr << " rout: " << routerAddr << endl;
    }
    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t handshakeSocket (context, ZMQ_REQ);

    cout << "\nConnecting to server handshake socket at: " << servAddr << "\n" << endl;
    handshakeSocket.connect(servAddr.c_str());

    int portNbr = getPortNumber(&handshakeSocket);
    handshakeSocket.close();

    cout << "Connecting to server socket at port " << portNbr << endl;
    ostringstream oss;
    oss << "tcp://" << argv[1] << ":" << portNbr;
    servAddr = oss.str();
    zmq::socket_t communicationSocket (context, ZMQ_PAIR);
    communicationSocket.connect (servAddr.c_str());

    cout << "Waiting for others to connect" << endl;
    zmq::message_t reply;
    communicationSocket.recv(&reply);
    string rpl = string(static_cast<char*>(reply.data()), reply.size());
    cout << "Let's go!" << "\n" << endl;

    if(rpl.compare("GO")!=0){
        cout << "ERROR: something went wrong on the server" << endl;
        return -1;
    }else{
        // Let's receive the incoming data
        pthread_t receiveDataThread;
        pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&communicationSocket));
        mainCycle(&communicationSocket);
    }

    return 0;
}

int getPortNumber(zmq::socket_t * socket){
    cout << "Asking server for port number" << endl;

    //  Ask the server for our port
    string requestStr(string("firstHandShake-") + id.c_str());
   
    int size = requestStr.size();
    zmq::message_t message (size);
    memcpy (message.data (), requestStr.c_str(), size);
    cout << "Sending obj " << requestStr << endl;
    socket->send(message);

    //  Wait for the server to respond
    zmq::message_t reply;
    socket->recv(&reply);
    string rpl = string(static_cast<char*>(reply.data()), reply.size());
    //cout << "Received \"" << rpl << "\"" << endl;
    int portNbr;
    istringstream(rpl) >> portNbr;
    cout << "Received port number " << portNbr << endl;

    return portNbr;
}

void * receiveData(void * arg){
    zmq::socket_t * socket((zmq::socket_t *)arg);
    while (true) {
        zmq::message_t request;

        //  Wait for next request from client
        socket->recv(&request);
        string rpl = string(static_cast<char*>(request.data()), request.size());
        cout << "Received \"" << rpl << "\"" << endl;

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
            ostringstream oss;
            oss << "Photon/Ray " << i;
            string objStr = oss.str();
            int size = objStr.size();
            sendMessage(socket, objStr);
        }
        cout << endl;
    }
}

void sendMessage(zmq::socket_t * socket, string messageStr){

    int size = id.size();
    zmq::message_t idMessage(size);
    memcpy (idMessage.data(), id.c_str(), size);
    socket->send (idMessage);

    size = messageStr.size();
    zmq::message_t message (size);
    memcpy (message.data (), messageStr.c_str(), size);
    cout << "Sending obj " << messageStr << endl;
    socket->send(message);
}