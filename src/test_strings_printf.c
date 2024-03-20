/* File: test_strings_printf.c
 * ---------------------------
 * This file implements all test functions for the implementation of `strings.h` and `printf.h`.
 */
#include "assert.h"
#include "printf.h"
#include <stddef.h>
#include "strings.h"
#include "uart.h"

// Prototypes copied from printf.c to allow unit testing of helper functions
int unsigned_to_base(char *buf, size_t bufsize, unsigned long val, int base, size_t min_width);
int signed_to_base(char *buf, size_t bufsize, long val, int base, size_t min_width);
int ipow(int base, int power);
int float_to_str(char *buf, size_t bufsize, float val, int base, size_t min_width, size_t precision);

// Constants for ease of testing
#define UNSIGNED_MAX 4294967295
#define SIGNED_MAX 2147483647

static void test_memset(void) {
    char buf[12];
    size_t bufsize = sizeof(buf);

    memset(buf, 0x77, bufsize); // fill buffer with repeating value
    for (int i = 0; i < bufsize; i++)
        assert(buf[i] == 0x77); // confirm value
    
    memset(buf, 0x177, bufsize);
    for (int i = 0; i < bufsize; i++) 
        assert(buf[i] == 0x77); // confirm only LSB was written

    // confirm writing 0 bytes does nothing
    memset(buf, 0x0, 0);
    for (int i = 0; i < bufsize; i++)
        assert(buf[i] == 0x77);
    
    // confirm writing invalid values (i.e., not unsigned char) does nothing
    memset(buf, -1, bufsize);
    for (int i = 0; i < bufsize; i++)
       assert(buf[i] == 0x77); 
    memset(buf, 0x100, bufsize);
    for (int i = 0; i < bufsize; i++)
        assert(buf[i] == 0x77);

    // confirm writing in the middle of `dst`
    int mid = 7;
    memset(buf + mid, 0x10, 5); // 0 - 6: 0x77, 7 - 11: 0x10
    for (int i = 0; i < mid; i++)
        assert(buf[i] == 0x77);
    for (int i = mid; i < bufsize; i++) 
        assert(buf[i] == 0x10);
}

static void test_strcmp(void) {
    assert(strcmp("apple", "apple") == 0);
    assert(strcmp("apple", "applesauce") < 0);
    assert(strcmp("apple", "app") > 0);
    assert(strcmp("pears", "apples") > 0);

    // empty strings
    assert(strcmp("", "") == 0);
    assert(strcmp("", "a") < 0);
    assert(strcmp("a", "") > 0);
}

static void test_strlcat(void) {
    char buf[10];
    size_t bufsize = sizeof(buf);

    // test case 1
    memset(buf, 0x77, bufsize); // init contents with known value

    buf[0] = '\0'; // null in first index makes empty string
    assert(strlen(buf) == 0);
    assert(strlcat(buf, "CS", bufsize) == strlen("CS")); // append CS
    assert(strlen(buf) == 2);
    assert(strcmp(buf, "CS") == 0);
    assert(strlcat(buf, "107e", bufsize) == strlen("CS") + strlen("107e")); // append 107e
    assert(strlen(buf) == 6);
    assert(strcmp(buf, "CS107e") == 0);

    // no null terminator within the first `dstsize` characters
    assert(strlcat(buf, "aaaa", 0) == strlen("aaaa"));
    assert(strcmp(buf, "CS107e") == 0);
    assert(strlcat(buf, "aaaa", strlen(buf)) == strlen(buf) + strlen("aaaa"));
    assert(strcmp(buf, "CS107e") == 0);

    // append bounded by dstsize
    assert(strlcat(buf, " abcdefg", bufsize) == strlen("CS107e") + strlen(" abcdefg"));
    assert(strcmp(buf, "CS107e ab") == 0);

    // test case 2
    memset(buf, 0x41, bufsize); // 'A'
    // `buf` has no null terminator -> does nothing
    assert(strlcat(buf, "abcd", bufsize) == strlen(buf) + strlen("abcd"));
    assert(strcmp(buf, "AAAAAAAAAA") == 0);

    buf[3] = '\0';
    assert(strcmp(buf, "AAA") == 0);
    // append empty string
    assert(strlcat(buf, "", bufsize) == strlen(buf));
    assert(strcmp(buf, "AAA") == 0);
    // append BB
    assert(strlcat(buf, "BB", bufsize) == strlen("AAA") + strlen("BB"));
    assert(strcmp(buf, "AAABB") == 0);
    // append CCCCCCCCCC
    assert(strlcat(buf, "CCCCCCCCCC", bufsize) == strlen("AAABB") + 10);
    assert(strcmp(buf, "AAABBCCCC") == 0);

    // test case 3
    buf[0] = '\0';
    assert(strlen(buf) == 0);
    assert(strlcat(buf, "ab", bufsize) == strlen("ab"));
    assert(strcmp(buf, "ab") == 0);
    assert(strlcat(buf, "cdefgh", bufsize) == strlen("abcdefgh"));
    assert(strcmp(buf, "abcdefgh") == 0);
}

