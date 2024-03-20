/*
 * COMMUNICATION
 */

#include "uart.h"
#include <stddef.h>
#include "assert.h"
#include "ccu.h"
#include "shell.h"
#include "gpio.h"
#include "mango.h"
#include "gpio_extra.h"
#include "interrupts.h"
#include "printf.h"
#include "ringbuffer.h"
#include "interface.h"

void comm_init(void) {
    uart_init();
    static bool initialized = false;
    if (initialized) error("comm_init() should be called only once.");
    initialized = true;
    uart_putstring("\n\n\n\n");
}

int comm_putstring(const char *str) {
    int n = 0;
    uart_putchar('#');
    uart_putchar('#');
    uart_putchar('#');

    while (str[n]) {
        uart_putchar(str[n++]);
    }
    uart_putchar('#');
    uart_putchar('#');
    uart_putchar('#');

    return n;
}

void uart_rx_interrupt_handler(long unsigned int irq, void *client_data) {
    static int hash_count = 0;
    static bool collecting = false;
    static char message_buffer[1024];
    static int message_index = 0;

    while (uart_haschar()) {
        unsigned char ch = uart_recv();
        
        if (!collecting) {
            if (ch == '#') {
                hash_count++;
                if (hash_count == 3) {
                    collecting = true;
                    message_index = 0; 
                }
            } else {
                hash_count = 0;
            }
        } else {
            if (ch == '#') {
                hash_count++;
                if (hash_count == 3) {
                    message_buffer[message_index] = '\0';
                    comm_putstring(message_buffer);
                    exchange_evaluate(message_buffer); 

                    collecting = false;
                    hash_count = 0;
                    message_index = 0; 
                }
            } else {
                if (message_index < sizeof(message_buffer) - 1) { 
                    message_buffer[message_index++] = ch;
                }
                hash_count = 0;
            }
        }
    }
}



void setup_uart_interrupts() {
    uart_use_interrupts(uart_rx_interrupt_handler, NULL);
    
}
