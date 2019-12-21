#ifndef ZMQMessage_H
#define ZMQMessage_H

/**
 * ZMQ message wrapper.
 */
class ZMQMessage
{
public:
    uint8_t* buffer;
    size_t size;

    /**
     * Create message from raw byte array of specified size.
     */
    ZMQMessage(uint8_t buffer[], size_t size)
    {
        this->buffer = buffer;
        this->size = size;
    }

    /**
     * Create message from string literal.
     * NOTE: Only suitable for text transfer because of null-termination!
     */
    ZMQMessage(const char* buffer)
    {
        this->buffer = (uint8_t*)buffer;
        this->size = strlen(buffer);
    }

    const char* c_str()
    {
        return (const char*)this->buffer;
    }
};

#endif