static void test_strtonum(void) {
    unsigned long val = strtonum("013", NULL);
    assert(val == 13);

    const char *input = "107rocks";
    const char *rest = NULL;

    val = strtonum(input, &rest);
    assert(val == 107);
    // rest was modified to point to first non-digit character
    assert(rest == &input[3]);
    
    // test input with hex
    input = "0x107rocks";
    val = strtonum(input, &rest);
    assert(val == 7 * 1 + 1 * 256);
    assert(rest == &input[5]);

    // test input with no valid characters for conversion
    input = "rocks";
    val = strtonum(input, &rest);
    assert(val == 0);
    assert(strcmp(rest, input) == 0);

    // test empty string
    input = "";
    val = strtonum(input, &rest);
    assert(val == 0);
    assert(rest == input);
    assert(*rest == '\0');

    // test input with incomplete hex
    input = "0x";
    val = strtonum(input, &rest);
    assert(val == 0);
    assert(rest == input + 2);
    assert(*rest == '\0');

    // test negative numbers
    input = "-1";
    val = strtonum(input, &rest);
    assert(val == 0);
    assert(rest == input);
 
    // unsigned long limit
    input = "0xffffffff";
    val = strtonum(input, &rest);
    assert(val == UNSIGNED_MAX);
    assert(rest == input + strlen(input));
    assert(*rest == '\0');
    
    input = "0x0";
    val = strtonum(input, &rest);
    assert(val == 0);
    assert(rest == input + strlen(input));
    assert(*rest == '\0');

    // general test cases
    input = "1234567890";
    val = strtonum(input, &rest);
    assert(val == 1234567890);
    assert(rest == input + strlen(input));
    assert(*rest == '\0');

    input = "0xABCDEFGHIJKLMNOPQRSTUVWXYZ";
    val = strtonum(input, &rest);
    assert(val == 11259375);
    assert(rest == input + 8);
    assert(*rest == 'G'); 

    input = "0xabcdefghijklmnopqrstuvwxyz";
    val = strtonum(input, &rest);
    assert(val == 11259375);
    assert(rest == input + 8);
    assert(*rest == 'g');

    input = "ABCDEFGHIJKLMNOPQRSUVWXYZ";
    val = strtonum(input, &rest);
    assert(val == 0);
    assert(rest == input);
    assert(*rest == 'A');

    input = "0x7E8";
    val = strtonum(input, &rest);
    assert(val == 2024);
    assert(rest == input + strlen(input));
    assert(*rest == '\0');

    input = "7E8";
    val = strtonum(input, &rest);
    assert(val == 7);
    assert(rest == input + 1);
    assert(*rest == 'E');

    input = "2024";
    val = strtonum(input, &rest);
    assert(val == 2024);
    assert(rest == input + strlen(input));
    assert(*rest == '\0');
}

