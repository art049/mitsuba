#include <zmq.hpp>
#include <string>
#include <iostream>
#include <cstdio>
#include <sstream>
#include <map>

#include "messagesAndPorts.h"

using namespace std;

struct socketPollingInfo {
    map < string, zmq::socket_t * > & sockets;
    map < string, zmq::pollitem_t > items;
};

void clientFirstHandshake(socketPollingInfo & infos, zmq::socket_t * handshakeSocket, int nbChunks, zmq::context_t * context);
void sendGoSignal(map < string, zmq::socket_t * > sockets);
void routeMessages(socketPollingInfo & infos);

string executeScript(string script);
string scriptName = "getAddress.sh";

int main (int argc, char * argv[]) {

    string servAddr;
    int nbChunks;
    if(argc != 3){
        cout << "ERROR: please pass the server address and number of clients as arguments" << endl;
        return -1;
    }else{
        ostringstream oss;
        oss << "tcp://" << argv[1] << ":" << serverPortNumber;
        servAddr = oss.str(); 
        nbChunks = atoi(argv[2]);
    }

    map < string, zmq::socket_t * > sockets;
    map < string, zmq::pollitem_t > items;
    socketPollingInfo infos = {sockets, items};
    
    string tmp = executeScript("chmod +x " + scriptName + " && echo ok");
    string routerAdress = executeScript("./" + scriptName);
    cout << "routerAddress " << routerAdress << endl;

    //  Prepare our context and server socket
    cout << "Connecting to server socket at " << servAddr << endl;
    zmq::context_t context (1);
    zmq::socket_t * serverSocket = new zmq::socket_t(context, ZMQ_PAIR);
    serverSocket->connect(servAddr.c_str());
    sockets["server"] = serverSocket;
    zmq::pollitem_t item = {static_cast<void *>(*(serverSocket)), 0, ZMQ_POLLIN, 0};
    items["server"] = item;

    // Sending our address to the server
    cout << "Sending our address to the server" << endl;
    int size = routerAdress.size();
    zmq::message_t message(size);
    memcpy(message.data (), routerAdress.c_str(), size);
    serverSocket->send(message);

    // Getting answer from server
    zmq::message_t servReply;
    serverSocket->recv(&servReply);
    string replyStr = string(static_cast<char*>(servReply.data()), servReply.size());
    cout << "Server answered \"" << replyStr << "\"\n" << endl;

    /*  Prepare our handshake socket for clients to connect
        This could be port SERVER_PORT but if you're using localhost it's already in use
    */
    cout << "Creating handshake socket at tcp://*:" << handshakePortNumber << endl;
    zmq::socket_t handshakeSocket (context, ZMQ_REP);
    ostringstream oss;
    oss << "tcp://*:" << handshakePortNumber;
    handshakeSocket.bind (oss.str());
    oss.str("");

    // Open a dedicated socket to every client and make sur they wait for everyone to be connected
    clientFirstHandshake(infos, &handshakeSocket, nbChunks, &context);
    cout << "Closing handshake socket" << endl;
    handshakeSocket.close();
    // Tell everyone to start computing stuff
    sendGoSignal(sockets);

    routeMessages(infos);

    return 0;
}


void routeMessages(socketPollingInfo & infos){
    
    map < string, zmq::pollitem_t > items = infos.items;
    map < string, zmq::socket_t * > sockets = infos.sockets;
    vector < zmq::pollitem_t > itemsValue;
    vector < string > itemsKey;

    for( map < string, zmq::pollitem_t >::iterator it = items.begin(); it != items.end(); ++it ) {
        itemsKey.push_back( it->first );
        itemsValue.push_back( it->second );
    }
    
    // Poll through the messages
    while (1) {
        zmq::message_t message;
        string recipient;
        int itemsSize = items.size();
        zmq::poll(itemsValue.data(), itemsSize, -1);

        for(int i=0; i<itemsValue.size(); i++) {
            // Check if that client sent a message
            if (itemsValue[i].revents & ZMQ_POLLIN) {
                // Get the message
                receiveMessageRouter(sockets[itemsKey[i]], &message, recipient, itemsKey[i]);
                // Transfer it to the person supposed to get it
                sockets[recipient]->send(message);
                std::cout << "Forwarding message to " << recipient << std::endl;
            }
        }
        
    }
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

void clientFirstHandshake(socketPollingInfo & infos, zmq::socket_t * handshakeSocket, int nbChunks, zmq::context_t * context){

    cout << "Waiting for all " << nbChunks << " clients to handshake" << endl; 

    for(unsigned int i=1; i<=nbChunks; i++){
        zmq::message_t request;

        // This should be the id of the client
        handshakeSocket->recv(&request);
        string replyStr = string(static_cast<char*>(request.data()), request.size()).c_str();

        string rpl = replyStr.substr(0, replyStr.find("-"));
        string idStr = replyStr.substr(replyStr.find("-")+1, replyStr.size());

        if(rpl.compare("firstHandShake") == 0){
            // Create unique port number
            ostringstream oss;
            oss << handshakePortNumber+i;
            string portNumberStr = oss.str();

            // Bind associated socket
            zmq::socket_t * socket = new zmq::socket_t(*context, ZMQ_PAIR);
            string header("tcp://*: ");
            socket->bind(header + portNumberStr.c_str());
            infos.sockets[idStr] = socket;
            zmq::pollitem_t item = {static_cast<void *>(*(infos.sockets[idStr])), 0, ZMQ_POLLIN, 0};
            infos.items[idStr] = item;

            int size = portNumberStr.size();
            zmq::message_t reply(size);
            memcpy(reply.data (), portNumberStr.c_str(), size);
            handshakeSocket->send(reply);
            cout << "Assigned port number " << handshakePortNumber + i << " and created associated socket"<< endl;
        }else{
            cout << "ERROR, wrong handshake received" << endl;
        }

    }
    cout << "All clients were assigned a port number" << endl;
}

void sendGoSignal(map < string, zmq::socket_t * > sockets){
    cout << "Sending go signal: ";
    for( map < string, zmq::socket_t * >::iterator it = sockets.begin(); it != sockets.end(); ++it ) {
        string signalStr = "GO";
        int size = signalStr.size();
        zmq::message_t signal(size);
        memcpy(signal.data (), signalStr.c_str(), size);
        sockets[it->first]->send(signal);      
    }
    cout << "Done\n" << endl; 
}