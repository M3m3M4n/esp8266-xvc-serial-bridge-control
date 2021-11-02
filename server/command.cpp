#include <WiFiManager.h>
#include "common.h"
#include "command.h"
#include "xvc.h"
#include "serial.h"

static uint8_t command_state;
static uint32_t command_return_value;

static uint8_t command_send_buffer[COMMAND_SEND_BUFFER_SIZE];
static uint8_t command_send_buffer_counter = 0; // 1 byte only 255 max

static WiFiManager wifiManager;

static WiFiServer command_server(COMMAND_ETHERNET_PORT);
static WiFiClient command_client;

bool serial_log_verbose = 0;
uint8_t bootmode = 1;

uint32_t bootmode_selector_last_pressed;

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
 * 08 set log status
 * 09 get log status
 * 10 reset self
 */
static uint8_t command_code;


/* Pinout
 */
static constexpr const int rst_pin = D0;
static constexpr const int bootmode_pin = D1;
static constexpr const int bootmode_selector = D4;

static uint32_t test()
{
    if(serial_log_verbose){
        LOGLN("[LOG] TEST");
    }
    return 0x69696969;
}

static uint32_t reset_self()
{
    ESP.reset();
    return 0;
}

static uint32_t reconfig_wifi()
{
    wifiManager.resetSettings();
    delay(1000);
    ESP.reset();
}

static uint32_t set_bootmode(uint8_t mode)
{
    digitalWrite(bootmode_pin, mode);
    bootmode = mode;
    return mode;
}

static uint32_t get_bootmode()
{
    return bootmode;
}

void handle_manual_boot_selector_press()
{
    int debounce = 5;
    if (digitalRead(bootmode_selector) == LOW){
        delay(debounce);
        if(digitalRead(bootmode_selector) == LOW){
            int started = millis();
            if(started - bootmode_selector_last_pressed <= 750){
                return;
            }
            bootmode_selector_last_pressed = started;
            set_bootmode(!bootmode);
        }
    }
}

static uint32_t reset_board()
{
    digitalWrite(rst_pin, 0);
    delay(5);
    digitalWrite(rst_pin, 1);
    return 1;
}

static uint32_t set_xvc_run_state(uint8_t mode)
{
    if (mode && !xvc_run_state) {
        init_xvc_server();
    } else if (!mode && xvc_run_state){
        stop_xvc_server();
    }
    return (uint32_t)xvc_run_state;
}

static uint32_t get_xvc_run_state()
{
    return (uint32_t)xvc_run_state;
}

static uint32_t set_serial_run_state(uint8_t mode)
{
    if (mode && !serial_run_state) {
        init_serial_server();
    } else if (!mode && serial_run_state){
        stop_serial_server();
    }
    return (uint32_t)serial_run_state;
}

static uint32_t get_serial_run_state()
{
    return (uint32_t)serial_run_state;
}

static uint32_t set_serial_log_verbose(uint8_t mode)
{
    serial_log_verbose = mode;
    return (uint32_t)mode;
}

static uint32_t get_serial_log_verbose()
{
    return (uint32_t)serial_log_verbose;
}

/* Return vals => data type in respond request
 * 255: Nothing executed, else check command_return_value
 * 0  : Status
 * ?  : ? Add later
 */