static void test_unsigned_to_base(void) {
    char buf[20];
    size_t bufsize = sizeof(buf); 
    memset(buf, 0x77, bufsize);

    /*
    int n = unsigned_to_base(buf, bufsize, 35, 16, 4);
    assert(strcmp(buf, "32") == 0);
    assert(n == 2);

    n = unsigned_to_base(buf, bufsize, 256, 16, 4);
    assert(strcmp(buf, "001") == 0);

    n = unsigned_to_base(buf, bufsize, 0, 16, 4);
    assert(strcmp(buf, "0") == 0);

    n = unsigned_to_base(buf, 4, 3501, 10, 5);
    assert(strcmp(buf, "1053") == 0);
    */

    // Given test cases
    assert(unsigned_to_base(buf, bufsize, 35, 16, 4) == 4);
    assert(strcmp(buf, "0023") == 0);

    assert(unsigned_to_base(buf, bufsize, 35, 10, 0) == 2);
    assert(strcmp(buf, "35") == 0);

    assert(unsigned_to_base(buf, 4, 9999, 10, 5) == 5);
    assert(strcmp(buf, "099") == 0);

    // edge cases
    // val == 0
    assert(unsigned_to_base(buf, 0, 0, 10, 0) == 1);
    assert(unsigned_to_base(buf, 1, 0, 10, 0) == 1);
    assert(buf[0] == '\0');
    assert(unsigned_to_base(buf, 2, 0, 10, 0) == 1);
    assert(buf[0] == '0');
    assert(buf[1] == '\0');
   
    assert(unsigned_to_base(buf, 0, 0, 16, 0) == 1);
    assert(unsigned_to_base(buf, 1, 0, 16, 0) == 1);
    assert(buf[0] == '\0');
    assert(unsigned_to_base(buf, 2, 0, 16, 0) == 1);
    assert(buf[0] == '0');
    assert(buf[1] == '\0');

    // val == UNSIGNED_MAX
    assert(unsigned_to_base(buf, bufsize, UNSIGNED_MAX, 16, 0) == 8);
    assert(strcmp(buf, "ffffffff") == 0);
    assert(unsigned_to_base(buf, bufsize, UNSIGNED_MAX, 10, 0) == 10);
    assert(strcmp(buf, "4294967295") == 0);

    // bufsize == 0
    assert(unsigned_to_base(buf, 0, 2024, 10, 0) == 4);
    assert(unsigned_to_base(buf, 0, 2024, 16, 0) == 3);
    assert(unsigned_to_base(buf, 0, 2024, 10, 5) == 5);

    // bufsize == 1
    for (unsigned long x = 4050; x < 4096; x++) {
       assert(unsigned_to_base(buf, 1, x, 10, 2) == 4);
       assert(buf[0] == '\0');
       assert(unsigned_to_base(buf, 1, x, 16, 2) == 3);
       assert(buf[0] == '\0');
    } 

    // general cases
    //
    // val == 3501
    // 
    // bufsize == 2 
    for (int i = 0; i <= 4; i++) {
        assert(unsigned_to_base(buf, 2, 3501, 10, i) == 4);
        assert(strcmp(buf, "3") == 0);
    }
    for (int i = 5; i <= 20; i++) {
        assert(unsigned_to_base(buf, 2, 3501, 10, i) == i);
        assert(strcmp(buf, "0") == 0);
    }

    // bufsize == 3
    for (int i = 0; i <= 4; i++) {
        assert(unsigned_to_base(buf, 3, 3501, 10, i) == 4);
        assert(strcmp(buf, "35") == 0);
    }
    assert(unsigned_to_base(buf, 3, 3501, 10, 5) == 5);
    assert(strcmp(buf, "03") == 0);
    for (int i = 6; i <= 20; i++) {
        assert(unsigned_to_base(buf, 3, 3501, 10, i) == i);
        assert(strcmp(buf, "00") == 0);
    }
    
    // bufsize == 4
    for (int i = 0; i <= 4; i++) {
        assert(unsigned_to_base(buf, 4, 3501, 10, i) == 4);
        assert(strcmp(buf, "350") == 0);
    }
    assert(unsigned_to_base(buf, 4, 3501, 10, 5) == 5);
    assert(strcmp(buf, "035") == 0);
    assert(unsigned_to_base(buf, 4, 3501, 10, 6) == 6);
    assert(strcmp(buf, "003") == 0);
    for (int i = 7; i <= 20; i++) {
        assert(unsigned_to_base(buf, 4, 3501, 10, i) == i);
        assert(strcmp(buf, "000") == 0);
    }
    
    // bufsize == 5
    for (int i = 0; i <= 4; i++) {
        assert(unsigned_to_base(buf, 5, 3501, 10, i) == 4);
        assert(strcmp(buf, "3501") == 0);
    }
    assert(unsigned_to_base(buf, 5, 3501, 10, 5) == 5);
    assert(strcmp(buf, "0350") == 0);
    assert(unsigned_to_base(buf, 5, 3501, 10, 6) == 6);
    assert(strcmp(buf, "0035") == 0);
    assert(unsigned_to_base(buf, 5, 3501, 10, 7) == 7);
    assert(strcmp(buf, "0003") == 0);
    for (int i = 8; i <= 20; i++) {
        assert(unsigned_to_base(buf, 5, 3501, 10, i) == i);
        assert(strcmp(buf, "0000") == 0);
    }
}

