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

#include "socketPolling.h"

void * receiveData(void * arg);
void mainCycle(vector < zmq::socket_t * > &sockets);
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
    vector < zmq::socket_t * > sockets;
    vector<zmq::pollitem_t> items;
    zmq::socket_t handshakeSocket (context, ZMQ_REQ);
    socketPollingInfo infos = {sockets, items};

    cout << "\nConnecting to server handshake socket at: " << servAddr << "\n" << endl;
    handshakeSocket.connect(servAddr.c_str());

    int portNbr = getPortNumber(&handshakeSocket);
    handshakeSocket.close();

    cout << "CONNECTING TO SERVER SOCKET AT PORT " << portNbr << endl;
    std::ostringstream oss;
    oss << "tcp://" << argv[1] << ":" << portNbr;
    servAddr = oss.str();
    
    zmq::socket_t * socket = new zmq::socket_t(context, ZMQ_PAIR);
    socket->connect(servAddr.c_str());
    sockets.push_back(socket);
    zmq::pollitem_t item = {static_cast<void *>(*(sockets[0])), 0, ZMQ_POLLIN, 0};
    items.push_back(item);

    cout << "WAITING FOR OTHERS TO CONNECT " << endl;
   	zmq::message_t reply;
    sockets[0]->recv(&reply);
    std::string rpl = std::string(static_cast<char*>(reply.data()), reply.size());
    cout << "LET'S GO!" << "\n" << endl;

    if(rpl.compare("GO")!=0){
    	cout << "ERROR: something went wrong on the server" << endl;
        return -1;
    }else{
		// Let's receive the incoming data
		pthread_t receiveDataThread;
        // We need to poll through all the sockets to get all incoming messages
		pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&infos));
        // Here we only send stuff, we just need the sockets data and not the items usee for polling
		mainCycle(sockets);
	}

    // Free everything

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
    int portNbr;
    std::istringstream(rpl) >> portNbr;
    cout << "RECEIVED PORT NUMBER " << portNbr << endl;

    return portNbr;
}

void * receiveData(void * arg){
    socketPollingInfo * infos = (socketPollingInfo *)arg;
    vector < zmq::pollitem_t > items = infos->items;
    vector < zmq::socket_t * > sockets = infos->sockets;
    
    // Poll through the messages
    while (1) {
        zmq::message_t request;
        int itemsSize = items.size();
        zmq::poll(items.data(), itemsSize, -1);

        for(int i=0; i<itemsSize; i++){
            // Check if that client sent a message
            if (items[i].revents & ZMQ_POLLIN) {
                sockets[i]->recv(&request);
                std::string rpl = std::string(static_cast<char*>(request.data()), request.size());
                std::cout << "Received \"" << rpl << "\" from " << i << std::endl;
            }
        }
        
    }

    pthread_exit (NULL);
    return NULL;
}

void mainCycle(vector < zmq::socket_t * > &sockets){
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
            sockets[0]->send (message);
        }
        cout << endl;
    }
}
