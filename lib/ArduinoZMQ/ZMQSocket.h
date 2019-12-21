#ifndef ZMQSocket_H
#define ZMQSocket_H

#include <Arduino.h>
#include <WiFiClient.h>

#define ZMQ_VERSION_1 1
#define ZMQ_VERSION_2 2

enum ZMQSocketType
{
    // PAIR = 0x00,
    // PUB = 0x01,
    // SUB = 0x02,
    REQ = 0x03,
    // REP = 0x04,
    // DEALER = 0x05,
    // ROUTER = 0x06,
    // PULL = 0x07,
    // PUSH = 0x08,
};

enum ZMQStatus
{
    ZMQ_STATUS_OK,
    ZMQ_STATUS_UNAVAILABLE,
};

enum ZMQFlags
{
    ZMQ_NOBLOCK = 0x01,
    ZMQ_SNDMORE = 0x02,
};

#include <ZMQMessage.h>
#include <ZMQPeer.h>

/**
 * ZMQ socket implementing ZMTP/2.0 protocol.
 */
class ZMQSocket
{
private:
    /**
     * TCP connection client.
     * This class handles data transmission.
     */
    WiFiClient* _client;
    ZMQSocketType _type;
    // boolean _connected = false;
    // boolean _initialized = false;
    uint8_t _zmq_version = 0;

    static std::vector<ZMQSocket*>* _sockets;
    std::vector<ZMQPeer*>* _peers;
    // std::vector<ZMQMessage*>* _sendQueue;

    static unsigned long _workerTimestamp;
    static size_t _workerSocketPointer;
    static size_t _workerPeerPointer;

    ZMQSocket(ZMQSocketType type, ZMQPeer* peer);

    template <typename Generic>
    void log(Generic text = "");
    void log();

    void initialize(ZMQPeer* peer);
    static ZMQStatus processSend(
        ZMQSocket* socket,
        ZMQPeer* peer,
        uint8_t flags = 0);
    static ZMQStatus processRecv(
        ZMQSocket* socket,
        ZMQPeer* peer,
        uint8_t flags = 0);
    static void process(
        ZMQSocket* socket,
        ZMQPeer* peer,
        uint8_t flags = 0);
    static boolean peerInitialized(ZMQSocket* socket, ZMQPeer* peer);
    boolean peerSend(ZMQPeer* peer, ZMQMessage* message, uint8_t flags = 0);
    ZMQMessage* peerRecv(ZMQPeer* peer);

public:
    virtual ~ZMQSocket();

    ZMQSocketType getType();
    static unsigned long peerReconnectInteval;

    static ZMQSocket* connect(ZMQSocketType type,
                              const char* host,
                              uint16_t port);
    void connect(const char* host,
                 uint16_t port);
    void flush();
    ZMQStatus send(const char* buffer, uint8_t flags = 0);
    ZMQStatus send(ZMQMessage* message, uint8_t flags = 0);
    ZMQMessage* recv(uint8_t flags = 0);
    static void work();
};

#endif
