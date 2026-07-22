#if 0

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <string.h>

LOG_MODULE_REGISTER(uart_aggressive_loop, LOG_LEVEL_INF);

/* Use the uart node you configured in the overlay */
#define UART_NODE DT_NODELABEL(uart0)
static const struct device *uart = DEVICE_DT_GET(UART_NODE);

static void uart_cb(const struct device *dev, void *user_data)
{
    uint8_t buf[32];
    int len;

    /* Update IRQ state */
    if (!uart_irq_update(dev)) {
        return;
    }

    /* If RX data is ready, read it */
    if (uart_irq_rx_ready(dev)) {
        len = uart_fifo_read(dev, buf, sizeof(buf));
        if (len > 0) {
            LOG_INF("RX %d bytes", len);
            /* Echo back immediately */
            uart_fifo_fill(dev, buf, len);
        }
    }
}

void main(void)
{
    LOG_INF("Starting UART2 IRQ loopback on P0.06 (TX) and P0.07 (RX)");
    printk("Starting UART2 IRQ loopback on P0.06 (TX) and P0.07 (RX)");

    if (!device_is_ready(uart)) {
        LOG_ERR("UART2 not ready");
        return;
    }

    /* Set callback and enable RX/TX interrupts */
    uart_irq_callback_set(uart, uart_cb);
    uart_irq_rx_enable(uart);
    uart_irq_tx_enable(uart);

    /* Send a test string */
    const char *msg = "HELLO_LOOPBACK\n";
    for (const char *p = msg; *p; p++) {
        uart_fifo_fill(uart, (const uint8_t *)p, 1);
        k_msleep(1);
    }

    /* Main thread does nothing; ISR handles echo */
    while (1) {
        k_sleep(K_FOREVER);
    }
}
#endif

#if 0
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(uart_diag, LOG_LEVEL_INF);

#define UART_NODE DT_NODELABEL(uart2)
uint8_t buf[16];
void main(void)
{
    const struct device *uart = DEVICE_DT_GET(UART_NODE);
    size_t got = 0;
    if (!device_is_ready(uart)) {
        LOG_ERR("UART2 not ready");
        return;
    }

    LOG_INF("Starting multi-byte poll echo");

    const char *msg = "Hello\n";
    size_t len = strlen(msg);

    /* Send each byte */
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart, msg[i]);                                                                
    }

    /* Read back with timeout */
    uint8_t rx;
    size_t received = 0;
    int64_t deadline = k_uptime_get() + 1000;

    while (k_uptime_get() < deadline && received < len) {
        if (uart_poll_in(uart, &rx) == 0) {
            //LOG_INF("Echoed[%d]: 0x%02x (%c)", received, rx, rx);
             buf[got++] = rx;
            received++;
        } else {
            k_sleep(K_MSEC(1));
        }
    }
    LOG_INF("Echoed %d bytes: %.*s", got, got, buf);
    if (received < len) {
        LOG_ERR("Timeout: expected %d bytes, got %d", len, received);
    } else {
        LOG_INF("All bytes echoed successfully");
    }
}
#endif

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <string.h>

LOG_MODULE_REGISTER(uart_diag, LOG_LEVEL_INF);

#define UART_NODE DT_NODELABEL(uart2)

/* Message queue to pass bytes from ISR to thread */
K_MSGQ_DEFINE(rx_queue, sizeof(uint8_t), 64, 4);

static const struct device *uart;

/* ISR: read bytes from RX FIFO and enqueue them */
static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uint8_t ch;
        int n = uart_fifo_read(dev, &ch, 1);
        if (n > 0) {
            /* Echo back immediately */
            uart_fifo_fill(dev, &ch, 1);

            /* Queue for logging in thread context */
            k_msgq_put(&rx_queue, &ch, K_NO_WAIT);
        }
    }
}

void main(void)
{
    uart = DEVICE_DT_GET(UART_NODE);
    if (!device_is_ready(uart)) {
        LOG_ERR("UART2 not ready");
        return;
    }

    LOG_INF("Starting interrupt-driven echo (buffered)");

    /* Attach ISR and enable RX interrupts */
    uart_irq_callback_set(uart, uart_isr);
    uart_irq_rx_enable(uart);

    /* Send initial test string */
    const char *msg = "Hello\n";
    uart_fifo_fill(uart, (const uint8_t *)msg, strlen(msg));

    /* Thread loop: dequeue bytes, buffer until newline, then log once */
    char buf[64];
    size_t len = 0;

    while (1) {
    uint8_t ch;
    k_msgq_get(&rx_queue, &ch, K_FOREVER);

        if (ch == '\n') {
            if (len > 0) {
                buf[len] = '\0';
                LOG_INF("Got message: %s", buf);
                len = 0;  // reset buffer
            }
        } else {
            if (len < sizeof(buf) - 1) {
                buf[len++] = ch;
            }
        }
    }
}





