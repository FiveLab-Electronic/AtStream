#include "AtStream.h"

bool AtStream::tick() {
    if (_stream.available()) {
        receive();

        return true;
    }

    return false;
}

bool AtStream::ready() {
    tick();

    if (_flags & AtStreamFlags_WaitResponse) {
        return false;
    }

    return true;
}

void AtStream::wait() {
    while (!ready());
}

const char *AtStream::getResponseData() {
    if (!(_flags & AtStreamFlags_ReceivedResponse)) {
        return nullptr;
    }

    return _responseData.buffer;
}

const char *AtStream::getLastCommand() {
    return _lastCommand;
}

AtStreamResult AtStream::getLastResult() {
    return _lastResult;
}

AtStreamResult AtStream::commandExecute(const char *command) {
    return sendCommand(command, nullptr);
}

AtStreamResult AtStream::commandExecute(const char *command, AtStreamArgument *arguments, uint8_t count) {
    char *argumentsLine = AtStream::makeArgumentsStr(arguments, count);

    AtStreamResult result = sendCommand(command, argumentsLine);

    free(argumentsLine);

    return result;
}

char *AtStream::makeArgumentsStr(const AtStreamArgument *args, uint8_t count) {
    auto atCommand = (char *) malloc(sizeof(char) * 1);
    uint16_t atCommandSize = 0;

    for (uint8_t i = 0; i < count; i++) {
        AtStreamArgument argument = args[i];

        if (argument.type == AtStreamArgumentType_Integer) {
            auto intInStr = (char *) malloc(sizeof(char) * (AT_STREAM_INT_CHAR_LENGTH + 1));

            if (!intInStr) {
                free(atCommand);

                return nullptr;
            }

            itoa((int) argument.intValue, intInStr, 10);

            auto intInStrSize = strlen(intInStr);

            atCommand = (char *) realloc(atCommand, atCommandSize + intInStrSize);

            if (!atCommand) {
                free(atCommand);

                return nullptr;
            }

            memcpy(&atCommand[atCommandSize], intInStr, intInStrSize);

            free(intInStr);

            atCommandSize += intInStrSize;
        } else if (argument.type == AtStreamArgumentType_String) {
            size_t strSize = strlen(argument.strPtr);

            atCommand = (char *) realloc(atCommand, atCommandSize + strSize + 2); // Use double quotes on start and end.

            if (!atCommand) {
                free(atCommand);

                return nullptr;
            }

            atCommand[atCommandSize] = '"';
            memcpy(&atCommand[atCommandSize + 1], argument.strPtr, strSize);
            atCommand[atCommandSize + strSize + 1] = '"';

            atCommandSize += strSize + 2;
        } else if (argument.type == AtStreamArgumentType_Null) {
            // Nothing to do.
        }

        if ((i + 1) != count) {
            atCommand = (char *) realloc(atCommand, atCommandSize + 1);

            if (!atCommand) {
                free(atCommand);

                return nullptr;
            }

            atCommand[atCommandSize] = ',';
            atCommandSize++;
        }
    }

    atCommand[atCommandSize] = '\0';

    return atCommand;
}

