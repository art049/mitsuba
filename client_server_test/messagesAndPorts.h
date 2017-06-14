#ifndef PORT_NUMBER_H
#define PORT_NUMBER_H

static const int serverPortNumber = 5555;
static const int handshakePortNumber = 5556;

void sendMessage(zmq::socket_t * socket, std::string messageStr, std::string recipient){

    int size = recipient.size();
    zmq::message_t idMessage(size);
    memcpy (idMessage.data(), recipient.c_str(), size);
    socket->send (idMessage);

    size = messageStr.size();
    zmq::message_t message (size);
    memcpy (message.data (), messageStr.c_str(), size);
    socket->send(message);

    std::cout << "Sending " << messageStr << " to " << recipient << std::endl;
}

void receiveMessageRouter(zmq::socket_t * socket, zmq::message_t * message, std::string & recipient, std::string sender){

    // Get the id of the person it is supposed to get to
    socket->recv(message);
    recipient = std::string(static_cast<char*>(message->data()), message->size());
    // Get the actual message for it
    socket->recv(message);

    // Debug only
    std::string rpl = std::string(static_cast<char*>(message->data()), message->size());
    std::cout << "Received message " << rpl << " from " << sender << " to " << recipient << std::endl;
}

#endif