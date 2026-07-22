/* src/main.c */
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <string.h>

LOG_MODULE_REGISTER(uart_tx_probe, LOG_LEVEL_INF);

#define UART_NODE DT_NODELABEL(uart0) /* matches your overlay */
static const struct device *uart = DEVICE_DT_GET(UART_NODE);

/* Probe pin (change if you prefer) */
#define PROBE_PIN 11
static const struct device *gpio_dev;

void main(void)
{
    const char *msg = "TXPROBE\n";
    size_t len = strlen(msg);

    LOG_INF("TX probe start (uart2). Probe P0.%d toggles while sending.", PROBE_PIN);

    if (!device_is_ready(uart)) {
        LOG_ERR("UART not ready (%p)", uart);
        return;
    }

    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(gpio_dev)) {
        LOG_ERR("GPIO0 not ready");
        return;
    }

    gpio_pin_configure(gpio_dev, PROBE_PIN, GPIO_OUTPUT_INACTIVE);

    while (1) {
        /* raise probe so you can correlate on scope/meter */
        gpio_pin_set(gpio_dev, PROBE_PIN, 1);

        /* send message quickly */
        for (size_t i = 0; i < len; ++i) {
            uart_poll_out(uart, (uint8_t)msg[i]);
            /* tiny gap so bytes aren't all squashed (optional) */
            k_msleep(1);
        }
        LOG_INF("Sent %d bytes", (int)len);

        /* small hold for measurement */
        k_msleep(10);

        gpio_pin_set(gpio_dev, PROBE_PIN, 0);

        /* short read attempt (not critical for probe) */
        uint8_t rx;
        int tries = 50;
        int got = 0;
        while (tries-- > 0) {
            if (uart_poll_in(uart, &rx) == 0) {
                LOG_INF("RX: 0x%02X '%c'", rx, (rx >= 32 && rx <= 126) ? rx : '.');
                got = 1;
            } else {
                k_msleep(5);
            }
        }
        if (!got) {
            LOG_WRN("No echo received (probe cycle)");
        }

        k_msleep(100);
    }
}
