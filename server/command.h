#ifndef COMMAND_H
#define COMMAND_H

#define COMMAND_SEND_BUFFER_SIZE 9
#define COMMAND_ETHERNET_PORT 42069

extern void init_command_server();
extern void command_loop();

#endif
