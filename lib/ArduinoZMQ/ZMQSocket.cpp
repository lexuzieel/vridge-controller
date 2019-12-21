#include "ArduinoZMQ.h"

std::vector<ZMQSocket*>* ZMQSocket::_sockets = new std::vector<ZMQSocket*>();
unsigned long ZMQSocket::_workerTimestamp = millis();
size_t ZMQSocket::_workerSocketPointer = 0;
size_t ZMQSocket::_workerPeerPointer = 0;

unsigned long ZMQSocket::peerReconnectInteval = 2000;

template <typename Generic>
void ZMQSocket::log(Generic text)
{
    Serial.print("ZQM: ");
    Serial.println(text);
}

void ZMQSocket::log()
{
    log("");
}

ZMQSocket::ZMQSocket(ZMQSocketType type, ZMQPeer* peer)
{
    this->_type = type;
    // peer->getClient() = client;

    this->_peers = new std::vector<ZMQPeer*>();
    this->_peers->push_back(peer);

    // this->_sendQueue = new std::vector<ZMQMessage*>();

    // this->initialize();
}

ZMQSocket::~ZMQSocket()
{
    for (auto peer : *this->_peers)
    {
        peer->flush();
        delete peer;
    }

    this->_peers->clear();
    delete this->_peers;

    // ZMQSocket::_sockets->erase(
    //     std::remove(
    //         ZMQSocket::_sockets->begin(),
    //         ZMQSocket::_sockets->end(),
    //         this),
    //     ZMQSocket::_sockets->end());
}

/**
 * Flush pending message queues.
 */
void ZMQSocket::flush()
{
    for (auto peer : *this->_peers)
    {
        peer->flush();
    }
}

ZMQSocketType ZMQSocket::getType()
{
    return this->_type;
}

/**
 * Check whether socket is connected.
 */
// boolean ZMQSocket::connected()
// {
//     return this->_connected;
// }

/**
 * Connect specificed SOCKET TYPE to the HOST on PORT.
 * Provides reference to the connection SOCKET instance.
 */
ZMQSocket* ZMQSocket::connect(ZMQSocketType type,
                              const char* host,
                              uint16_t port)
{
    auto peer = new ZMQPeer(host, port);

    auto socket = new ZMQSocket(type, peer);

    // if (ZMQSocket::_sockets == NULL)
    // {
    //     ZMQSocket::_sockets = new std::vector<ZMQSocket*>();
    // }

    ZMQSocket::_sockets->push_back(socket);

    return socket;
}

void ZMQSocket::connect(const char* host,
                        uint16_t port)
{
    auto peer = new ZMQPeer(host, port);
    this->_peers->push_back(peer);
}

/**
 * Check if the given peer is initialized.
 */
boolean ZMQSocket::peerInitialized(ZMQSocket* socket, ZMQPeer* peer)
{
    if (!peer->initialized())
    {
        socket->initialize(peer);
    }
    else if (!peer->_client->connected())
    {
        peer->_initialized = false;
    }

    return peer->initialized();
}

/**
 * Process peer next send request.
 */
ZMQStatus ZMQSocket::processSend(
    ZMQSocket* socket,
    ZMQPeer* peer,
    uint8_t flags)
{
    if (ZMQSocket::peerInitialized(socket, peer))
    {
        auto message = peer->sendPeek();

        if (message != NULL)
        {
            if (socket->peerSend(peer, message, flags))
            {
                // Remove message from the queue if it has been sent.
                message = peer->sendPop();

                if (message != NULL)
                {
                    delete message;
                }
            }
        }

        return ZMQ_STATUS_OK;
    }

    return ZMQ_STATUS_UNAVAILABLE;
}

/**
 * Process peer next recv request.
 */
ZMQStatus ZMQSocket::processRecv(
    ZMQSocket* socket,
    ZMQPeer* peer,
    uint8_t flags)
{
    if (ZMQSocket::peerInitialized(socket, peer))
    {
        auto message = socket->peerRecv(peer);

        if (message != NULL)
        {
            peer->recvPush(message);
        }

        return ZMQ_STATUS_OK;
    }

    return ZMQ_STATUS_UNAVAILABLE;
}

/**
 * Process peer connection on the socket.
 */
