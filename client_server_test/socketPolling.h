#ifndef SOCKET_POLLING_H
#define SOCKET_POLLING_H

using namespace std;

struct socketPollingInfo {
    map < string, zmq::socket_t * > & sockets;
    map < string, zmq::pollitem_t > items;
};

#endif