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

struct socketPollingInfo {
    vector < zmq::socket_t * > sockets;
    vector < zmq::pollitem_t > items;
    zmq::socket_t * socket;
};

std::string executeScript(std::string script);
void * receiveData(void * arg);
void computeStats();
void clientFirstHandshake(zmq::socket_t * socket);
void sendGoSignal(vector < zmq::socket_t * > sockets);

string scriptName = "getServerAddress.sh";

int main () {
	
	std::string pipeau = executeScript("chmod +x " + scriptName + " && echo ok");
    std::string servAdress = executeScript("./" + scriptName);
    cout << "servAdress " << servAdress << endl;

    vector < zmq::socket_t * > sockets;
    vector<zmq::pollitem_t> items;
    zmq::context_t context (1);

    for(unsigned int i=0; i<NB_CHUNKS; i++){
        std::string address = "tcp://*: " + to_string(5555+i+1);
        cout << "Creating client " << i << " socket at: " << address << endl;
        
        zmq::socket_t * socket = new zmq::socket_t(context, ZMQ_PAIR);
        socket->bind(address.c_str());
        
        sockets.push_back(socket);
        zmq::pollitem_t item = {static_cast<void *>(*(sockets[i])), 0, ZMQ_POLLIN, 0};
        items.push_back(item);
    }

    //  Prepare our context and socket
    cout << "Creating handshake socket at tcp://*:5555\n" << endl;
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:5555");

    clientFirstHandshake(&socket);
    cout << "CLOSING HANDSHAKE SOCKET" << endl;
    socket.close();

    sendGoSignal(sockets);

    // Let's receive the incoming data from all the clients
    socketPollingInfo infos = {sockets, items};
    pthread_t receiveDataThread;
    pthread_create(&receiveDataThread, NULL, receiveData, (void*)(&infos));

    // Let's compute the stats
    computeStats();

    //close all sockets 

    return 0;
}

void sendGoSignal(vector < zmq::socket_t * > sockets){
    cout << "SENDING GO SIGNAL" << endl;
    for(unsigned int i=0; i<sockets.size(); i++){
        std::string signalStr = "GO";
        int size = signalStr.size();
        zmq::message_t signal(size);
        memcpy(signal.data (), signalStr.c_str(), size);
        sockets[i]->send(signal);      
    }
    cout << "DONE\n" << endl; 
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
    
    socketPollingInfo infos = *((socketPollingInfo *)arg);
    vector < zmq::pollitem_t > items = infos.items;
    vector < zmq::socket_t * > sockets = infos.sockets;
    
    // Poll through the messages
    while (1) {
        zmq::message_t message;
        int itemsSize = items.size();
        zmq::poll(items.data(), itemsSize, -1);

        for(int i=0; i<itemsSize; i++){
            // Check if that client sent a message
            if (items[i].revents & ZMQ_POLLIN) {
                sockets[i]->recv(&message);
                std::string rpl = std::string(static_cast<char*>(message.data()), message.size());
                std::cout << "Received \"" << rpl << "\" from " << i << std::endl;

                //  Send reply back to client
                std::string replyStr = "Obj received";
                int size = replyStr.size();
                zmq::message_t reply(size);
                memcpy(reply.data (), replyStr.c_str(), size);
                sockets[i]->send(reply);
            }
        }
        
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
