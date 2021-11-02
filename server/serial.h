#ifndef SERIAL_H
#define SERIAL_H

#define SERIAL_BAUD 115200
#define SERIAL_BUFFER_SIZE 64
#define SERIAL_ETHERNET_PORT 2222

extern uint8_t serial_run_state;

// set all highz in main
extern void init_serial_pins_highz();

extern void init_serial_server();
extern void stop_serial_server();
extern void serial_loop();

#endif
