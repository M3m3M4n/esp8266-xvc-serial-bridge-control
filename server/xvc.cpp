#include "common.h"
#include "xvc.h"

// xvc config
// Pin out
static constexpr const int tms_gpio = D2;
static constexpr const int tck_gpio = D5;
static constexpr const int tdo_gpio = D6;
static constexpr const int tdi_gpio = D7;

static WiFiServer xvc_server(XVC_ETHERNET_PORT);
static WiFiClient xvc_client;

uint8_t xvc_run_state;

// JTAG buffers
static uint8_t xvc_jtag_cmd[16];
static uint8_t xvc_jtag_buffer[1024], xvc_jtag_result[512];

/* Transition delay coefficients */
static const unsigned int xvc_jtag_delay = 10;  // NOTE!
// ~ xvc config

static bool jtag_read(void)
{
    return digitalRead(tdo_gpio) & 1;
}

static void jtag_write(std::uint_fast8_t tck, std::uint_fast8_t tms, std::uint_fast8_t tdi)
{
    digitalWrite(tck_gpio, tck);
    digitalWrite(tms_gpio, tms);
    digitalWrite(tdi_gpio, tdi);

    for (std::uint32_t i = 0; i < xvc_jtag_delay; i++)
    asm volatile ("nop");
}

static std::uint32_t jtag_xfer(std::uint_fast8_t n, std::uint32_t tms, std::uint32_t tdi)
{
    std::uint32_t tdo = 0;
    for (uint_fast8_t i = 0; i < n; i++) {
        jtag_write(0, tms & 1, tdi & 1);
        tdo |= jtag_read() << i;
        jtag_write(1, tms & 1, tdi & 1);
        tms >>= 1;
        tdi >>= 1;
    }
    return tdo;
}

static int jtag_init(void)
{
    pinMode(tdo_gpio, INPUT);
    pinMode(tdi_gpio, OUTPUT);
    pinMode(tck_gpio, OUTPUT);
    pinMode(tms_gpio, OUTPUT);

    digitalWrite(tdi_gpio, 0);
    digitalWrite(tck_gpio, 0);
    digitalWrite(tms_gpio, 1);

    return XVC_ERROR_OK;
}

int sread(void *target, int len)
{
    uint8_t *t = (uint8_t *) target;
    while (len) {
        int r = xvc_client.read(t, len);
        if (r <= 0)
            return r;
        t += r;
        len -= r;
    }
    return 1;
}

int srcmd(void * target, int maxlen)
{
    uint8_t *t = (uint8_t *) target;
    while (maxlen) {
        int r = xvc_client.read(t, 1);
        if (r <= 0)
            return r;

        if (*t == ':') {
            return 1;
        }
        t += r;
        maxlen -= r;
    }
    return 0;
}

void xvc_loop()
{
    if(xvc_run_state){
        if (!xvc_client || !xvc_client.connected()) {
            xvc_client = xvc_server.available();
        } else {
            if (xvc_client.available()) {
                if (srcmd(xvc_jtag_cmd, 8) != 1)
                    return;
                if (memcmp(xvc_jtag_cmd, "getinfo:", 8) == 0) {
                    if(serial_log_verbose)
                        LOG("XVC_info\n");
                    xvc_client.write("xvcServer_v1.0:");
                    xvc_client.print(XVC_MAX_WRITE_SIZE);
                    xvc_client.write("\n");
                    return;
                }
                if (memcmp(xvc_jtag_cmd, "settck:", 7) == 0) {
                    if(serial_log_verbose)
                        LOG("XVC_tck\n");
                    int ntck;
                    if (sread(&ntck, 4) != 1) {
                        LOG("reading tck failed\n");
                        return;
                    }
                    // Actually TCK frequency is fixed, but replying a fixed TCK will halt hw_server
                    xvc_client.write((const uint8_t *)&ntck, 4);
                    return;
                }

                if (memcmp(xvc_jtag_cmd, "shift:", 6) != 0) {
                    xvc_jtag_cmd[15] = '\0';
                    LOG("invalid cmd ");
                    LOGLN((char *)xvc_jtag_cmd);
                    return;
                }

                int len;
                if (sread(&len, 4) != 1) {
                    LOG("reading length failed\n");
                    return;
                }

                unsigned int nr_bytes = (len + 7) / 8;

                if(serial_log_verbose){
                    LOG("len = ");
                    LOG(len);
                    LOG(" nr_bytes = ");
                    LOGLN(nr_bytes);
                }

                if (nr_bytes * 2 > sizeof(xvc_jtag_buffer)) {
                    LOG("buffer size exceeded");
                    return;
                }

                if (sread(xvc_jtag_buffer, nr_bytes * 2) != 1) {
                    LOG("reading data failed\n");
                    return;
                }

                memset((uint8_t *)xvc_jtag_result, 0, nr_bytes);

                jtag_write(0, 1, 1);

                int bytesLeft = nr_bytes;
                int bitsLeft = len;
                int byteIndex = 0;
                uint32_t tdi, tms, tdo;

                while (bytesLeft > 0) {
                    tms = 0;
                    tdi = 0;
                    tdo = 0;
                    if (bytesLeft >= 4) {
                        memcpy(&tms, &xvc_jtag_buffer[byteIndex], 4);
                        memcpy(&tdi, &xvc_jtag_buffer[byteIndex + nr_bytes], 4);
                        tdo = jtag_xfer(32, tms, tdi);
                        memcpy(&xvc_jtag_result[byteIndex], &tdo, 4);
                        bytesLeft -= 4;
                        bitsLeft -= 32;
                        byteIndex += 4;
                    } else {
                        memcpy(&tms, &xvc_jtag_buffer[byteIndex], bytesLeft);
                        memcpy(&tdi, &xvc_jtag_buffer[byteIndex + nr_bytes], bytesLeft);
                        tdo = jtag_xfer(bitsLeft, tms, tdi);
                        memcpy(&xvc_jtag_result[byteIndex], &tdo, bytesLeft);
                        bytesLeft = 0;
                        break;
                    }
                }

                jtag_write(0, 1, 0);

                if (xvc_client.write(xvc_jtag_result, nr_bytes) != nr_bytes) {
                    LOG("write");
                }
            }
        }
    }
}

void init_xvc_pins_highz()
{
    pinMode(tdo_gpio, INPUT);
    pinMode(tdi_gpio, INPUT);
    pinMode(tck_gpio, INPUT);
    pinMode(tms_gpio, INPUT);
    xvc_run_state = 0;
}

void init_xvc_server()
{
    jtag_init();
    xvc_server.begin();
    xvc_server.setNoDelay(true);
    xvc_run_state = 1;
}

void stop_xvc_server()
{
    xvc_client.flush();
    xvc_client.stop();
    xvc_server.stop();
    init_xvc_pins_highz();
}
