#ifndef XVC_H
#define XVC_H

#include <ESP8266WiFi.h>

#define XVC_PORT 2542
#define XVC_TMS  4
#define XVC_TCK  14
#define XVC_TDO  12
#define XVC_TDI  13

// =============================================================================================

template <uint8_t tck_pin,
          uint8_t tdo_pin,
          uint8_t tdi_pin,
          uint8_t tms_pin>
class JtagPort
{
public:
    static void begin()
    {
        pinMode(tck_pin, OUTPUT);
        pinMode(tdo_pin, INPUT);
        pinMode(tdi_pin, OUTPUT);
        pinMode(tms_pin, OUTPUT);
        GPOC = tck_pin_mask |
                tdi_pin_mask |
                tms_pin_mask;
    }

    static void stop()
    {
        pinMode(tck_pin, INPUT);
        pinMode(tdo_pin, INPUT);
        pinMode(tdi_pin, INPUT);
        pinMode(tms_pin, INPUT);
    }

    static bool step(bool tms, bool tdi)
    {
        uint32_t value[2] = {tck_pin_mask, 0};
        value[tms] |= tms_pin_mask;
        value[tdi] |= tdi_pin_mask;
        GPOC = value[0];
        GPOS = value[1];
        GPOS = tck_pin_mask;
        bool tdo = (GPI & tdo_pin_mask) != 0;
        return tdo;
    }

    static void shift(uint32_t bit_len, uint32_t byte_len, uint8_t *buffer)
    {
        uint8_t mask = 1;
        uint8_t tms_byte = *buffer;
        uint8_t tdi_byte = *(buffer + byte_len);
        uint8_t tdo_byte = 0;
        for (uint32_t bit_index = 0; bit_index < bit_len; bit_index++) {
            bool tms = (tms_byte & mask) != 0;
            bool tdi = (tdi_byte & mask) != 0;
            bool tdo = step(tms, tdi);
            if (tdo)
                tdo_byte |= mask;
            mask <<= 1;
            if (mask == 0) {
                *buffer++ = tdo_byte;
                mask = 1;
                tms_byte = *buffer;
                tdi_byte = *(buffer + byte_len);
                tdo_byte = 0;
            }
        }
        *buffer = tdo_byte;
        GPOC = tck_pin_mask;
    }

private:
    static constexpr const uint32_t tck_pin_mask = (1 << tck_pin);
    static constexpr const uint32_t tdo_pin_mask = (1 << tdo_pin);
    static constexpr const uint32_t tdi_pin_mask = (1 << tdi_pin);
    static constexpr const uint32_t tms_pin_mask = (1 << tms_pin);
};

// =============================================================================================

template <typename jtag_port>
class XvcServer
{
    enum class ProtocolState
    {
        WaitingCommand,
        GetInfoCommand,
        SetClockCommand,
        ShiftCommand,
        ShiftData,
    };

public:

    XvcServer(uint16_t port) : server(port), client()
    {
        server.setNoDelay(true);
        jtag_port::stop();
        running = 0;
    }

    void begin()
    {
        if (!running) {
            jtag_port::begin();
            server.begin();
            running = 1;
        }
    }

    void stop()
    {
        if (running) {
            client.flush();
            client.stop();
            server.stop();
            jtag_port::stop();
            running = 0;
        }
    }

    uint8_t is_running()
    {
        return running;
    }

    void handle()
    {
        if (running) {
            if (client.connected()) {
                if (client.available()) {
                    size_t len = client.read(buffer + position, remaining);
                    remaining -= len;
                    position += len;
                    if (remaining == 0) {
                        next_state();
                    }
                }
            }
            else if (server.hasClient()) {
                client = server.available();
                enter_waiting_command();
            }
        }
    }

private:

    void enter_waiting_command()
    {
        state = ProtocolState::WaitingCommand;
        remaining = 2;
        position = 0;
    }

    void enter_error_state()
    {
        client.stop();
        enter_waiting_command();
    }

    void parse_command()
    {
        position = 0;
        if (memcmp(buffer, "ge", 2) == 0) {
            remaining = 6;
            state = ProtocolState::GetInfoCommand;
        }
        else if (memcmp(buffer, "se", 2) == 0) {
            remaining = 9;
            state = ProtocolState::SetClockCommand;
        }
        else if (memcmp(buffer, "sh", 2) == 0) {
            remaining = 8;
            state = ProtocolState::ShiftCommand;
        }
        else {
            enter_error_state();
        }
    }

    void next_state()
    {
        switch (state) {
        case ProtocolState::WaitingCommand:
            parse_command();
            break;
        case ProtocolState::GetInfoCommand:
            client.printf("xvcServer_v1.0:%u\n", max_buffer_size);
            enter_waiting_command();
            break;
        case ProtocolState::SetClockCommand:
            client.write(buffer + 5, 4);
            enter_waiting_command();
            break;
        case ProtocolState::ShiftCommand:
            bit_len = buffer[7];
            bit_len = (bit_len << 8) | buffer[6];
            bit_len = (bit_len << 8) | buffer[5];
            bit_len = (bit_len << 8) | buffer[4];
            byte_len = (bit_len + 7) / 8;
            if (byte_len <= max_buffer_size) {
                state = ProtocolState::ShiftData;
                remaining = byte_len * 2;
                position = 0;
            }
            else {
                enter_error_state();
            }
            break;
        case ProtocolState::ShiftData:
            jtag_port::shift(bit_len, byte_len, buffer);
            client.write(buffer, byte_len);
            enter_waiting_command();
            break;
        default:
            enter_error_state();
            break;
        }
    }

private:

    WiFiServer server;
    WiFiClient client;

    ProtocolState state;
    size_t remaining;
    size_t position;

    uint32_t bit_len;
    uint32_t byte_len;

    static constexpr size_t max_buffer_size = 16 * 1024;
    uint8_t buffer[max_buffer_size];

    uint8_t running;
};

#endif