static void test_signed_to_base(void) {
    char buf[20];
    size_t bufsize = sizeof(buf);

    memset(buf, 0x77, bufsize); // init contents with known value

    // Given test cases
    assert(signed_to_base(buf, 5, -9999, 10, 6) == 6);
    assert(strcmp(buf, "-099") == 0);
    assert(signed_to_base(buf, 20, 35, 10, 0) == 2);
    assert(strcmp(buf, "35") == 0);
    assert(signed_to_base(buf, 20, -35, 10, 0) == 3);
    assert(strcmp(buf, "-35") == 0);
    assert(signed_to_base(buf, 20, 35, 16, 4) == 4);
    assert(strcmp(buf, "0023") == 0);
    assert(signed_to_base(buf, 20, -35, 16, 4) == 4);
    assert(strcmp(buf, "-023") == 0);

    // edge cases
    // val == -1
    assert(signed_to_base(buf, 2, -1, 10, 0) == 2);
    assert(strcmp(buf, "-") == 0);
    assert(signed_to_base(buf, 2, -1, 10, 1) == 2);
    assert(strcmp(buf, "-") == 0);
    assert(signed_to_base(buf, 2, -1, 10, 2) == 2);
    assert(strcmp(buf, "-") == 0);
    assert(signed_to_base(buf, 3, -1, 10, 1) == 2);
    assert(strcmp(buf, "-1") == 0);
    assert(signed_to_base(buf, 3, -1, 10, 2) == 2);
    assert(strcmp(buf, "-1") == 0);
    assert(signed_to_base(buf, 3, -1, 10, 3) == 3);
    assert(strcmp(buf, "-0") == 0);
    assert(signed_to_base(buf, 3, -1, 10, 4) == 4);
    assert(strcmp(buf, "-0") == 0);

    // val == 2^31 - 1
    assert(signed_to_base(buf, 9, 0x7fffffff, 16, 0) == 8);
    assert(strcmp(buf, "7fffffff") == 0);
    
    // val == -2^31 + 1
    assert(signed_to_base(buf, 10, -0x7fffffff, 16, 0) == 9);
    assert(strcmp(buf, "-7fffffff") == 0);
    assert(signed_to_base(buf, 9, -0x7fffffff, 16, 0) == 9);
    assert(strcmp(buf, "-7ffffff") == 0);
    assert(signed_to_base(buf, 11, -0x7fffffff, 10, 0) == 11);
    assert(strcmp(buf, "-2147483647"));
    assert(signed_to_base(buf, 5, -0x7fffffff, 10, 0) == 11);
    assert(strcmp(buf, "-214") == 0); 
    assert(signed_to_base(buf, 2, -0x7fffffff, 10, 0) == 11);
    assert(strcmp(buf, "-") == 0);
    assert(signed_to_base(buf, 10, -0x7fffffff, 10, 19) == 19);
    assert(strcmp(buf, "-00000000") == 0);
    assert(signed_to_base(buf, 10, -0x7fffffff, 10, 18) == 18);
    assert(strcmp(buf, "-00000002") == 0);
}

static void test_float_to_str(void) {
    // for final project
    // base 16 untested
    
    // test ipow
    assert(ipow(0, 0) == 1);
    assert(ipow(0, 1) == 0);
    assert(ipow(1, 0) == 1);
    assert(ipow(1, 4) == 1);
    assert(ipow(2, 0) == 1);
    assert(ipow(2, 1) == 2);
    assert(ipow(2, 2) == 4);
    assert(ipow(2, 3) == 8);
    assert(ipow(2, 10) == 1024);
    assert(ipow(3, 0) == 1);
    assert(ipow(3, 1) == 3);
    assert(ipow(3, 2) == 9);
    assert(ipow(3, 3) == 27);
    assert(ipow(3, 4) == 81);
    assert(ipow(10, 4) == 10000);

    char buf[20];
    size_t bufsize = sizeof(buf);

    memset(buf, 0x77, bufsize); // init contents with known value

    // Edge cases
    assert(float_to_str(buf, 0, 0.3, 10, 0, 0) == 1);
    assert(float_to_str(buf, 1, 0.3, 10, 0, 0) == 1);
    assert(strcmp(buf, "") == 0);
    
    // No precision
    assert(float_to_str(buf, bufsize, 0.3, 10, 0, 0) == 1);
    assert(strcmp(buf, "0") == 0);
    assert(float_to_str(buf, bufsize, -0.3, 10, 0, 0) == 2);
    assert(strcmp(buf, "-0") == 0);

    // With precision
    assert(float_to_str(buf, bufsize, -0.123456, 10, 0, 6) == 9);
    assert(strcmp(buf, "-0.123456") == 0);
    assert(float_to_str(buf, bufsize, -0.123456, 10, 15, 6) == 15);
    assert(strcmp(buf, "-0000000.123456") == 0);
    assert(float_to_str(buf, bufsize, -0.123456, 10, 30, 6) == 30);
    assert(strcmp(buf, "-000000000000000000") == 0);
    assert(float_to_str(buf, bufsize, 0.300000, 10, 0, 4) == 6);
    assert(strcmp(buf, "0.3000") == 0);
    assert(float_to_str(buf, bufsize, 0, 10, 0, 4) == 6);
    assert(strcmp(buf, "0.0000") == 0);
    assert(float_to_str(buf, bufsize, -10, 10, 0, 4) == 8);
    assert(strcmp(buf, "-10.0000") == 0);
    assert(float_to_str(buf, bufsize, -8, 10, 4, 4) == 7);
    assert(strcmp(buf, "-8.0000") == 0);
    
    // Rounding
    assert(float_to_str(buf, bufsize, 0.99, 10, 0, 0) == 1);
    assert(strcmp(buf, "1") == 0);
    assert(float_to_str(buf, bufsize, 0.49, 10, 0, 0) == 1);
    assert(strcmp(buf, "0") == 0);
    assert(float_to_str(buf, bufsize, 9.89, 10, 0, 0) == 2);
    assert(strcmp(buf, "10") == 0);
    assert(float_to_str(buf, bufsize, -0.1, 10, 0, 0) == 2);
    assert(strcmp(buf, "-0") == 0);
    assert(float_to_str(buf, bufsize, -1.1, 10, 0, 0) == 2);
    assert(strcmp(buf, "-1") == 0);
    assert(float_to_str(buf, bufsize, -0.89, 10, 0, 0) == 2);
    assert(strcmp(buf, "-1") == 0);
    assert(float_to_str(buf, bufsize, -0.89, 10, 0, 1) == 4);
    assert(strcmp(buf, "-0.9") == 0);
    assert(float_to_str(buf, bufsize, -100.99, 10, 0, 0) == 4);
    assert(strcmp(buf, "-101") == 0);
    assert(float_to_str(buf, bufsize, -100.99, 10, 0, 1) == 6);
    assert(strcmp(buf, "-101.0") == 0);
    assert(float_to_str(buf, bufsize, -100.99, 10, 0, 2) == 7);
    assert(strcmp(buf, "-100.99") == 0);
    assert(float_to_str(buf, bufsize, -100.99, 10, 0, 6) == 11);
    assert(strcmp(buf, "-100.990000") == 0);
    assert(float_to_str(buf, 5, -100.99, 10, 0, 0) == 4);
    assert(strcmp(buf, "-101") == 0);
    assert(float_to_str(buf, 5, -100.99, 10, 0, 1) == 6);
    assert(strcmp(buf, "-101") == 0);
    assert(float_to_str(buf, 5, -100.99, 10, 0, 2) == 7);
    assert(strcmp(buf, "-100") == 0);
    assert(float_to_str(buf, 5, -100.99, 10, 0, 6) == 11);
    assert(strcmp(buf, "-100") == 0);
    assert(float_to_str(buf, bufsize, 100.99, 10, 0, 1) == 5);
    assert(strcmp(buf, "101.0") == 0);
    assert(float_to_str(buf, bufsize, 100.99, 10, 0, 6) == 10);
    assert(strcmp(buf, "100.990000") == 0);
    assert(float_to_str(buf, 5, 100.99, 10, 0, 0) == 3);
    assert(strcmp(buf, "101") == 0);
    assert(float_to_str(buf, 5, 100.99, 10, 0, 1) == 5);
    assert(strcmp(buf, "101.") == 0);
}

