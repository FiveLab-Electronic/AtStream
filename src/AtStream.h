#pragma once

#include <Stream.h> // @todo: remove as requirements (create proxy for read/write)
#include <Arduino.h> // @todo: remove as requirements (use only for debug)

#define AT_STREAM_INT_CHAR_LENGTH 6

#ifndef AT_STREAM_DEBUG
#define AT_STREAM_DEBUG 0
#endif

#ifndef AT_STREAM_RESPONSE_DATA_SIZE
#define AT_STREAM_RESPONSE_DATA_SIZE 256
#endif

#ifndef AT_STREAM_RESPONSE_LINE_SIZE
#define AT_STREAM_RESPONSE_LINE_SIZE 64
#endif

#define AT_STREAM_SET_BIT(flags, bit) \
    flags = (flags) | (bit);

#define AT_STREAM_CLEAR_BIT(flags, bit) \
    flags = (flags) & ~(bit);

#define AT_STREAM_ADD_BYTE_TO_RESPONSE_DATA(data, byte) \
    data.buffer[(data).size] = byte; \
    (data).size++;

#define AT_STREAM_CHECK_BUSY(flags) \
    if ((flags) & AtStreamFlags_WaitResponse) { \
        return AtStreamResult_Busy; \
    }

#if AT_STREAM_DEBUG
#define AT_STREAM_DEBUG_TX(l) \
    Serial.print(l);

#define AT_STREAM_DEBUG_RX(l) \
    Serial.print("<-- ");     \
    Serial.println(l);
#else
#define AT_STREAM_DEBUG_TX(l)
#define AT_STREAM_DEBUG_RX(l)
#endif


template<uint16_t S>
struct AtStreamBuffer {
    char buffer[S] = {};
    uint16_t size = 0;
};

enum AtStreamResult {
    AtStreamResult_Unknown = 0,
    AtStreamResult_Ok = 1,
    AtStreamResult_Error = 2,
    AtStreamResult_Fail = 3,
    AtStreamResult_Busy = 4,
    AtStreamResult_ErrorMemoryAllocation = 5,
    AtStreamResult_WrongCommand = 6
};

enum AtStreamFlags {
    AtStreamFlags_WaitResponse = (1 << 0),
    AtStreamFlags_ReceivedResponse = (1 << 1),
    AtStreamFlags_ReservedBit3 = (1 << 2),
    AtStreamFlags_ReservedBit4 = (1 << 3)
};

enum AtStreamArgumentType {
    AtStreamArgumentType_Integer,
    AtStreamArgumentType_String,
    AtStreamArgumentType_Null
};

struct AtStreamArgument {
    AtStreamArgumentType type;
    const char *strPtr;
    int intValue;
};

class AtStream {
public:
    explicit AtStream(Stream &stream) : _stream(stream) {}

    /**
     * Tick stream.
     * Returns true if we process any data or logic and false for IDLE.
     */
    virtual bool tick();

    /**
     * Is ready for send command to execution?
     */
    virtual bool ready();

    /**
     * Wait for ready to send next command.
     */
    void wait();

    /**
     * Get full response data.
     * Returns nullptr if response not received.
     */
    const char *getResponseData();

    /**
     * Get last command sends for execute.
     * Note: command can be not executed if error.
     */
    const char *getLastCommand();

    /**
     * Get last executed result.
     */
    AtStreamResult getLastResult();

    /**
     * Execute specific AT command.
     */
    AtStreamResult commandExecute(const char *command);

    /**
     * Execute specific AT command with arguments.
     */
    AtStreamResult commandExecute(const char *command, AtStreamArgument *arguments, uint8_t count);

    /**
     * Make arguments chars.
     * Returns pointer to allocated memory.
     */
    static char *makeArgumentsStr(const AtStreamArgument *args, uint8_t count);

    /**
     * Parse arguments from string
     */
    static AtStreamArgument *parseArgumentsStr(const char *buffer, uint16_t bytes, uint8_t &count);

    /**
     * Free memory by arguments ptr.
     */
    static void freeArguments(AtStreamArgument *&arguments, uint8_t count);

protected:
    Stream &_stream;

    AtStreamBuffer<AT_STREAM_RESPONSE_DATA_SIZE> _responseData;
    AtStreamBuffer<AT_STREAM_RESPONSE_LINE_SIZE> _responseLine;

    uint8_t _flags = 0;
    AtStreamResult _lastResult = AtStreamResult_Unknown;
    char *_lastCommand = nullptr;

    virtual void receiveNotRelatedLine();

    /**
     * Receive data from stream
     */
    void receive();

    /**
     * Send command with arguments
     */
    AtStreamResult sendCommand(const char *command, const char *argumentsLine);

    void write(const char *str);
    void write(char c);
};
