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

#endif