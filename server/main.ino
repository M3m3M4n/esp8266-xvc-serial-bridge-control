#include "command.h"
#include "serial.h"
#include "xvc.h"

void setup()
{
    delay(500);
    init_command_server();
    init_serial_pins_highz();
    init_xvc_pins_highz();
}
void loop()
{
    command_loop();
    serial_loop();
    xvc_loop();
}
