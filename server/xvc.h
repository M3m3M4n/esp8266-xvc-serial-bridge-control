#ifndef XVC_H
#define XVC_H

#define XVC_MAX_WRITE_SIZE 512
#define XVC_ERROR_OK 1
#define XVC_ETHERNET_PORT 2542

extern uint8_t xvc_run_state;

// set all highz in main
extern void init_xvc_pins_highz();

extern void init_xvc_server();
extern void stop_xvc_server();
extern void xvc_loop();

#endif
