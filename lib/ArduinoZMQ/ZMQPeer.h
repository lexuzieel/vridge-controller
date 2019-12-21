#ifndef ZMQPeer_H
#define ZMQPeer_H

#include <WiFiClient.h>
#include <ZMQMessage.h>
#include <ZMQSocket.h>

class ZMQSocket;

/**
 * ZMQ peer instance.
 */
class ZMQPeer
{
    friend ZMQSocket;

private:
    const char* _host;
    uint16_t _port;
    WiFiClient* _client;
    boolean _initialized = false;
    unsigned long _initTimestamp = millis();
    std::vector<ZMQMessage*>* _sendQueue;
    std::vector<ZMQMessage*>* _recvQueue;
    boolean _sendmore = false;

    void flushSendQueue();
    void flushRecvQueue();

public:
    static size_t sendQueueCapacity;
    static size_t recvQueueCapacity;

    ZMQPeer(const char* host,
            uint16_t port);
    virtual ~ZMQPeer();

    const char* getHost();
    uint16_t getPort();
    WiFiClient* getClient();

    void sendPush(ZMQMessage* message);
    ZMQMessage* sendPeek();
    ZMQMessage* sendPop();

    void flush();

    void recvPush(ZMQMessage* message);
    ZMQMessage* recvPeek();
    ZMQMessage* recvPop();

    boolean connect();
    boolean initialized();
};

#endif
