#ifndef COMMAND_H
#define COMMAND_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include "xvc.h"
#include "serial.h"

#define COMMAND_SEND_BUFFER_SIZE 9
#define COMMAND_PORT 42069

#define COMMAND_RST_PIN               16
#define COMMAND_BOOTMODE_CONTROL_PIN  5
#define COMMAND_BOOTMODE_SELECTOR_PIN 2

const char string_0[] PROGMEM = "[LOG]"; 
const char string_1[] PROGMEM = "STARTING COMMAND SERVER...";

// Initialize Table of Strings
const char* const string_table[] PROGMEM = {string_0, string_1};

// =============================================================================================

template <uint8_t rst_pin,
          uint8_t bootmode_control_pin,
          uint8_t bootmode_selector_pin>
class CommandPort
{
public:
    static void begin(uint8_t bootmode)
    {
        pinMode(rst_pin, OUTPUT);
        digitalWrite(rst_pin, 0); // keep board under reset
        pinMode(bootmode_control_pin, OUTPUT);
        set_bootmode(bootmode);
        pinMode(bootmode_selector_pin, INPUT);
        
    }

    static void set_bootmode(uint8_t bootmode)
    {
        digitalWrite(bootmode_control_pin, bootmode);
    }

    static void pulse_reset()
    {
        digitalWrite(rst_pin, 0);
        delay(5);
        digitalWrite(rst_pin, 1);
    }

    static void pull_reset_down()
    {
        digitalWrite(rst_pin, 0);
    }

    static void pull_reset_up()
    {
        digitalWrite(rst_pin, 1);
    }

    static uint8_t read_boot_selector()
    {
        return digitalRead(bootmode_selector_pin);
    }
};

// =============================================================================================
// ONLY SEND RETURN WHEN EXECUTE CMD SUCCESSFULY

/* Receive format
 * Header 0x04 0x20 0x69
 *  - Single byte for command
 *  - Optional single byte for data
 * => 4 to 5 bytes
 */

/* Return format
 * Header same as request 0x04 0x20 0x69
 *  - Single byte for cmd_code
 *  - Single byte for return type: Check return vals of command_state_update for types
 *  - 4 bytes data
 * => 9 bytes
 */

/* CMD Codes
 * 00 set boot mode
 * 01 get boot mode
 * 02 reset board
 * 03 set xvc running status disable / enable (set all high z) // also disable xvc loop
 * 04 get xvc running status
 * 05 set serial running status disable / enable (cmd still available) // not sending / reading from serial
 * 06 get serial running status
 * 07 reconfig wifi
 * 08 reset self
 */

// =============================================================================================

template <typename command_port>
class CommandServer
{

public:

    CommandServer(uint16_t port = 0) : server((port != 0) ? port : COMMAND_PORT), client(), wifiManager(), xvc_server(XVC_PORT), serial_server(SERIAL_PORT)
    {
        this->port = port;
    }

    void setup()
    {
        // https://www.esp8266.com/viewtopic.php?p=83075
        wifi_set_sleep_type(NONE_SLEEP_T);
        // https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
        // Also some pin need to be high to boot => need main FPGA power + usb to flash
        // used for boot select -> disabled
        bootmode = 1;
        command_port::begin(bootmode);
        // Pull reset down upon starting incase both board serial & esp serial are output -> short 
        command_port::pull_reset_down();
        // Enable serial logging for wifi
        SerialPort::begin();
        Serial.begin(SERIAL_BAUD);
        wifiManager.setConfigPortalBlocking(true);
        wifiManager.setTimeout(180);
        wifiManager.setHostname(PSTR("esp8266xvc.lan"));
        if(!wifiManager.autoConnect(PSTR("ESP8266 XVC-SERIAL BRIDGE"))) {
            Serial.println(PSTR("Wifi failed to connect, restarting..."));
            delay(500);
            ESP.restart();
        }
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
        Serial.println(PSTR("STARTING COMMAND SERVER..."));
        Serial.println(PSTR("PORT LIST:"));
        Serial.print(PSTR("\tCOMMAND: "));
        Serial.println((port != 0) ? port : COMMAND_PORT);
        Serial.print(PSTR("\tSERIAL: "));
        Serial.print(SERIAL_PORT);
        Serial.println(PSTR(" - DISABLED."));
        Serial.print(PSTR("\tXVC: "));
        Serial.print(XVC_PORT);
        Serial.println(PSTR(" - DISABLED."));
        delay(500);
        Serial.end();
        SerialPort::stop();
        command_port::pull_reset_up();
        // Disable esp serial from this point, only bridge target serial port
        // Do not enable if board serial outputing data
        
        server.begin();
        server.setNoDelay(true);
    }