static uint8_t command_state_update(const uint8_t& data)
{
    // ret_val == return request type
    if(serial_log_verbose){
        LOG("[LOG] CURRENT COMMAND STATE: ");
        LOGLN(command_state);
        LOG("[LOG] COMMAND RECEIVED: ");
        LOGLN(data, HEX);
    }
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
                    goto RESET_STATE;
                case 0:
                    // set boot mode need data
                    // also go here when need data
SET_STATE_4:        command_state = 4;
                    break;
                case 1:
                    command_return_value = get_bootmode();
                    ret_val = 0;
                    goto RESET_STATE;
                case 2:
                    command_return_value = reset_board();
                    ret_val = 0;
                    goto RESET_STATE;
                case 3:
                    goto SET_STATE_4;
                case 4:
                    command_return_value = get_xvc_run_state();
                    ret_val = 0;
                    goto RESET_STATE;
                case 5:
                    goto SET_STATE_4;
                case 6:
                    command_return_value = get_serial_run_state();
                    ret_val = 0;
                    goto RESET_STATE;
                case 7:
                    command_return_value = reconfig_wifi();
                    ret_val = 0;
                    goto RESET_STATE;
                case 8:
                    goto SET_STATE_4;
                case 9:
                    command_return_value = get_serial_log_verbose();
                    ret_val = 0;
                    goto RESET_STATE;
                case 10:
                    command_return_value = reset_self();
                    ret_val = 0;
                    goto RESET_STATE;
                default:
                    goto STATE_UNK_CMD;
            }
            break;
        case 4: // fetch additional data and execute cmd
            switch(command_code){
                case 0:
                    command_return_value = set_bootmode(data);
                    ret_val = 0;
                    goto RESET_STATE;
                case 3:
                    command_return_value = set_xvc_run_state(data);
                    ret_val = 0;
                    goto RESET_STATE;
                case 5:
                    command_return_value = set_serial_run_state(data);
                    ret_val = 0;
                    goto RESET_STATE;
                case 8:
                    command_return_value = set_serial_log_verbose(data);
                    ret_val = 0;
                    goto RESET_STATE;
                default:
STATE_UNK_CMD:
                    LOG("[LOG] RECEIVED UNKNOWN COMMAND: ");
                    LOGLN(command_code, HEX);
                    goto RESET_STATE;
            }
            break;
        default:
            LOG("[LOG] UNKNOWN COMMAND STATE: ");
            LOGLN(command_state, HEX);
RESET_STATE:
            command_state = 0;
    }
    return ret_val;
}

// Return value && command code in global
static void prepare_respond(const uint8_t& type)
{
    memcpy(&command_send_buffer, "\x04\x20\x69", 3);
    memcpy(&command_send_buffer[3], &command_code, 1);
    memcpy(&command_send_buffer[4], &type, 1);
    memcpy(&command_send_buffer[5], &command_return_value, 4);
    command_send_buffer_counter = 9;
}

static void command_respond()
{
    uint8_t count = 0;
    for(count; count < command_send_buffer_counter; count++){
        if(command_client.connected())
            command_client.write(command_send_buffer[count]);
    }
    command_send_buffer_counter = 0;
}


void command_loop()
{
    handle_manual_boot_selector_press();
    // Command loop
    // Read from ethernet
    if(!command_client || !command_client.connected()) {
        command_client = command_server.available();
        command_state = 0;
    } else {
        while (command_client.available()) {
            // Data should be availabe so skip -1 check.
            // Execute or update state then send back
            // No point using fifo since only 1 core?
            uint8_t type = command_state_update((uint8_t)command_client.read());
            if(type != 255){
                if(serial_log_verbose){
                    LOGLN("[LOG] EXECUTED AND RETURN: ");
                    LOG("[LOG] TYPE: ");
                    LOGLN(type);
                    LOG("[LOG] RETURN VALUE: ");
                    LOGLN(command_return_value, HEX);
                }
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

static void init_command_gpio()
{
    pinMode(rst_pin, OUTPUT);
    digitalWrite(rst_pin, 1);
    pinMode(bootmode_pin, OUTPUT);
    digitalWrite(bootmode_pin, bootmode);
    pinMode(bootmode_selector, INPUT);
}

void init_command_server()
{

    // https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
    // Also some pin need to be high to boot => need main FPGA power + usb to flash
    // used for boot select -> disabled
    // Enable serial logging for wifi
    Serial.begin(SERIAL_BAUD);
    serial_log_verbose = 0;

    if(!wifiManager.autoConnect("ESP XVC-SERIAL BRIDGE")) {
        LOGLN("Wifi failed to connect and hit timeout!");
        delay(500);
        ESP.reset();
    }
    LOGLN("[LOG] STARTING COMMAND SERVER...");
    LOGLN("[LOG] PORT LIST:");
    LOG("[LOG] COMMAND: ");
    LOGLN(COMMAND_ETHERNET_PORT);
    LOG("[LOG] SERIAL: ");
    LOG(SERIAL_ETHERNET_PORT);
    LOGLN(" - DISABLED.");
    LOG("[LOG] XVC: ");
    LOG(XVC_ETHERNET_PORT);
    LOGLN(" - DISABLED.");
    delay(500);
    Serial.end();
    init_command_gpio();
    command_server.begin();
    command_server.setNoDelay(true);
}
