#include "AtStream.h"

AtStream atStream(Serial1);

void setup() {
    Serial1.begin(9600);
}

void loop() {
    atStream.commandExecute("VERSION");
    atStream.wait();

    const char *versionResponse = atStream.getResponseData();

    AtStreamArgument arguments[2] = {
        {AtStreamArgumentType_String, "version"},
        {AtStreamArgumentType_Integer, nullptr, 2}
    };

    // Send command: AT+SETVER="version",2
    atStream.commandExecute("SETVER", arguments, 2);
    atStream.wait();
}
