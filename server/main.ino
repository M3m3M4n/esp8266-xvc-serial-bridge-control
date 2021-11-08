#include "command.h"

CommandServer<CommandPort<COMMAND_RST_PIN, COMMAND_BOOTMODE_CONTROL_PIN, COMMAND_BOOTMODE_SELECTOR_PIN>> command_server(COMMAND_PORT);

void setup()
{
    delay(500);
    command_server.setup();
}

void loop()
{
    command_server.handle();
}
