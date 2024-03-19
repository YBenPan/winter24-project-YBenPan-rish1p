/* File: printf.c
 * --------------
 * This file is the implementation of `printf.h` and helper functions `min`, `maax`, `unsigned_to_base`, and `signed_to_base`.
 */
#include "printf.h"
#include <stdarg.h>
#include <stdint.h>
#include "strings.h"
#include "uart.h"

/* Prototypes for internal helpers.
 * Typically these would be qualified as static (private to module)
 * but, in order to call them from the test program, we declare them externally
 */
int unsigned_to_base(char *buf,
                     size_t bufsize,
                     unsigned long val,
                     int base, size_t
                     min_width);
int signed_to_base(char *buf,
                   size_t bufsize,
                   long val,
                   int base,
                   size_t min_width);


#define MAX_OUTPUT_LEN 1024
#define LIMIT (MAX_OUTPUT_LEN * 8) // * 8 just in case of overflow

static int min(int a, int b) {
    /* Returns the minimum of `a` and `b` */
    return a <= b ? a : b;
}

static int max(int a, int b) {
    /* Returns the maximum of `a` and `b` */
    return a >= b ? a : b;
}

int unsigned_to_base(char *buf, size_t bufsize, unsigned long val, int base, size_t min_width) {
    /* Converts `val` (unsigned long in base `base` [10 or 16]) to string. Pad with leading zeroes if length of string is less than `min_width`. Store in `buf` and truncate to fit `bufsize`. */
    if (base != 10 && base != 16) { // unsupported base
        return bufsize;
    }
    char converted[MAX_OUTPUT_LEN]; // stores converted string in reverse
    int n = 0; // n is the loop variable
    while (1) {
        converted[n++] = val % base + (val % base >= 10 ? 'a' - 10 : '0');
        val /= base;
        if (val == 0) break;
    }
    converted[n] = '\0'; // add null-terminator for safety
    // `n` now stores the length of the converted string (not including '\0')
    int n_zeroes = max(0, (int)min_width - n); // number of leading zeroes
    if (bufsize == 0) { 
        return n_zeroes + n;
    }
    int i = 0;                                        
    for (; i < n_zeroes && i < bufsize - 1; i++) { // fill with leading zeroes
        buf[i] = '0';
    }
    for (int j = 0; i + j < bufsize - 1 && i + j < n_zeroes + n; j++) { // fill with converted characters
        buf[i + j] = converted[n - j - 1];
    }
    int cnt = min(bufsize - 1, n_zeroes + n);
    buf[cnt] = '\0';
    return n_zeroes + n;
}

int signed_to_base(char *buf, size_t bufsize, long val, int base, size_t min_width) {
    /* Similar to `unsigned_to_base` but instead with signed value `val`. Calls on `unsigned_to_base` to handle the unsigned part of `val`. */
    if (val >= 0) {
        return unsigned_to_base(buf, bufsize, val, base, min_width);
    }
    if (base != 10 && base != 16) { // unsupported base
        return bufsize;
    }
    if (bufsize == 0) {
        return 0;
    }
    if (bufsize == 1) {
        buf[0] = '\0';
        return 0; 
    }
    buf[0] = '-';
    return 1 + unsigned_to_base(buf + 1, bufsize - 1, -val, base, max(0, min_width - 1));
}

int ipow(int base, int power) {
    // integer only; for float see mathlib.h
    int n = 1;
    for (int i = 0; i < power; i++) n *= base; 
    return n;
}

int float_to_str(char *buf, size_t bufsize, float val, int base, size_t min_width, size_t precision) {
    /* Converts floating point to string based on field with and precision;
     * Supports decimal and hexadecimal */
    if (base != 10 && base != 16) { // unsupported base
        return bufsize;
    }
    if (precision > 6) {
        precision = 6;
    }
    int int_width = precision > 0 ? max(0, (int)min_width - (int)precision - 1) : min_width;
    bool neg = val < 0;
    if (neg) { // add negative sign before rounding
        if (bufsize == 0) {
            return signed_to_base(buf, bufsize, (long)val, base, int_width);
        }
        if (bufsize == 1) {
            buf[0] = '\0';
            return signed_to_base(buf, bufsize, (long)val, base, int_width);
        }
        buf[0] = '-';
        val = -val;
        int_width = max(0, int_width - 1);
    }
    if (precision < 6) { // check rounding
        int digit = (int)(val * ipow(base, precision + 1)) % base;
        if ((base == 10 && digit >= 5) || (base == 16 && digit >= 8)) {
            val = val + 1.0 / ipow(base, precision);
        }
    }
    // convert integer part first
    int ret;
    if (neg) {
        ret = 1 + unsigned_to_base(buf + 1, bufsize - 1, (long)val, base, int_width);
    }
    else {
        ret = unsigned_to_base(buf, bufsize, (long)val, base, int_width);
    }
    if (precision == 0) { // no decimal point
        return ret;
    }
    if (ret + 1 >= bufsize) { // no space for decimal point
        return ret + 1 + precision;
    }
    buf[ret++] = '.';
    // extract relevant decimal parts and assign to int for preciseness
    int x = (long)(val * ipow(base, precision)) % ipow(base, precision); 
    for (int i = 0; i < precision && i + ret < bufsize - 1; i++) { // stores precision part
        buf[ret + i] = (x / ipow(base, precision - i - 1)) + '0'; 
        x %= ipow(base, precision - i - 1);
    }
    int cnt = min(bufsize - 1, ret + precision);
    buf[cnt] = '\0';
    return ret + precision;
}