void ZMQSocket::process(ZMQSocket* socket, ZMQPeer* peer, uint8_t flags)
{
    // Serial.printf("PEER %d: %s:%d INITIALIZED = %d\n",
    //               ZMQSocket::_workerPeerPointer,
    //               peer->_host,
    //               peer->_port,
    //               peer->initialized());
    ZMQSocket::processSend(socket, peer, flags);
    ZMQSocket::processRecv(socket, peer, flags);
    // /**
    //  * Peer work logic.
    //  */
    // if (!peer->initialized())
    // {
    //     socket->initialize(peer);
    // }
    // else if (!peer->_client->connected())
    // {
    //     peer->_initialized = false;
    // }

    // if (peer->initialized())
    // {
    //     Serial.println("process initialized");
    //     ZMQMessage* message;

    //     // Process output.
    //     message = peer->sendPeek();

    //     if (message != NULL)
    //     {
    //         Serial.println("has send message");
    //         // if (socket->peerSend(peer, message, flags))
    //         // {
    //         //     // Remove message from the queue if it has been sent.
    //         //     message = peer->sendPop();

    //         //     if (message != NULL)
    //         //     {
    //         //         delete message;
    //         //     }
    //         // }
    //     }

    //     // Process input.
    //     message = socket->peerRecv(peer);

    //     if (message != NULL)
    //     {
    //         Serial.println("has recv message");
    //         peer->recvPush(message);
    //     }

    //     return ZMQ_STATUS_OK;
    // }

    // return ZMQ_STATUS_UNAVAILABLE;
}

/**
 * Work on the next socket.
 * Use this method in the main loop to run ZMQ in async manner.
 */
void ZMQSocket::work()
{
    if (millis() - ZMQSocket::_workerTimestamp <= 1000)
    {
        // Wait until next work cycle
        return;
    }

    ZMQSocket::_workerTimestamp = millis();

    // Reset socket pointer if out of bounds.
    if (ZMQSocket::_workerSocketPointer >= ZMQSocket::_sockets->size())
    {
        ZMQSocket::_workerSocketPointer = 0;
    }

    if (ZMQSocket::_sockets->size() > 0)
    {
        auto socket = ZMQSocket::_sockets->at(ZMQSocket::_workerSocketPointer);

        // Serial.printf("SOCKET %d\n", ZMQSocket::_workerSocketPointer);

        // Reset peer pointer if out of bounds.
        if (ZMQSocket::_workerPeerPointer >= socket->_peers->size())
        {
            ZMQSocket::_workerPeerPointer = 0;
        }

        if (socket->_peers->size() > 0)
        {
            auto peer = socket->_peers->at(ZMQSocket::_workerPeerPointer);

            ZMQSocket::process(socket, peer);

            ZMQSocket::_workerPeerPointer++;
        }

        // Advance socker pointer.
        if (ZMQSocket::_workerPeerPointer >= socket->_peers->size())
        {
            ZMQSocket::_workerPeerPointer = 0;
            ZMQSocket::_workerSocketPointer++;
        }
    }
    else
    {
        ZMQSocket::_workerSocketPointer++;
    }
}

/**
 * Try to send given buffer over the socket to each peer.
 * Returns ZMQ_STATUS_UNAVAILABLE if at least one peer was unavailable.
 */
ZMQStatus ZMQSocket::send(const char* buffer, uint8_t flags)
{
    return ZMQSocket::send(new ZMQMessage(buffer), flags);
}

/**
 * Try to send given message over the socket to each peer.
 * Returns ZMQ_STATUS_UNAVAILABLE if at least one peer was unavailable.
 */
ZMQStatus ZMQSocket::send(ZMQMessage* buffer, uint8_t flags)
{
    ZMQStatus status = ZMQ_STATUS_OK;

    switch (this->_type)
    {
        case REQ:
        {
            /**
             * REQ socket send implements the following:
             * - SHALL route outgoing messages to connected peers using
             *   a round-robin strategy
             * - SHALL block on sending, or return a suitable error,
             *   when it has no connected peers
             * - SHALL NOT discard messages that it cannot send to
             *   a connected peer
             */
            for (auto peer : *this->_peers)
            {
                peer->sendPush(buffer);
                if (this->processSend(this, peer, flags) == ZMQ_STATUS_UNAVAILABLE)
                {
                    status = ZMQ_STATUS_UNAVAILABLE;
                }
            }
            break;
        }
    }

    return status;
}

/**
 * Recieve next message.
 */
