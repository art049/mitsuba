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

#define NB_CHUNKS 2

using namespace std;

std::string executeScript(std::string script);
void * receiveData(void * arg);
void computeStats();
void clientFirstHandshake(zmq::socket_t * socket);

int main () {

    std::string servAdress = executeScript("./getServerAddress.sh");
    cout << "servAdress " << servAdress << endl; 

    vector < zmq::socket_t * > sockets;
    std::vector<zmq::pollitem_t> items;
    zmq::context_t context (1);
    for(unsigned int i=0; i<NB_CHUNKS; i++){
        std::string address = "tcp://*:" + to_string(5555+i+1);
        zmq::socket_t socket(context, ZMQ_PAIR);
        socket.bind (address.c_str());
        
        sockets.push_back(&socket);
        zmq::pollitem_t item = {static_cast<void *>(*(sockets[i])), 0, ZMQ_POLLIN, 0};
        items.push_back(item);
    }

    //  Initialize poll set
    /*std::vector<zmq::pollitem_t> items =  {
        {static_cast<void *>(*(sockets[0])), 0, ZMQ_POLLIN, 0},
        {static_cast<void *>(*(sockets[1])), 0, ZMQ_POLLIN, 0}
    };*/

    //  Prepare our context and socket
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:5555");

    clientFirstHandshake(&socket);

    // Let's receive the incoming data
    pthread_t receiveDataThread;
    pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&socket));

    // Let's compute the stats
    computeStats();

    return 0;
}

void clientFirstHandshake(zmq::socket_t * socket){
    cout << "WAITING FOR ALL " << NB_CHUNKS << " CLIENTS TO HANDSHAKE" << endl; 
    for(unsigned int i=0; i<NB_CHUNKS; i++){
        zmq::message_t request;

        //  Wait for the next client to connect
        socket->recv(&request);
        std::string rpl = std::string(static_cast<char*>(request.data()), request.size());

        if(rpl.compare("firstHandShake") == 0){
            //  Send the client its port number
            std::string replyStr = to_string(5555+i+1);
            int size = replyStr.size();
            zmq::message_t reply(size);
            memcpy(reply.data (), replyStr.c_str(), size);
            socket->send(reply);
            std::cout << "Assigned port number " << 5555 + i + 1 << std::endl;
        }else{
            cout << "ERROR, wrong handshake received" << endl;
        }
    }
    cout << "ALL CLIENTS WERE ASSIGNED A PORT NUMBER" << endl; 
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
        std::string replyStr = "Obj received";
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