#include <zmq.hpp>
#include <string>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "messagesAndPorts.h"


using namespace std;

void * receiveData(void * arg);
void mainCycle(zmq::socket_t * socket);
int getPortNumber(zmq::socket_t * socket, string id);
void sendMessage(zmq::socket_t * socket, string messageStr, string recipient);
string getRandomRecipient();

int main (int argc, char * argv[])
{
    // TODO: read this from a file created by the script that launches clients
    string routerAddr, id;
    if(argc != 3){
        cout << "ERROR: please pass the router address and id as arguments" << endl;
        return -1;
    }else{
        ostringstream oss;
        oss << "tcp://" << argv[1] << ":" << handshakePortNumber;
        routerAddr = oss.str(); 
        id = argv[2];
        cout << " rout: " << routerAddr << endl;
    }

    // TODO: Remove this
    srand (time(NULL));

    //  Prepare our context and socket
    zmq::context_t context (1);
    zmq::socket_t handshakeSocket (context, ZMQ_REQ);

    cout << "\nConnecting to router handshake socket at: " << routerAddr << "\n" << endl;
    handshakeSocket.connect(routerAddr.c_str());

    int portNbr = getPortNumber(&handshakeSocket, id);
    handshakeSocket.close();

    cout << "Connecting to router socket at port " << portNbr << endl;
    ostringstream oss;
    oss << "tcp://" << argv[1] << ":" << portNbr;
    routerAddr = oss.str();
    zmq::socket_t communicationSocket (context, ZMQ_PAIR);
    communicationSocket.connect (routerAddr.c_str());

    cout << "Waiting for others to connect" << endl;
    zmq::message_t reply;
    communicationSocket.recv(&reply);
    string rpl = string(static_cast<char*>(reply.data()), reply.size());
    cout << "Let's go!" << "\n" << endl;

    if(rpl.compare("GO")!=0){
        cout << "ERROR: something went wrong on the router" << endl;
        return -1;
    }else{
        // Let's receive the incoming data
        pthread_t receiveDataThread;
        pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&communicationSocket));
        mainCycle(&communicationSocket);
    }

    return 0;
}

int getPortNumber(zmq::socket_t * socket, string id){
    cout << "Asking router for port number" << endl;

    //  Ask the router for our port
    string requestStr(string("firstHandShake-") + id.c_str());
   
    int size = requestStr.size();
    zmq::message_t message (size);
    memcpy (message.data (), requestStr.c_str(), size);
    cout << "Sending obj " << requestStr << endl;
    socket->send(message);

    //  Wait for the router to respond
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
            string recipient = getRandomRecipient();
            sendMessage(socket, objStr, recipient);
        }
        cout << endl;
    }
}

string getRandomRecipient(){
    string entities[3] = {"chunk0", "chunk1", "server"};
    int idx = rand() % 3;
    return entities[idx];
}
