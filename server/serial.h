#ifndef SERIAL_H
#define SERIAL_H

#include <ESP8266WiFi.h>

#define SERIAL_TX_PIN 1
#define SERIAL_RX_PIN 3

#define SERIAL_BAUD 115200
#define SERIAL_BUFFER_SIZE 64
#define SERIAL_INTERNAL_BUFFER_SIZE 2048
#define SERIAL_PORT 2222

// =============================================================================================
class SerialPort
{

public:
 
    static void begin()
    {
        pinMode(1, FUNCTION_0);
        pinMode(3, FUNCTION_0);
    }

    static void stop()
    {
        //GPIO 1 (TX) swap the pin to a GPIO.
        pinMode(SERIAL_TX_PIN, FUNCTION_3);
        //GPIO 3 (RX) swap the pin to a GPIO.
        pinMode(SERIAL_RX_PIN, FUNCTION_3);
        pinMode(SERIAL_TX_PIN, INPUT);
        pinMode(SERIAL_RX_PIN, INPUT);
    }
};

// =============================================================================================

class SerialServer
{
public:
    SerialServer(uint16_t port) : server(port), client()
    {
        Serial.end();
        SerialPort::stop();
        server.setNoDelay(true);
        running = 0;
    }

    void begin()
    {
        if (!running) {
            SerialPort::begin();
            Serial.begin(SERIAL_BAUD);
            Serial.setRxBufferSize(SERIAL_INTERNAL_BUFFER_SIZE);
            server.begin();
            running = 1;
        }
    }

    void stop()
    {
        if (running) {
            Serial.end();
            SerialPort::stop();
            running = 0;
        }
    }

    void handle()
    {
        if (running) {
            /*
            // Serial console quite unresponsive
            if (client.connected()) {
                client.sendAvailable(Serial);
                client.flush();
                Serial.sendAvailable(client);
            }
            else if (server.hasClient()) {
                client = server.available();
            }
            */
            // Need mnore processing power than previous impl but responsive
            if(!client.connected()) {
                if (server.hasClient())
                    client = server.available();
            } else {
                while (client.available() && counter < SERIAL_BUFFER_SIZE) {
                    // data should be availabe so skip -1 check.
                    // more data than SERIAL_BUFFER_SIZE? next time.
                    buffer[counter++] = (uint8_t)client.read();
                }
                // Client to serial
                Serial.write(buffer, counter);
                counter = 0;

                // Serial to client
                while(Serial.available() && counter < SERIAL_BUFFER_SIZE){
                    buffer[counter] = (uint8_t)Serial.read();
                    counter++;
                }
                client.write(buffer, counter);
                counter = 0;
            }
        }
    }

    uint8_t is_running()
    {
        return running;
    }

private:

    WiFiServer server;
    WiFiClient client;
    uint8_t running;

    uint32_t counter;
    uint8_t buffer[SERIAL_BUFFER_SIZE];
};

#endif