    // Loop always running
    void handle()
    {
        // Button loop
        handle_manual_boot_selector_press();
        // Service loop
        serial_server.handle();
        xvc_server.handle();
        // Command loop
        if(!client || !client.connected()) {
            client = server.available();
            command_state = 0;
        } else {
            while (client.available()) {
                // Data should be availabe so skip -1 check.
                // Execute or update state then send back
                // No point using fifo since only 1 core?
                uint8_t type = command_state_update((uint8_t)client.read());
                if(type != 255){
                    prepare_respond(type);
                    command_respond();
                    return;
                }
                else if (command_state == 0){
                    // let others do their jobs, we can wait
                    return;
                }
            }
        }
    }

private:

    // API Handlers
    // Always return uint32_t

    uint32_t test()
    {
        return 0x69696969;
    }

    uint32_t reset_self()
    {
        ESP.reset();
        return 0;
    }

    uint32_t reconfig_wifi()
    {
        wifiManager.resetSettings();
        delay(1000);
        ESP.reset();
        return 0;
    }

    uint32_t set_bootmode(uint8_t mode)
    {
        command_port::set_bootmode(mode);
        bootmode = mode;
        return mode;
    }

    uint32_t get_bootmode()
    {
        return bootmode;
    }

    uint32_t reset_board()
    {
        command_port::pulse_reset();
        return 1;
    }

    uint32_t set_xvc_run_state(uint8_t mode)
    {
        uint8_t is_running = xvc_server.is_running();
        if (mode && !is_running) {
            xvc_server.begin();
        } else if (!mode && is_running){
            xvc_server.stop();
        }
        return (uint32_t)xvc_server.is_running();
    }

    uint32_t get_xvc_run_state()
    {
        return (uint32_t)xvc_server.is_running();
    }

    uint32_t set_serial_run_state(uint8_t mode)
    {
        uint8_t is_running = serial_server.is_running();
        if (mode && !is_running) {
            serial_server.begin();
        } else if (!mode && is_running){
            serial_server.stop();
        }
        return (uint32_t)serial_server.is_running();
    }

    uint32_t get_serial_run_state()
    {
        return (uint32_t)serial_server.is_running();
        return 0;
    }

    // ~ API handlers

    // Loop helper

    void handle_manual_boot_selector_press()
    {
        int debounce = 5;
        if (command_port::read_boot_selector() == LOW){
            delay(debounce);
            if(command_port::read_boot_selector() == LOW){
                int started = millis();
                if(started - bootmode_selector_last_pressed <= 750){
                    return;
                }
                bootmode_selector_last_pressed = started;
                set_bootmode(!bootmode);
            }
        }
    }

    void prepare_respond(const uint8_t& type)
    {
        memcpy(&command_send_buffer, "\x04\x20\x69", 3);
        memcpy(&command_send_buffer[3], &command_code, 1);
        memcpy(&command_send_buffer[4], &type, 1);
        memcpy(&command_send_buffer[5], &command_return_value, 4);
        command_send_buffer_counter = 9;
    }