static void test_snprintf(void) {
    char buf[200];
    size_t bufsize = sizeof(buf);

    memset(buf, 0x77, bufsize); // init contents with known value

    // Edge cases
    // bufsize == 0
    assert(snprintf(buf, 0, "%s, %s", "Lorem", "Ipsum") == 12);
    assert(buf[0] == 'w'); // buf unchanged
    assert(snprintf(buf, 0, "%c", 'K') == 1);
    assert(buf[0] == 'w'); 
    assert(snprintf(buf, 0, "%s", "Lorem Ipsum") == 11);
    assert(buf[0] == 'w');
    assert(snprintf(buf, 0, "%d", -1) == 2);
    assert(buf[0] == 'w'); 
    assert(snprintf(buf, 0, "%ld", (long)(-SIGNED_MAX)) == 11);
    assert(buf[0] == 'w'); 
    assert(snprintf(buf, 0, "%x", 0x2000040) == 7);
    assert(buf[0] == 'w');
    assert(snprintf(buf, 0, "%lx", (unsigned long)(0xffffffff)) == 8);
    assert(buf[0] == 'w'); 
    assert(snprintf(buf, 0, "%p", (void *)(0x2000040)) == 10);
    assert(buf[0] == 'w');
    
    // bufsize == 1
    assert(snprintf(buf, 1, "%s, %s", "Lorem", "Ipsum") == 12);
    assert(buf[0] == '\0'); 
    assert(buf[1] == 'w');
    assert(snprintf(buf, 1, "%c", 'K') == 1);
    assert(buf[0] == '\0'); 
    assert(buf[1] == 'w');
    assert(snprintf(buf, 1, "%s", "Lorem Ipsum") == 11);
    assert(buf[0] == '\0');
    assert(buf[1] == 'w');
    assert(snprintf(buf, 1, "%d", -1) == 2);
    assert(buf[0] == '\0'); 
    assert(buf[1] == 'w');
    assert(snprintf(buf, 1, "%ld", (long)(-SIGNED_MAX)) == 11);
    assert(buf[0] == '\0'); 
    assert(buf[1] == 'w');
    assert(snprintf(buf, 1, "%x", 0x2000040) == 7);
    assert(buf[0] == '\0');
    assert(buf[1] == 'w');
    assert(snprintf(buf, 1, "%lx", (unsigned long)(0xffffffff)) == 8);
    assert(buf[0] == '\0'); 
    assert(buf[1] == 'w');
    assert(snprintf(buf, 1, "%p", (void *)(0x2000040)) == 10);
    assert(buf[0] == '\0');
    assert(buf[1] == 'w');
    
    // No formatting
    assert(snprintf(buf, bufsize, "Hello, world!") == 13);
    assert(strcmp(buf, "Hello, world!") == 0);
    assert(snprintf(buf, bufsize, "Hello") == 5);
    assert(snprintf(buf, 2, "Hello") == 5);

    // Character
    assert(snprintf(buf, bufsize, "%c", 'A') == 1);
    assert(strcmp(buf, "A") == 0);
    assert(snprintf(buf, bufsize, "H%c%c%co, %%%%%%%corld%c", 'e', 'l', 'l', 'w', '!') == 16);
    assert(strcmp(buf, "Hello, %%%world!") == 0);
    assert(snprintf(buf, 10, "Hello, %c%c%c%c%c%c", 'w', 'o', 'r', 'l', 'd', '!') == 13);
    assert(strcmp(buf, "Hello, wo") == 0);

    // String
    assert(snprintf(buf, bufsize, "%s", "binky") == 5);
    assert(strcmp(buf, "binky") == 0);
    assert(snprintf(buf, bufsize, "%s, %s", "Lorem", "Ipsum") == 12);
    assert(strcmp(buf, "Lorem, Ipsum") == 0);
    assert(snprintf(buf, 8, "%s, %s", "Lorem", "Ipsum") == 12);
    assert(strcmp(buf, "Lorem, ") == 0); 
    
    // Decimal
    assert(snprintf(buf, bufsize, "%d", 45) == 2);
    assert(strcmp(buf, "45") == 0);
    assert(snprintf(buf, bufsize, "%ld", (long)SIGNED_MAX) == 10);
    assert(strcmp(buf, "2147483647") == 0);

    // Signed Decimal
    assert(snprintf(buf, bufsize, "%d", -45) == 3);
    assert(strcmp(buf, "-45") == 0);
    assert(snprintf(buf, bufsize, "%d%d", -45, -90) == 6);
    assert(strcmp(buf, "-45-90") == 0);
    assert(snprintf(buf, bufsize, "%ld", (long)(-1)) == 2);
    assert(strcmp(buf, "-1") == 0);
    assert(snprintf(buf, bufsize, "%ld = %ld", (long)(-SIGNED_MAX), (long)(-SIGNED_MAX)) == 25);
    assert(strcmp(buf, "-2147483647 = -2147483647") == 0);
    assert(snprintf(buf, 12, "%ld = %ld", (long)(-SIGNED_MAX), (long)(-SIGNED_MAX)) == 25);
    assert(strcmp(buf, "-2147483647") == 0);

    // Signed Decimal with Padding
    assert(snprintf(buf, bufsize, "%016ld", (long)(-SIGNED_MAX)) == 16);
    assert(strcmp(buf, "-000002147483647") == 0);
    assert(snprintf(buf, bufsize, "%01ld%02ld%03ld%04ld%05ld%06ld%07ld%08ld%09ld%010ld%011ld", (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX)) == 121);
    assert(strcmp(buf, "-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647") == 0);
    assert(snprintf(buf, bufsize, "%012ld%013ld%014ld%015ld", (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX)) == 54);
    assert(strcmp(buf, "-02147483647-002147483647-0002147483647-00002147483647") == 0);

    // Hexadecimal
    assert(snprintf(buf, bufsize, "%04x", 0xef) == 4);
    assert(strcmp(buf, "00ef") == 0);
    assert(snprintf(buf, bufsize, "%lx", ~0L) == 16);
    assert(strcmp(buf, "ffffffffffffffff") == 0);
    
    // Pointer
    assert(snprintf(buf, bufsize, "%p", (void *)0x20200004) == 10);
    assert(strcmp(buf, "0x20200004") == 0);
    assert(snprintf(buf, bufsize, "%p %p", (void *)0x20200004, (void *)0x20200003) == 21);
    assert(strcmp(buf, "0x20200004 0x20200003") == 0);
    assert(snprintf(buf, bufsize, "%p", (void *)0x0) == 10);
    assert(strcmp(buf, "0x00000000") == 0);
    assert(snprintf(buf, 20, "%p", (void *)0x12340) == 10);
    assert(strcmp(buf, "0x00012340") == 0);
    assert(snprintf(buf, 20, "%p", (void *)0x100200300400L) == 14);
    assert(strcmp(buf, "0x100200300400") == 0);
    assert(snprintf(buf, 20, "%p", (void *)0x7f94) == 10);
    assert(strcmp(buf, "0x00007f94") == 0);

    // General cases
    assert(snprintf(buf, bufsize, "%d + %d = %d", -1, -1, -1) == 12);
    assert(strcmp(buf, "-1 + -1 = -1") == 0);
    assert(snprintf(buf, bufsize, "%c%%%s%%%02d%%%02x%%%03ld%%%03lx%%%p", 'A', "str", (int)(-1), (unsigned int)(0xf), (long)(-0x7ffffffe), (unsigned long)(0x7ffffffd), (void *)(0x2000000)) == 43);
    assert(strcmp(buf, "A%str%-1%0f%-2147483646%7ffffffd%0x02000000") == 0);
    assert(snprintf(buf, bufsize, "CS%d%c!", 107, 'e') == 7);
    assert(strcmp(buf, "CS107e!") == 0);

    // Test if `a` is output correctly
    assert(snprintf(buf, bufsize, "%08x", 0xa01c) == 8);
    assert(strcmp(buf, "0000a01c") == 0);
    assert(snprintf(buf, bufsize, "%x", 0x9abcdef0) == 8);
    assert(strcmp(buf, "9abcdef0") == 0);
}

