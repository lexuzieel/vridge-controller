#include <ArduinoZMQ.h>
#include <ZMQPeer.h>

size_t ZMQPeer::sendQueueCapacity = 16;
size_t ZMQPeer::recvQueueCapacity = 16;

ZMQPeer::ZMQPeer(const char* host,
                 uint16_t port)
{
    this->_host = host;
    this->_port = port;
    this->_client = new WiFiClient();
    this->_client->setSync(true);
    this->_client->setTimeout(5000);
    this->_sendQueue = new std::vector<ZMQMessage*>();
    this->_recvQueue = new std::vector<ZMQMessage*>();

    this->_initTimestamp = millis() - ZMQSocket::peerReconnectInteval;

    this->connect();
}

ZMQPeer::~ZMQPeer()
{
    // while (this->_client->connected())
    // {
    //     Serial.println("Flushing...");
    //     this->_client->flush(1000);
    //     // Wait for the client to disconnect.
    //     delay(1);
    // }

    this->_client->stop();
    delete this->_client;

    this->flushSendQueue();

    delete this->_sendQueue;

    this->flushRecvQueue();

    delete this->_recvQueue;
}

void ZMQPeer::flushSendQueue()
{
    for (auto message : *this->_sendQueue)
    {
        delete message;
    }

    this->_sendQueue->clear();
}

void ZMQPeer::flushRecvQueue()
{
    for (auto message : *this->_recvQueue)
    {
        delete message;
    }

    this->_recvQueue->clear();
}

/**
 * Flush pending message queues.
 */
void ZMQPeer::flush()
{
    this->flushSendQueue();
    this->flushRecvQueue();
}

const char* ZMQPeer::getHost()
{
    return this->_host;
}

uint16_t ZMQPeer::getPort()
{
    return this->_port;
}

WiFiClient* ZMQPeer::getClient()
{
    return this->_client;
}

/**
 * Push message onto the send queue.
 */
void ZMQPeer::sendPush(ZMQMessage* message)
{
    if (this->_sendQueue->size() > 0 &&
        this->_sendQueue->size() >= ZMQPeer::sendQueueCapacity)
    {
        // Free memory
        delete this->_sendQueue->front();
        // Erase last message in the queue.
        this->_sendQueue->erase(this->_sendQueue->begin());
    }

    this->_sendQueue->push_back(message);
}

/**
 * Peek next message buffer from the send queue.
 * Returns NULL if queue is empty.
 */
ZMQMessage* ZMQPeer::sendPeek()
{
    if (this->_sendQueue->size() > 0)
    {
        return this->_sendQueue->front();
    }

    return NULL;
}

/**
 * Remove next message buffer from the send queue.
 * Returns NULL if queue is empty.
 */
ZMQMessage* ZMQPeer::sendPop()
{
    auto buffer = this->sendPeek();

    if (buffer != NULL)
    {
        // Erase last message in the queue, but DO NOT free the memory.
        this->_sendQueue->erase(this->_sendQueue->begin());
    }

    return buffer;
}

/**
 * Push message onto the recv queue.
 */
void ZMQPeer::recvPush(ZMQMessage* message)
{
    if (this->_recvQueue->size() > 0 &&
        this->_recvQueue->size() >= ZMQPeer::recvQueueCapacity)
    {
        // Free memory
        delete this->_recvQueue->front();
        // Erase last message in the queue.
        this->_recvQueue->erase(this->_recvQueue->begin());
    }

    this->_recvQueue->push_back(message);
}

/**
 * Peek next message buffer from the recv queue.
 * Returns NULL if queue is empty.
 */
ZMQMessage* ZMQPeer::recvPeek()
{
    if (this->_recvQueue->size() > 0)
    {
        return this->_recvQueue->front();
    }

    return NULL;
}

/**
 * Remove next message buffer from the recv queue.
 * Returns NULL if queue is empty.
 */
ZMQMessage* ZMQPeer::recvPop()
{
    auto buffer = this->recvPeek();

    if (buffer != NULL)
    {
        // Free memory
        // delete this->_recvQueue->front();
        // Erase last message in the queue, but DO NOT free the memory.
        this->_recvQueue->erase(this->_recvQueue->begin());
    }

    return buffer;
}

boolean ZMQPeer::connect()
{
    if (this->_client->connected())
    {
#ifdef ZMQ_DEBUG
        Serial.printf("ZMQPeer: Already connected to %s:%d.\n", this->_host, this->_port);
#endif
        return true;
    }

    if (!this->_client->connect(this->_host, this->_port))
    {
#ifdef ZMQ_DEBUG
        Serial.printf("ZMQPeer: Host %s:%d is unavailable.\n", this->_host, this->_port);
#endif
        return false;
    }

    return true;
}

boolean ZMQPeer::initialized()
{
    return this->_initialized;
}