    void command_respond()
    {
        uint8_t count = 0;
        for(count; count < command_send_buffer_counter; count++){
            client.write(command_send_buffer[count]);
        }
        command_send_buffer_counter = 0;
    }

    /* Return vals => data type in respond request
    * 255: Nothing executed, else check command_return_value
    * 0  : Status
    * ?  : ? Add later
    */
    uint8_t command_state_update(const uint8_t& data)
    {
        // ret_val == return request type
        uint8_t ret_val = 255;
        switch(command_state){
            case 0: // first pass
                if(data == '\x04')
                    command_state = 1;
                break;
            case 1: // 2nd pass
                if(data == '\x20')
                    command_state = 2;
                else
                    goto RESET_STATE;
                break;
            case 2:// 3rd pass
                if(data == '\x69')
                    command_state = 3;
                else
                    goto RESET_STATE;
                break;
            case 3: // execute simple cmd
                command_code = data;
                switch(command_code){
                    case 100:
                        command_return_value = test();
                        ret_val = 0;
                        goto RESET_STATE_0;
                    case 0:
                        // set boot mode need data
                        // also go here when need data
SET_STATE_4:
                        command_state = 4;
                        break;
                    case 1:
                        command_return_value = get_bootmode();
                        ret_val = 0;
                        goto RESET_STATE_0;
                    case 2:
                        command_return_value = reset_board();
                        ret_val = 0;
                        goto RESET_STATE_0;
                    case 3:
                        goto SET_STATE_4;
                    case 4:
                        command_return_value = get_xvc_run_state();
                        ret_val = 0;
                        goto RESET_STATE_0;
                    case 5:
                        goto SET_STATE_4;
                    case 6:
                        command_return_value = get_serial_run_state();
                        ret_val = 0;
                        goto RESET_STATE_0;
                    case 7:
                        command_return_value = reconfig_wifi();
                        ret_val = 0;
                        goto RESET_STATE_0;
                    case 8:
                        command_return_value = reset_self();
                        ret_val = 0;
                        goto RESET_STATE_0;
                    default:
                        goto STATE_UNK_CMD;
                }
                break;
            case 4: // fetch additional data and execute cmd
                switch(command_code){
                    case 0:
                        command_return_value = set_bootmode(data);
                        ret_val = 0;
                        goto RESET_STATE_0;
                    case 3:
                        command_return_value = set_xvc_run_state(data);
                        ret_val = 0;
                        goto RESET_STATE_0;
                    case 5:
                        command_return_value = set_serial_run_state(data);
                        ret_val = 0;
                        goto RESET_STATE_0;
                    default:
STATE_UNK_CMD:
                        goto RESET_STATE;
                }
                break;
            default:
                /* For normal command, reset to 0 after execution
                 * If received unkown command lead to unkown state, check if byte is 0x4
                 * This is to prevent failure to receive one command to chain to another and state keep reset to 0
                 */
RESET_STATE:
                if(data == '\x04')
                    command_state = 1;
                else
RESET_STATE_0:
                    command_state = 0;
        }
        return ret_val;
    }

    // ~ Loop helper

private:

    WiFiServer server;
    WiFiClient client;
    WiFiManager wifiManager;

    uint16_t port;
    uint8_t command_code;
    uint8_t bootmode;
    uint32_t bootmode_selector_last_pressed;
    uint8_t command_state;
    uint32_t command_return_value;

    uint8_t command_send_buffer[COMMAND_SEND_BUFFER_SIZE];
    uint8_t command_send_buffer_counter = 0; // 1 byte only 255 max

    SerialServer serial_server;
    XvcServer<JtagPort<XVC_TCK, XVC_TDO, XVC_TDI, XVC_TMS>> xvc_server;
};

extern CommandServer<CommandPort<COMMAND_RST_PIN, COMMAND_BOOTMODE_CONTROL_PIN, COMMAND_BOOTMODE_SELECTOR_PIN>> command_server;

#endif