ZMQMessage* ZMQSocket::recv(uint8_t flags)
{
    switch (this->_type)
    {
        case REQ:
        {
            if ((flags & ZMQ_NOBLOCK) != 0)
            {
                ZMQMessage* message = NULL;

                for (auto peer : *this->_peers)
                {
                    // Try to pop pending message from every peer.
                    auto temp = peer->recvPop();

                    if (temp != NULL && message == NULL)
                    {
                        // Store only the first one.
                        message = temp;
                    }
                }

                return message;
            }
            else
            {
                while (true)
                {
                    ZMQMessage* message = NULL;

                    for (auto peer : *this->_peers)
                    {
                        this->processRecv(this, peer);

                        // Try to pop pending message from every peer.
                        auto temp = peer->recvPop();

                        if (temp != NULL && message == NULL)
                        {
                            // Store only the first one.
                            message = temp;
                        }
                    }

                    // Return only if there is a message, block otherwise.
                    if (message != NULL)
                    {
                        return message;
                    }

                    // This is a blocking operation.
                    delay(1);
                }
            }
            break;
        }
    }

    return NULL;
}

/**
 * Initialize connection to the peer.
 */
void ZMQSocket::initialize(ZMQPeer* peer)
{
    if (millis() - peer->_initTimestamp <= ZMQSocket::peerReconnectInteval)
    {
        return;
    }

    peer->_initTimestamp = millis();

    if (!peer->connect())
    {
        peer->_initialized = false;
        return;
    }

    char buffer;
    size_t bufferCursor;

    char signature[] = {
        // signature
        0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
        // revision
        0x01,
        // socket-type
        this->_type,
        // identity
        0x00, 0x00
        //
    };

#ifdef ZMQ_DEBUG
    log("REQUEST");
    for (uint8_t i = 0; i < sizeof(signature); i++)
    {
        Serial.printf("%02X", signature[i]);
    }
    Serial.println();
#endif

    peer->getClient()->write(signature, sizeof(signature));

    /**
     * Signature
     */
#ifdef ZMQ_DEBUG
    log("SIGNATURE");
#endif
    bufferCursor = 0;
    while (bufferCursor < 10)
    {
        if (peer->getClient()->readBytes(&buffer, 1) != 0)
        {
#ifdef ZMQ_DEBUG
            Serial.printf("%02X", buffer);
            if (bufferCursor == 9 && buffer != 0x7F)
            {
                Serial.println();
                Serial.printf(

                    "ZMQ: Invalid signature format.\n"

                );
            }
#endif
            bufferCursor++;
        }
        else
        {
            break;
        }
    }

#ifdef ZMQ_DEBUG
    Serial.println();
#endif

    /**
     * Revision
     */
    peer->getClient()->readBytes(&buffer, 1);

#ifdef ZMQ_DEBUG
    log("REVISION");
    Serial.printf("%02X\n", buffer);
#endif

    /**
     * Socket-type
     */
    peer->getClient()->readBytes(&buffer, 1);

#ifdef ZMQ_DEBUG
    log("SOCKET-TYPE");
    Serial.printf("%02X\n", buffer);
#endif

    switch (this->_type)
    {
        case ZMQSocketType::REQ:
        {
            if (buffer != 0x04)
            {
                Serial.printf(

                    "ZMQ: Invalid socket type for socket REQ: only REP allowed.\n"

                );
                return;
            }
            break;
        }
    }

    /**
     * Identity
     */
    peer->getClient()->readBytes(&buffer, 1); // 0x00
    peer->getClient()->readBytes(&buffer, 1); // Number of bytes?

    uint8_t identityLength = buffer;

#ifdef ZMQ_DEBUG
    log("IDENTITY");
#endif
    bufferCursor = 0;
    while (bufferCursor < identityLength)
    {
        if (peer->getClient()->readBytes(&buffer, 1) != 0)
        {
#ifdef ZMQ_DEBUG
            Serial.printf("%02X", buffer);
#endif
            bufferCursor++;
        }
        else
        {
            break;
        }
    }

#ifdef ZMQ_DEBUG
    Serial.println();
#endif

    peer->_initialized = true;
}

/**
 * Send message to the peer.
 * For internal use only.
 */
