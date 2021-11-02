#include "common.h"
#include "serial.h"

// serial bridge config
// ethernet side
static WiFiServer serial_server(SERIAL_ETHERNET_PORT);
static WiFiClient serial_client;
// buffer
static uint8_t serial_buffer[SERIAL_BUFFER_SIZE];
static uint16_t serial_counter = 0;
// run enable
uint8_t serial_run_state;
// pinout

// ~ serial bridge config

void init_serial_pins_highz()
{
    Serial.end();
    //GPIO 1 (TX) swap the pin to a GPIO.
    pinMode(1, FUNCTION_3);
    //GPIO 3 (RX) swap the pin to a GPIO.
    pinMode(3, FUNCTION_3);
    pinMode(1, INPUT);
    pinMode(3, INPUT);
    serial_run_state = 0;
}

void init_serial_server()
{
    pinMode(1, FUNCTION_0); 
    pinMode(3, FUNCTION_0); 
    Serial.begin(SERIAL_BAUD);
    serial_server.begin();
    // leave delay
    // serial_server.setNoDelay(true);
    serial_run_state = 1;
}

void stop_serial_server()
{
    serial_client.flush();
    serial_client.stop();
    serial_server.stop();
    init_serial_pins_highz();
}

void serial_loop()
{
    if(serial_run_state){
        if(!serial_client || !serial_client.connected()) {
        serial_client = serial_server.available();
        } else {
            while (serial_client.available() && serial_counter < SERIAL_BUFFER_SIZE) {
                // data should be availabe so skip -1 check.
                // more data than SERIAL_BUFFER_SIZE? next time.
                serial_buffer[serial_counter++] = (uint8_t)serial_client.read();
            }
            // Ethernet to serial
            Serial.write(serial_buffer, serial_counter);
            serial_counter = 0;

            // Serial to ethernet
            while(Serial.available() && serial_counter < SERIAL_BUFFER_SIZE){
                serial_buffer[serial_counter] = (uint8_t)Serial.read();
                serial_counter++;
            }
            serial_client.write(serial_buffer, serial_counter);
            serial_counter = 0;
        }
    }
}