void test_snprintf_float(void) {
    char buf[200];
    size_t bufsize = sizeof(buf);
    assert(snprintf(buf, bufsize, "%f", 0.123456) == 8);
    assert(strcmp(buf, "0.123456") == 0);

    // Precision only
    assert(snprintf(buf, bufsize, "%.f", 0.123456) == 1);
    assert(strcmp(buf, "0") == 0);
    assert(snprintf(buf, bufsize, "%.0f", 0.123456) == 1);
    assert(strcmp(buf, "0") == 0);
    assert(snprintf(buf, bufsize, "%.2f", 0.123456) == 4);
    assert(strcmp(buf, "0.12") == 0);
    assert(snprintf(buf, bufsize, "%.5f", 0.123456) == 7);
    assert(strcmp(buf, "0.12346") == 0);

    // Field width only
    assert(snprintf(buf, bufsize, "%05f", 0.123456) == 8);
    assert(strcmp(buf, "0.123456") == 0);
    assert(snprintf(buf, bufsize, "%01.f", 0.123456) == 1);
    assert(strcmp(buf, "0") == 0);
    assert(snprintf(buf, bufsize, "%010.f", 0.123456) == 10);
    assert(strcmp(buf, "0000000000") ==0);
    assert(snprintf(buf, bufsize, "%010f", 0.123456) == 10);
    assert(strcmp(buf, "000.123456") == 0);
    assert(snprintf(buf, bufsize, "%01f", 0.123456) == 8);
    assert(strcmp(buf, "0.123456") == 0);

    // Both precision and field width
    assert(snprintf(buf, bufsize, "%010.2f", 0.123456) == 10);
    assert(strcmp(buf, "0000000.12") == 0);
    assert(snprintf(buf, bufsize, "%010.5f", 0.123456) == 10);
    assert(strcmp(buf, "0000.12346") == 0);

    // Integers
    assert(snprintf(buf, bufsize, "%f", (float)0) == 8);
    assert(strcmp(buf, "0.000000") == 0);
    assert(snprintf(buf, bufsize, "%05.f", (float)2) == 5);
    assert(strcmp(buf, "00002") == 0);
    assert(snprintf(buf, bufsize, "%05.4f", (float)2) == 6);
    assert(strcmp(buf, "2.0000") == 0);
    assert(snprintf(buf, bufsize, "%02f", (float)24) == 9);
    assert(strcmp(buf, "24.000000") == 0);

    // Negative numbers
    assert(snprintf(buf, bufsize, "%f", (float)(-24)) == 10);
    assert(strcmp(buf, "-24.000000") == 0);
    assert(snprintf(buf, bufsize, "%.f", -0.56789) == 2);
    assert(strcmp(buf, "-1") == 0);
    assert(snprintf(buf, bufsize, "%.4f", -0.56789) == 7);
    assert(strcmp(buf, "-0.5679") == 0);
    assert(snprintf(buf, bufsize, "%010f", -0.56789) == 10);
    assert(strcmp(buf, "-00.567890") == 0);
    assert(snprintf(buf, bufsize, "%010.3f", -0.56789) == 10);
    assert(strcmp(buf, "-00000.568") == 0);
    assert(snprintf(buf, bufsize, "%010.5f", -0.999999) == 10);
    assert(strcmp(buf, "-001.00000") == 0);

    // General cases
    assert(snprintf(buf, bufsize, "%05.3f", 1.4) == 5);
    assert(strcmp(buf, "1.400") == 0);
    assert(snprintf(buf, bufsize, "%05f", 1.4) == 8);
    assert(strcmp(buf, "1.400000") == 0);
    assert(snprintf(buf, bufsize, "%05.3f", 1.8) == 5);
    assert(strcmp(buf, "1.800") == 0);
    assert(snprintf(buf, bufsize, "%05.f", 1.8) == 5);
    assert(strcmp(buf, "00002") == 0);
    assert(snprintf(buf, bufsize, "%05.1f", 1.8) == 5);
    assert(strcmp(buf, "001.8") == 0);
    assert(snprintf(buf, bufsize, "%05.2f", 1.8) == 5);
    assert(strcmp(buf, "01.80") == 0);
}