boolean ZMQSocket::peerSend(ZMQPeer* peer, ZMQMessage* message, uint8_t flags)
{
    if (peer->_initialized)
    {
        // delay(100);
        std::vector<char>* frame;

        if (!peer->_sendmore)
        {
            frame = new std::vector<char>();

            // empty frame
            frame->push_back(0x01); // flags: LONG = 0, MORE = 1
            frame->push_back(0x00); // length: 0

            peer->getClient()->write(frame->data(), frame->size());

            delete frame;
        }

        // return true;

        // Set peer SENDMORE flag for further continous sending.
        peer->_sendmore = (flags & ZMQ_SNDMORE) != 0;

        frame = new std::vector<char>();

        auto length = message->size;

        uint8_t flagsByte = 0x00;

        if (peer->_sendmore)
        {
            flagsByte |= 0x01;
        }

        if (length < 0xFF)
        {
            frame->push_back(flagsByte);
            frame->push_back(length);
        }
        else
        {
            flagsByte |= 0x02; // LONG = 1
            frame->push_back(flagsByte);

            for (int i = 7; i >= 0; i--)
            {
                if (i > 3)
                {
                    frame->push_back(0x00);
                }
                else
                {
                    // Decompose only the last 4 bytes (32 bits) of length
                    frame->push_back((length >> (i * 8)) & 0xFF);
                }
            }
        }

        std::copy(message->buffer,
                  message->buffer + message->size,
                  std::back_inserter(*frame));
        //     delete frame;

        // return false;

        // while (peer->getClient()->availableForWrite() < frame->size())
        // {
        //     // Wait?
        // }

        // Serial.println(frame->data());
        // Serial.println(frame->size());
        // peer->getClient()->setNoDelay(true);

        // uint8_t buffer[frame->size()];
        // for (size_t i = 0; i < frame->size(); i++)
        // {
        //     buffer[i] = frame->at(i);
        // }
        // TODO: Fix crash
        peer->getClient()->write(frame->data(), frame->size());
        // peer->getClient()->write(message->buffer, message->size);
        // peer->getClient()->write("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 32);

#ifdef ZMQ_DEBUG
        log("SEND");
        for (uint8_t i = 0; i < frame->size(); i++)
        {
            Serial.printf("%02X", frame->at(i));
        }
        Serial.println();
#endif

        // delete content;
        delete frame;

        return true;
    }

    return false;
}

/**
 * Recieve message from the peer.
 * For internal use only.
 */
ZMQMessage* ZMQSocket::peerRecv(ZMQPeer* peer)
{
    if (peer->_initialized)
    {
        if (peer->getClient()->available() == 0)
        {
            return NULL;
        }

#ifdef ZMQ_DEBUG
        log("RECV");
#endif
        /**
         * Check for malformed empty frame.
         */
        char byteBuffer;
        boolean malformed = true;

        peer->getClient()->readBytes(&byteBuffer, 1);
        if (byteBuffer == 0x01)
        {
            peer->getClient()->readBytes(&byteBuffer, 1);
            if (byteBuffer == 0x00)
            {
                malformed = false;
            }
        }

        if (malformed)
        {
#ifdef ZMQ_DEBUG
            log("ERROR: Malformed frame or empty stream.");
#endif
            peer->getClient()->flush();
            return NULL;
        }

        boolean longFlag;
        boolean moreFlag;
        size_t frameLength = 0;
        size_t bufferCursor = 0;

        // Read the message.

        if (peer->getClient()->readBytes(&byteBuffer, 1) != 0)
        {
            moreFlag = (byteBuffer & 0x01) > 0;
            longFlag = (byteBuffer & 0x02) > 0;

#ifdef ZMQ_DEBUG
            log("FRAME START");
            Serial.printf("MORE = %d\n", moreFlag);
            Serial.printf("LONG = %d\n", longFlag);
#endif

            if (longFlag == false)
            {
                peer->getClient()->readBytes(&byteBuffer, 1);
                frameLength = byteBuffer;
            }
            else
            {
                for (uint8_t i = 1; i <= 8; i++)
                {
                    peer->getClient()->readBytes(&byteBuffer, 1);
                    frameLength |= byteBuffer << ((8 - i) * 8);
                }
            }

#ifdef ZMQ_DEBUG
            Serial.printf("LENGTH = %d\n", frameLength);
#endif

            auto message = new ZMQMessage(new uint8_t[frameLength](),
                                          frameLength);

            bufferCursor = 0;
            while (bufferCursor < frameLength)
            {
                if (peer->getClient()->readBytes(&byteBuffer, 1) != 0)
                {
#ifdef ZMQ_DEBUG
                    if (frameLength <= 1024)
                    {
                        Serial.printf("%02X", byteBuffer);
                    }
#endif

                    message->buffer[bufferCursor] = byteBuffer;
                    bufferCursor++;
                }
                else
                {
                    break;
                }
            }

#ifdef ZMQ_DEBUG
            Serial.println();
            log("FRAME END");
#endif

            return message;
        }
    }

    return NULL;
}
