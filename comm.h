#ifndef comm_H
#define comm_H

/*
 * Hardware abstraction for a serial port (comm).
 *
 * Author: Julie Zelenski <zelenski@cs.stanford.edu>
 */

#include <stdbool.h>
#include "comm.h"
#include "interrupts.h"

/*
 * `comm_init`: Required initialization for module
 *
 * Initialize the comm pipeline.
 */
void comm_init(void);

/*
 * `comm_getchar`
 *
 * Obtains the next input character from the serial port and returns it.
 * If receive buffer is empty, will block until next character is received.
 *
 * @return    the character read or EOF if error or at end of input
 */
int comm_getchar(void);

/*
 * `comm_putchar`
 *
 * Outputs a character to the serial port.
 * If send buffer is full, will block until space available.
 *
 * @param ch   the character to write to the serial port
 * @return     the character written
 */
int comm_putchar(int ch);

/*
 * `comm_flush`
 *
 * Flushes any output characters pending in the send buffer.
 */
void comm_flush(void);

/*
 * `comm_haschar`
 *
 * Returns whether there is a character in the receive buffer.
 *
 * @return      true if character ready to be read, false otherwise
 */
bool comm_haschar(void);

/*
 * `comm_putstring`
 *
 * Outputs a string to the serial port by calling `comm_putchar`
 * on each character.
 *
 * @param str  the string to output
 * @return     the count of characters written or EOF if error
 */
int comm_putstring(const char *str);

/*
 * `comm_send`
 *
 * Outputs raw byte to the serial port. `comm_send` outputs the raw byte
 * with no translation (unlike `comm_putchar` which adds processing for
 * converting end-of-line markers). To send text character, use
 * `comm_putchar`; if raw binary data, use `comm_send`.
 *
 * @param byte   the byte to write to the serial port
 */
void comm_send(unsigned char byte);

/*
 * `comm_recv`
 *
 * Obtain raw byte from the serial port. `comm_recv` returns the raw
 * byte with no translation (unlike comm_getchar which adds processing
 * for converting end-of-line and end-of-file markers). To read text
 * character, use `comm_getchar`; if raw binary data, use `comm_recv`.
 *
 * @return   the byte read from the serial port
 */
unsigned char comm_recv(void);

/*
 * `comm_use_interrupts`
 *
 * @param handler   the handler function to call
 * @param client_data  to write to the serial port
 */
void comm_use_interrupts(handlerfn_t handler, void *client_data);

/*
 * `comm_start_error`,`comm_end_error`
 *
 * Output ANSI color code for red+bold on start error, restore to
 * normal on end error. Used by assert/error to highlight error
 * messages.
 */
void comm_start_error(void);
void comm_end_error(void);
void setup_uart_interrupts();
void uart_rx_interrupt_handler();

#endif