void test_printf(void) {
    assert(printf("Hello, World!\n") == 14);
   
    // Character
    printf("EXPECTED: Lorem, Ipsum\n");
    printf("RECEIVED: ");
    assert(printf("Lorem%c Ipsum%c", ',', '\n') == 13);
    printf("\n");
    printf("EXPECTED: United States of America!\n");
    printf("RECEIVED: ");
    assert(printf("United%cStates%cof%cAmerica%c%c", ' ', ' ', ' ', '!', '\n') == 26);
      
    // Decimal
    printf("EXPECTED: 107e\n");
    printf("RECEIVED: ");
    assert(printf("%de\n", 107) == 5);
    printf("EXPECTED: 2147483647\n");
    printf("RECEIVED: ");
    assert(printf("%ld\n", (long)SIGNED_MAX) == 11);

    // Signed Decimal
    printf("EXPECTED: -107e\n");
    printf("RECEIVED: "); 
    assert(printf("%de\n", -107) == 6);
    printf("EXPECTED: -45-90\n");
    printf("RECEIVED: ");
    assert(printf("%d%d\n", -45, -90) == 7);
    printf("EXPECTED: -2\n");
    printf("RECEIVED: ");
    assert(printf("%ld\n", (long)(-2)) == 3);
    printf("EXPECTED: -2147483647 = -2147483647\n");
    printf("RECEIVED: ");
    assert(printf("%ld = %ld\n", (long)(-SIGNED_MAX), (long)(-SIGNED_MAX)) == 26);

    // Signed Decimal with Padding
    printf("EXPECTED: -000002147483647\n");
    printf("RECEIVED: ");
    assert(printf("%016ld\n", (long)(-SIGNED_MAX)) == 17);
    printf("EXPECTED: -2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647-2147483647\n");
    printf("RECEIVED: ");
    assert(printf("%01ld%02ld%03ld%04ld%05ld%06ld%07ld%08ld%09ld%010ld%011ld\n", (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX)) == 122);
    printf("EXPECTED: -02147483647-002147483647-0002147483647-00002147483647\n");
    printf("RECEIVED: ");
    assert(printf("%012ld%013ld%014ld%015ld\n", (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX), (long)(-SIGNED_MAX)) == 55);

    // Hexadecimal
    printf("EXPECTED: 000def\n");
    printf("RECEIVED: ");
    assert(printf("%06x\n", 0xdef) == 7);
    printf("EXPECTED: ffffffffffffffff\n");
    printf("RECEIVED: ");
    assert(printf("%lx\n", ~0L) == 17);
    
    // Pointer
    printf("EXPECTED: 0x20200004\n");
    printf("RECEIVED: ");
    assert(printf("%p\n", (void *)0x20200004) == 11);
    printf("EXPECTED: 0x0\n");
    printf("RECEIVED: ");
    assert(printf("%p\n", (void *)0x0) == 11);

    // General cases
    printf("EXPECTED: A%%str%%-1%%0f%%-2147483646%%7ffffffd%%0x2000000\n");
    printf("RECEIVED: ");
    assert(printf("%c%%%s%%%02d%%%02x%%%03ld%%%03lx%%%p\n", 'A', "str", (int)(-1), (unsigned int)(0xf), (long)(-0x7ffffffe), (unsigned long)(0x7ffffffd), (void *)(0x2000000)) == 44);
    printf("EXPECTED: CS107e!\n");
    printf("RECEIVED: ");
    assert(printf("CS%d%c!\n", 107, 'e') == 8);

    printf("\n");
    printf("Congratulations! All tests passed.\n");
}