int vsnprintf(char *buf, size_t bufsize, const char *format, va_list args) {
    /* Processes `format` one character at each time. If we encounter '%', then we process and substitute the corresponding argument into the resulting string. Truncate (if necessary) and store string in `buf`. */
    int cnt = 0;
    char tmp[LIMIT] = {'\0'};
    while (*format) { // processes the string `format` one-by-one
        if (*format == '%') {
            format++;
            if (*format == 'c') { // char
               char s[] = {va_arg(args, int), '\0'}; 
               cnt = strlcat(tmp, s, LIMIT);
            }
            else if (*format == 's') { // string
                cnt = strlcat(tmp, va_arg(args, const char*), LIMIT);    
            }
            else if (*format == '%') { // percent sign
                cnt = strlcat(tmp, "%", LIMIT);
            }
            else if (*format == 'p') { // pointer
                cnt = strlcat(tmp, "0x", LIMIT);
                char str[MAX_OUTPUT_LEN] = {'0'}; // stores converted argument
                unsigned_to_base(str, MAX_OUTPUT_LEN, (unsigned long)va_arg(args, void*), 16, 8);
                cnt = strlcat(tmp, str, LIMIT);
            }
            else { 
                int width = (*format == '0') ? strtonum(format + 1, &format) : 0; // read field width
                int precision = (*format == '.') ? strtonum(format + 1, &format) : 6;                                                                             
                char str[MAX_OUTPUT_LEN] = {'0'}; 
                if (*format == 'l') {
                    format++;
                    if (*format == 'd') {  // long decimal
                        signed_to_base(str, MAX_OUTPUT_LEN, va_arg(args, long), 10, width);
                    }
                    else if (*format == 'x') { // long hex
                        unsigned_to_base(str, MAX_OUTPUT_LEN, va_arg(args, unsigned long), 16, width);
                    }
                }
                else if (*format == 'd') { // decimal
                    signed_to_base(str, MAX_OUTPUT_LEN, va_arg(args, int), 10, width);
                }
                else if (*format == 'x') { // hex
                    unsigned_to_base(str, MAX_OUTPUT_LEN, va_arg(args, unsigned int), 16, width);
                }
                else if (*format == 'f') { // float
                    // va_arg promotes float to double
                    float_to_str(str, MAX_OUTPUT_LEN, va_arg(args, double), 10, width, precision); 
                }
                // TODO: Add lprintf & rprintf here if time permits
                cnt = strlcat(tmp, str, LIMIT); // strlcat returns length of `buf` plus `str` pre-truncation
            }
        }
        else {
            char s[] = {*format, '\0'};
            cnt = strlcat(tmp, s, LIMIT);
        }
        format++;
    }
    if (bufsize == 0) {
        return cnt;
    }
    int lim = min(bufsize - 1, cnt);
    for (int i = 0; i < lim; i++) { // copy tmp to buf up to `lim`
        buf[i] = tmp[i];
    }
    buf[lim] = '\0';
    va_end(args);
    return cnt;
}

int snprintf(char *buf, size_t bufsize, const char *format, ...) {
    /* Accepts variadic function arguments and calls on `vsnprintf` to handle them */
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(buf, bufsize, format, ap);
    va_end(ap);
    return ret;
}

int printf(const char *format, ...) {
    /* Calls on `vsnprintf` to handle variadic function arguments. Output resulting string through serial communication protocols */
    char buf[MAX_OUTPUT_LEN];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, MAX_OUTPUT_LEN, format, ap);
    va_end(ap);
    int ret = uart_putstring(buf); 
    return ret;
}


/* From here to end of file is some sample code and suggested approach
 * for those of you doing the disassemble extension. Otherwise, ignore!
 *
 * The struct insn bitfield is declared using exact same layout as bits are organized in
 * the encoded instruction. Accessing struct.field will extract just the bits
 * apportioned to that field. If you look at the assembly the compiler generates
 * to access a bitfield, you will see it simply masks/shifts for you. Neat!
 */
/*
static const char *reg_names[32] = {"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
                                    "s0/fp", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
                                    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
                                    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6" };

struct insn  {
    uint32_t opcode: 7;
    uint32_t reg_d:  5;
    uint32_t funct3: 3;
    uint32_t reg_s1: 5;
    uint32_t reg_s2: 5;
    uint32_t funct7: 7;
};

void sample_use(unsigned int *addr) {
    struct insn in = *(struct insn *)addr;
    printf("opcode is 0x%x, reg_dst is %s\n", in.opcode, reg_names[in.reg_d]);
}
*/