AtStreamArgument *AtStream::parseArgumentsStr(const char *buffer, uint16_t bytes, uint8_t &count) {
    if (!bytes) {
        return nullptr;
    }

    AtStreamArgument *arguments = nullptr;
    AtStreamArgument argument = {};

    uint16_t argumentStartPosition = 0;
    uint16_t argumentLength = 0;

    for (uint16_t i = 0; i < bytes; i++) {
        if (buffer[i] == ',' || (i + 1) == bytes) {
            // Found argument separator. Parse.
            if (buffer[argumentStartPosition] == '"' && buffer[i - 1] == '"') {
                // String argument.
                uint16_t strLength = (i - 1) - (argumentStartPosition + 1);
                char *strPtr = (char *) malloc(sizeof(char) * strLength + 1); // Terminate char to end

                if (!strPtr) {
                    return nullptr;
                }

                memcpy(strPtr, &buffer[argumentStartPosition + 1], strLength);
                strPtr[strLength] = '\0';

                argument = {AtStreamArgumentType_String, strPtr, 0};
            } else if (!argumentLength) {
                // Null argument.
                argument = {AtStreamArgumentType_Null, nullptr, 0};
            } else {
                // Numeric argument.
                uint16_t numLength = i - argumentStartPosition;
                char *numPtr = (char *) malloc(sizeof(char) * (AT_STREAM_INT_CHAR_LENGTH + 1));

                if (!numPtr) {
                    return nullptr;
                }

                memcpy(numPtr, &buffer[argumentStartPosition], numLength);
                numPtr[6] = '\0';


                int intValue = atoi(numPtr);
                free(numPtr);
                argument = {AtStreamArgumentType_Integer, nullptr, intValue};
            }

            arguments = (AtStreamArgument *) realloc(arguments, sizeof(AtStreamArgument) * (count + 1));
            arguments[count] = argument;

            count++;

            argumentStartPosition = i + 1;
            argumentLength = 0;

            continue;
        }

        argumentLength++;
    }

    return arguments;
}

void AtStream::freeArguments(AtStreamArgument *&arguments, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (arguments[i].strPtr) {
            free((char *) arguments[i].strPtr);
        }
    }

    free(arguments);
    arguments = nullptr;
}

AtStreamResult AtStream::sendCommand(const char *command, const char *argumentsLine) {
    free(_lastCommand);
    _lastCommand = strdup(command);

    AT_STREAM_CHECK_BUSY(_flags);
    AT_STREAM_SET_BIT(_flags, AtStreamFlags_WaitResponse);
    AT_STREAM_CLEAR_BIT(_flags, AtStreamFlags_ReceivedResponse);

    _lastResult = AtStreamResult_Unknown;

    AT_STREAM_DEBUG_TX("--> ");

    write("AT+");
    write(command);

    if (nullptr != argumentsLine) {
        write('=');
        write(argumentsLine);
    }

    write("\r\n");

    return AtStreamResult_Ok;
}

void AtStream::receive() {
    while (_stream.available()) {
        char byte = (char) _stream.read();

        if (byte == '\r') {
            // Ignore
            continue;
        }

        AT_STREAM_ADD_BYTE_TO_RESPONSE_DATA(_responseData, byte);

        if (byte == '\n') {
            AT_STREAM_ADD_BYTE_TO_RESPONSE_DATA(_responseLine, '\0');

            _responseLine.size = 0;

            AT_STREAM_DEBUG_RX(_responseLine.buffer);

            bool finish = false;

            if (strcmp(_responseLine.buffer, "OK") == 0) {
                // Finish process with OK state.
                finish = true;

                _lastResult = AtStreamResult_Ok;
            }

            if (strcmp(_responseLine.buffer, "ERROR") == 0) {
                // Finish process with error state.
                finish = true;

                _lastResult = AtStreamResult_Error;
            }

            if (strcmp(_responseLine.buffer, "FAIL") == 0) {
                // Finish process with fail state.
                finish = true;

                _lastResult = AtStreamResult_Fail;
            }

            if (finish) {
                AT_STREAM_ADD_BYTE_TO_RESPONSE_DATA(_responseData, '\0');
                _responseData.size = 0;

                AT_STREAM_SET_BIT(_flags, AtStreamFlags_ReceivedResponse);
                AT_STREAM_CLEAR_BIT(_flags, AtStreamFlags_WaitResponse);

                return;
            }

            continue;
        }

        AT_STREAM_ADD_BYTE_TO_RESPONSE_DATA(_responseLine, byte);
    }
}

void AtStream::write(const char *str) {
    AT_STREAM_DEBUG_TX(str);

    _stream.write(str);
}

void AtStream::write(char c) {
    AT_STREAM_DEBUG_TX(c);

    _stream.write(c);
}