// This function just here as code to disassemble for extension
int sum(int n) {
    int result = 6;
    for (int i = 0; i < n; i++) {
        result += i * 3;
    }
    return result + 729;
}

void test_disassemble(void) {
    const unsigned int add = 0x00f706b3;
    const unsigned int xori = 0x0015c593;
    const unsigned int bne = 0xfe061ce3;
    const unsigned int sd = 0x02113423;

    // If you have not implemented the extension, core printf
    // will output address not disassembled followed by I
    // e.g.  "... disassembles to 0x07ffffd4I"
    printf("Encoded instruction %x disassembles to %pI\n", add, &add);
    printf("Encoded instruction %x disassembles to %pI\n", xori, &xori);
    printf("Encoded instruction %x disassembles to %pI\n", bne, &bne);
    printf("Encoded instruction %x disassembles to %pI\n", sd, &sd);

    unsigned int *fn = (unsigned int *)sum; // disassemble instructions from sum function
    for (int i = 0; i < 10; i++) {
        printf("%p:  %x  %pI\n", &fn[i], fn[i], &fn[i]);
    }
}


void main(void) {
    uart_init();
    uart_putstring("Start execute main() in test_strings_printf.c\n");

    // test_memset();
    // test_strcmp();
    // test_strlcat();
    // test_strtonum();
    // test_unsigned_to_base();
    // test_signed_to_base();
    test_float_to_str();
    // test_snprintf();
    test_snprintf_float();
    // test_printf();
    // test_disassemble();

    uart_putstring("Successfully finished executing main() in test_strings_printf.c\n");
}
