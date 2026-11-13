#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Pin definitions: WeAct E-Ink -> Raspberry Pi Pico 2
#define PIN_SCL    18 // Serial communication clock input
#define PIN_SDA_TX 19 // Serial communication data input/output (bidirectional SDA)
#define PIN_DC     20 // Data/Command control input (L: command, H: data)
#define PIN_CS     17 // Chip Select input (L: active, H: inactive)
#define PIN_BUSY   26 // Driver status output (L: busy, H: ready)
#define PIN_RES    27 // Global hardware reset input (L: active, H: normal operation)
#define SPI_PORT spi0 //

void gpioInitCustom(void) {
    /*
    Initializes CS, DC, RES pins as outputs (default HIGH) and BUSY as input.

    INPUT:
        void - none
    OUTPUT:
        void - none
    */
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);

    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);

    gpio_init(PIN_RES);
    gpio_set_dir(PIN_RES, GPIO_OUT);

    gpio_put(PIN_CS,  1);
    gpio_put(PIN_DC,  1);
    gpio_put(PIN_RES, 1);

    gpio_init(PIN_BUSY);
    gpio_set_dir(PIN_BUSY, GPIO_IN);
}

void spiInitCustom(void) {
    /*
    Configures hardware SPI0 at 1 MHz (Mode 0, MSB first) on SCL and SDA pins.

    INPUT:
        void - none
    OUTPUT:
        void - none
    */
    spi_init(SPI_PORT, 1000 * 1000);

    gpio_set_function(PIN_SCL,    GPIO_FUNC_SPI);
    gpio_set_function(PIN_SDA_TX, GPIO_FUNC_SPI);

    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void writeCommand(uint8_t cmd) {
    /*
    Sends a single command byte over SPI with D/C set to LOW.

    INPUT:
        uint8_t cmd - command byte to send
    OUTPUT:
        void - none
    */
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

void writeDataBytes(const uint8_t *data, size_t len) {
    /*
    Sends a block of data bytes over SPI with D/C set to HIGH.

    INPUT:
    const uint8_t *data - pointer to data buffer
        size_t len - number of bytes to send
    OUTPUT:
        void - none
    */
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);
    spi_write_blocking(SPI_PORT, data, len);
    gpio_put(PIN_CS, 1);
}

void waitBusy(const char *etap) {
    /*
    Blocks until BUSY pin goes HIGH (ready) or times out after 5 seconds.

    INPUT:
    const char *etap - stage label printed on timeout
    OUTPUT:
        void - none
    */
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (gpio_get(PIN_BUSY) == 0) {
        sleep_ms(5);
        if (to_ms_since_boot(get_absolute_time()) - start > 5000) {
            printf("[TIMEOUT] BUSY stuck at 0 in step: %s\n", etap);
            return;
        }
    }
}

void display_reset(void) {
    /*
    Pulses RES pin LOW to perform a hardware reset and waits for BUSY ready state.

    INPUT:
        void - none
    OUTPUT:
        void - none
    */
    gpio_put(PIN_RES, 1);
    sleep_ms(10);
    gpio_put(PIN_RES, 0);
    sleep_ms(10);
    gpio_put(PIN_RES, 1);
    sleep_ms(20);
    waitBusy("Reset");
}

void configureBoosterUC8253(void) {
    /*
    Configures booster soft start (0x06), power settings (0x01), and panel resolution (0x00).

    INPUT:
        void - none
    OUTPUT:
        void - none
    */
    const uint8_t btst[] = {0x17, 0x17, 0x17};
    const uint8_t pwr[]  = {0x03, 0x00, 0x2B, 0x2B, 0x09};
    const uint8_t psr[]  = {0xD7, 0x0E};

    writeCommand(0x06);
    writeDataBytes(btst, sizeof(btst));

    writeCommand(0x01);
    writeDataBytes(pwr, sizeof(pwr));

    writeCommand(0x00);
    writeDataBytes(psr, sizeof(psr));
}

void powerOnBooster(void) {
    /*
    Sends Power ON command (0x04) and waits until booster startup completes.

    INPUT:
        void - none
    OUTPUT:
        void - none
    */
    writeCommand(0x04);
    sleep_ms(20);
    waitBusy("Power ON (0x04)");
}

void powerOffBooster(void) {
    /*
    Sends Power OFF command (0x02) and waits until booster shutdown completes.

    INPUT:
        void - none
    OUTPUT:
        void - none
    */
    writeCommand(0x02);
    sleep_ms(20);
    waitBusy("Power OFF (0x02)");
}

uint8_t readStatus_0x71(void) {
    /*
    Sends 0x71 via SPI0, switches SDA to GPIO input, and bit-bangs 8 status bits.

    INPUT:
        void - none
    OUTPUT:
        uint8_t status - 8-bit status register value from command 0x71
    */
    uint8_t cmd = 0x71;
    uint8_t status = 0;

    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);

    gpio_set_function(PIN_SCL,    GPIO_FUNC_SIO);
    gpio_set_function(PIN_SDA_TX, GPIO_FUNC_SIO);

    gpio_set_dir(PIN_SCL, GPIO_OUT);
    gpio_put(PIN_SCL, 0);

    gpio_set_dir(PIN_SDA_TX, GPIO_IN);
    gpio_pull_up(PIN_SDA_TX);

    gpio_put(PIN_DC, 1);
    sleep_us(2);

    for (uint8_t i = 0; i < 8; i++) {
        status <<= 1;
        gpio_put(PIN_SCL, 1);
        sleep_us(2);
        if (gpio_get(PIN_SDA_TX)) {
            status |= 0x01;
        }
        gpio_put(PIN_SCL, 0);
        sleep_us(2);
    }

    gpio_put(PIN_CS, 1);

    gpio_disable_pulls(PIN_SDA_TX);
    gpio_set_function(PIN_SCL,    GPIO_FUNC_SPI);
    gpio_set_function(PIN_SDA_TX, GPIO_FUNC_SPI);

    return status;
}

void printStatus(const char *label, uint8_t s) {
    /*
    Prints the status byte in hex, binary, and extracts the PON flag (bit 3).

    INPUT:
        const char *label - prefix text displayed before the status value
        uint8_t s - status byte to format and print
    OUTPUT:
        void - none
    */
    printf("%s: 0x%02X | BIN: %d%d%d%d%d%d%d%d (PON=%d)\n",
        label,
        s,
        (s >> 7) & 1, (s >> 6) & 1, (s >> 5) & 1, (s >> 4) & 1,
        (s >> 3) & 1, (s >> 2) & 1, (s >> 1) & 1, s & 1,
        (s >> 3) & 1);
}

int main(void) {
    /*
    Initializes peripherals, configures the display driver, and periodically logs status flags.

    INPUT:
        void - none
    OUTPUT:
        int - program exit code (0)
    */
    stdio_init_all();
    sleep_ms(2000);

    spiInitCustom();
    gpioInitCustom();
    display_reset();
    configureBoosterUC8253();

    sleep_ms(10000);
    printf("[INFO] Finish initialized!\n");
    printf("V:0.0.1c\n");
    printf("=== UC8253 (240x416) - STATUS TEST WITH CONVERTER CONFIGURATION ===\n");

    while (1) {
        powerOnBooster();
        sleep_ms(2000);

        printStatus("Po Power ON  (0x04) ", readStatus_0x71());
        sleep_ms(2000);

        powerOffBooster();
        sleep_ms(2000);

        printStatus("Po Power OFF (0x02)", readStatus_0x71());
        sleep_ms(2000);
    }
    return 0;
}