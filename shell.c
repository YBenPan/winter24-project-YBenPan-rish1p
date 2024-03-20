/* File: shell.c
 * -------------
 * This file contains an implementation of shell terminal for the Mango Pi!
 * CITATION: I used lab 4 example 2 for some of the helper functions for evaluate
 * EXTENSION: I did both extensions. 
 * EXTENSION(2): I did the profiler extension.
 * CITATION 2(After doing symtab extension/profiler) The citation and instructions for running from assign4 still apply here
 */
#include "shell.h"
#include "shell_commands.h"
#include "uart.h"
#include "strings.h"
#include "keyboard.h"
#include "mango.h"
#include "hstimer.h"
#include "symtab.h"
#include "malloc.h"
#include "comm.h"
#include "assert.h"

#define LINE_LEN 80

// Module-level global variables for shell
 static struct {
    input_fn_t shell_read;
    formatted_fn_t shell_printf;
} module;

// NOTE TO STUDENTS:
// Your shell commands output various information and respond to user
// error with helpful messages. The specific wording and format of
// these messages would not generally be of great importance, but
// in order to streamline grading, we ask that you aim to match the
// output of the reference version.
//
// The behavior of the shell commands is documented in "shell_commands.h"
// https://cs107e.github.io/header#shell_commands
// The header file gives example output and error messages for all
// commands of the reference shell. Please match this wording and format.
//
// Your graders thank you in advance for taking this care!

static int count = 0; //Holds number of commands
static char *history_array[10] = {0}; //Array for command history.
typedef struct {
    uintptr_t address;
    unsigned int count;
} instruction_address_t;

instruction_address_t all_addresses[2048];
instruction_address_t top_20[20];
unsigned int address_count = 0; // Counter for the number of different addresses found

//This function initializes the array with the top 20 elements.
void init_top_20(void) {
    for (int i = 0; i < 20; i++) {
        top_20[i].address = 0;
        top_20[i].count = 0;
    }
}





//This helper function updates the top 20 values in the top_20 array!
//It first inserts it, then it sorts it accordingly linearly.
void update_top_20(uintptr_t pc, unsigned int count) {
    int index = -1;

    for (int i = 0; i < 20; i++) //Checking to see if it already exists
    {
        if (top_20[i].address == pc) 
        {
            top_20[i].count = count;
            index = i;
            break;
        }
    }

    if (index == -1) //When it doesn't exist, replace it with smallest value in array.
    {
        if (count > top_20[19].count) 
        {
            top_20[19].address = pc;
            top_20[19].count = count;

        }
    }

    
    for (int i = 0; i < 19; i++) //Linearly sort the items.
    {
        for (int j = i + 1; j < 20; j++) 
        {
            if (top_20[i].count < top_20[j].count) 
            {
                instruction_address_t temp = top_20[i];
                top_20[i] = top_20[j];
                top_20[j] = temp;
            }
        }
    }
}

//This is a handler function for the profiler
//It adds the function to the all address array, as well as updates it in the top20
void profiler_handler(uintptr_t pc, void *aux_data) {

    for (unsigned int i = 0; i < address_count; i++) 
    {
        if (all_addresses[i].address == pc) 
        {
            all_addresses[i].count += 1;
            if(all_addresses[i].count > top_20[19].count) 
            {
            update_top_20(all_addresses[i].address, all_addresses[i].count);
            hstimer_interrupt_clear(HSTIMER0);
            }
            return;
        }
    }

    if (address_count < 2048) //Special case when there are less than 2048 addresses
    {
        all_addresses[address_count].address = pc;
        all_addresses[address_count].count = 1;
        address_count++;
        if (address_count <= 20 || 1 > top_20[19].count) 
        {
            update_top_20(pc, 1);
        }

    }
    hstimer_interrupt_clear(HSTIMER0);

}

//This helper function initializes the profiler, with a hstimer time of 500 microseconds.
void profiler_init(void) {
    hstimer_init(HSTIMER0, 500); 
    interrupts_register_handler(INTERRUPT_SOURCE_HSTIMER0, profiler_handler, NULL); 
    interrupts_enable_source(INTERRUPT_SOURCE_HSTIMER0);
}

//When called, this function will enable the profiler/global interrupts
void profiler_enable(void) {
    profiler_init();
    init_top_20();
    interrupts_enable_source(INTERRUPT_SOURCE_HSTIMER0); // Enable timer interrupt
    hstimer_enable(HSTIMER0);
    interrupts_global_enable();
}

//This function will disable the profiler and print the output using symtab
void profiler_disable(void) {
    hstimer_disable(HSTIMER0);
    interrupts_disable_source(INTERRUPT_SOURCE_HSTIMER0);
    // interrupts_global_disable();
    char label[128];
    module.shell_printf("  Counts  |  Function    [pc]\n");
    module.shell_printf("-----------------------------\n");
    for (int i = 0; i < 20; i++) 
    {
	symtab_label_for_addr(label, sizeof(label), top_20[i].address);//Storing the text in label
    module.shell_printf("%d | address: %s | pc:[0x%lx] \n",top_20[i].count, label, (unsigned long)top_20[i].address );
}
return;
}



//This function enables/disables a profiler in the system, for measuring memory hot-spots.
int cmd_profile(int argc, const char *argv[]) {
    if (argc != 2) {
        module.shell_printf("error: profile expects 1 argument [on|off]\n");
        return -1;
    }
    if (strcmp(argv[1], "on") == 0) {
        profiler_enable();
        module.shell_printf("Profiling has started!.\n");
    } else if (strcmp(argv[1], "off") == 0) {
        profiler_disable();
        module.shell_printf("Profiling has ended!\n");
    } else {
        module.shell_printf("invalid entry.\n");
        return -1;
    }
    return 0;
}


int cmd_comm(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++)
    {
        comm_putstring(argv[i]);
        module.shell_printf("%s ", argv[i]);
    }

        module.shell_printf("\n");
    return 0;
}


typedef struct {
    const char *name;
    const char *usage;
    const char *description;
} option_t;

static const option_t options[] = {
    {"buy",  "buy <stockcode>",  "buys a stock with a given stockcode"},
    {"sell",  "sell <stockcode>",  "sells a stock with a given stockcode"},
    {"price",  "price <stockcode>",  "return price a stock with a given stockcode"},
    {"graph",  "graph <stockcode>",  "graphs a price a stock with a given stockcode"},
    {"pnl",  "pnl <stockcode>",  "returns how much money you have (stonks!)"},
    {"bankruptcy",  "bankruptcy [please]", "declares bankruptcy! we just print more money and get rid of your debt"},
    {"profile", "profile [on] [off]", "measures hot-spots by address/memory execution"},
};

int cmd_options(int argc, const char *argv[]);
static const command_t commands[] = {
    {"help",  "help [cmd]",  "print command usage and description", cmd_help},
    {"echo",  "echo [args]", "print arguments", cmd_echo},
    {"reboot", "reboot","reboot the mango Pi", cmd_reboot},
    {"clear", "clear","clear screen (if your terminal supports it)", cmd_clear},
    {"peek",  "peek [addr]", "print contents of memory at address", cmd_peek},
    {"poke",  "poke [addr] [val]", "store value into memory at address", cmd_poke},
    {"profile", "profile [on] [off]", "measures hot-spots by address/memory execution", cmd_profile},
    {"comm", "comm [send]", "communicates", cmd_comm},
    {"options", "options", "what are your life options?", cmd_options}
};


int cmd_options(int argc, const char *argv[]) {
        for (int i = 0; i < 7; i++) {
            module.shell_printf("%s\t- %s\n", options[i].name, options[i].description);
        }
    return 0;
}
//This function prints out all commands or searches through them respectively.
int cmd_help(int argc, const char *argv[]) {
    if (argc == 1) { //Print all commands
        for (int i = 0; i < 8; i++) {
            module.shell_printf("%s\t- %s\n", commands[i].name, commands[i].description);
        }
    } else { // Searching through commands
        for (int i = 0; i < 8; i++) {
            if (strcmp(argv[1], commands[i].name) == 0) {
                module.shell_printf("%s\t   %s\n", commands[i].usage, commands[i].description);
                return 0;
            }
        }
        module.shell_printf("error: no such command '%s'.\n", argv[1]);//No command found
        return -1;
    }
    return 0;
}

int cmd_echo(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++)
        module.shell_printf("%s ", argv[i]);
    module.shell_printf("\n");
    return 0;
}

//This function reboots the Mango Pi
int cmd_reboot(int argc, const char *argv[]) {
    mango_reboot(); //Calling the mango.h reboot function
    return 0;
}

int cmd_clear(int argc, const char* argv[]) {
    //const char *ANSI_CLEAR = "\033[2J"; // if your terminal does not handle formfeed, can try this alternative?

    module.shell_printf("\f");   // minicom will correctly respond to formfeed character
    return 0;
}
//This function peeks at a specified address in the terminal
//It has safe checks for ensuring the correct number of arguments
//As well as the address is alligned.
int cmd_peek(int argc, const char *argv[]) {
    const char *end;
    unsigned long addr = strtonum(argv[1], &end);
    if (argc != 2) 
    {
        module.shell_printf("error: peek expects 1 argument [addr]\n");
        return -1;
    }

    if (*end != '\0') //Special case for non address entry (like the word bob in example)
    {
        module.shell_printf("error: peek cannot convert '%s'\n", argv[1]);
        return -1;
    }

    if (addr % 4 != 0) 
    {
        module.shell_printf("error: peek address must be 4-byte aligned\n");
        return -1;
    }

    uint32_t addr_val = *(volatile uint32_t *)(uintptr_t)addr;//Storing value from address

    module.shell_printf("0x%08lx: %08x\n", addr, addr_val);

    return 0;
}

//This function inserts a value at a given address
//It has safe checks for ensuring the correct number of arguments
//As well as appropriate conversion/allignment.
int cmd_poke(int argc, const char *argv[]) {
    const char *end;
    unsigned long addr = strtonum(argv[1], &end);
    if (argc != 3) {
        module.shell_printf("error: poke expects 2 arguments [addr] and [val]\n");
        return -1;
    }

    if (*end != '\0') { //Special case for non address entry (like the word bob in example)
        module.shell_printf("error: poke cannot convert '%s'\n", argv[1]);
        return -1;
    }

    unsigned long val = strtonum(argv[2], &end);
    if (*end != '\0') {
        module.shell_printf("error: poke cannot convert '%s'\n", argv[2]);
        return -1;
    }

    if (addr % 4 != 0) {
        module.shell_printf("error: poke address must be 4-byte aligned\n");
        return -1;
    }

    *(volatile uint32_t *)(uintptr_t)addr = (uint32_t)val;//Storing in the appropriate location

    return 0;//Return to confirm it is done.
}



void shell_init(input_fn_t read_fn, formatted_fn_t print_fn) {
    module.shell_read = read_fn;
    module.shell_printf = print_fn;
}

void shell_bell(void) {
    uart_putchar('\a');
}

// This function prints/accepts input for a command line appropriately.
void shell_readline(char buf[], size_t bufsize) {
    size_t length = 0;
    size_t cursor = 0;
    int pos = count;
    memset(buf, 0, bufsize);

    while (1) {
        unsigned char ch = module.shell_read(); // Read next character
        if (ch == '\n') { // Enter key case
            module.shell_printf("\n");
            break;
        } else if (ch == '\b' || ch == 0x7F) 
        { // Backspace
            if (cursor > 0) {
module.shell_printf("%c", '\b');
module.shell_printf(" ");
module.shell_printf("%c", '\b');
                memcpy(buf + cursor - 1, buf + cursor, length - cursor);
                cursor--;
                length--;
                //reprint();
            } else {
                shell_bell();
            }
        } else if (ch == 0x01) 
        { // CTRL A go to front
            cursor = 0;
            //reprint();
        } else if (ch == 0x05) 
        { // CTRL E go to end
            cursor = length;
            //reprint();
        } else if (ch == 0x15) 
        { // CTRL U delete line
            memset(buf, 0, bufsize);
            length = 0;
            cursor = 0;
            //reprint();
        } else if (ch >= 32 && ch < 127 && length < bufsize - 1) 
        { // Printing regular ASCII
            memcpy(buf + cursor + 1, buf + cursor, length - cursor); // Shifting right appropriately
            buf[cursor] = ch;
            module.shell_printf("%c",ch);
            length++;
            cursor++;
            //reprint();
        }
        if (ch == PS2_KEY_ARROW_UP || ch == PS2_KEY_ARROW_DOWN) 
        {
            int updated_pos;
            if (ch == PS2_KEY_ARROW_UP) {//Whether moving up or down
                updated_pos = pos - 1;
            } else {
                updated_pos = pos + 1;
            }
            if ((updated_pos >= -1) && (updated_pos < 10) && (updated_pos < count)) 
            {
                memset(buf, 0, bufsize);
                pos = updated_pos;
                if (pos >= 0) 
                {
                    const char* command = history_array[pos % 10];
                    size_t command_length = strlen(command);
                    memcpy(buf, command, command_length);
                    length = strlen(buf);
                } else 
                {
                    length = 0;
                }
                //reprint();
                cursor = length;
            } else 
            {
                shell_bell();
            }
            continue;
        }
    }

    buf[length] = '\0'; // Null terminating at end
}

//CITATION: This function is from lab 4 exercise 2
static char *strndup(const char *src, size_t n) {
    size_t len = strlen(src);
    if (len > n) {
        len = n; 
    }

    char *x = (char *)malloc(len + 1);
    if (x == NULL) {
        return NULL; 
    }

    memcpy(x, src, len);
    x[len] = '\0';

    return x; 
}

//CITATION: This function is from lab 4 exercise 2
static bool isspace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n';
}

//CITATION: This function is from lab 4 exercise 2
static int tokenize(const char *line, char *array[],  int max) {
    int num_tokens = 0;
    const char *cur = line;

    while (num_tokens < max) {
        while (isspace(*cur)) cur++;    // skip spaces (stop non-space/null)
        if (*cur == '\0') break;        // no more non-space chars
        const char *start = cur;
        while (*cur != '\0' && !isspace(*cur)) cur++; // advance to end (stop space/null)
        array[num_tokens++] = strndup(start, cur - start);   // make heap-copy, add to array
    }
    return num_tokens;
}


//This function evaluates the input against a 
//List of commands in the command array
int shell_evaluate(const char *line) {
    char *tokens[100];
    int num_tokens = tokenize(line, tokens, 100);

    if (num_tokens == 0) {
        return -1; 
    }

    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(tokens[0], commands[i].name) == 0) {
            int result = commands[i].fn(num_tokens, (const char **)tokens);
            for (int j = 0; j < num_tokens; ++j) {
                free(tokens[j]);//Freeing associated token to avoid memory leak
            }

            return result;
        }
    }
    module.shell_printf("error: no such command '%s'.\n", tokens[0]);

    // Free the allocated tokens
    for (int j = 0; j < num_tokens; ++j) {
        free(tokens[j]);
    }

    return -1;
}

//This function adds the appropriate command to the history array.
void history_append(const char *command) {
    int relative_index = count % 10;//Using modulo to get relative position of index even after filled.

    if (history_array[relative_index]) {//Free old command at index
        free(history_array[relative_index]);
    }

    history_array[relative_index] = strndup(command, LINE_LEN);

}

//This function prints the last 10 items in the history
void print_history() {
    int start;
    if (count > 10) 
    {
        start = count - 10;//Checking the start position
    } else 
    {
        start = 0;
    }

    for (int i = start; i < count; i++) {
        int index = i % 10;
        module.shell_printf("[%d] %s\n", i + 1, history_array[index]);
    }
}


void shell_run(void) {
    //interrupts_init(); //TURN OFF WHEN RUNNING MAIN
    interrupts_global_enable();

    // Setup UART to use interrupts for incoming characters
    setup_uart_interrupts();

    module.shell_printf("Welcome to the CS107E shell. Remember to type on your PS/2 keyboard!\n");
    while (1)
    {
        char line[LINE_LEN];
        count++;
        module.shell_printf("[%d] Pi> ",count);
        shell_readline(line, sizeof(line));
        history_append(line);
        if (strcmp(line, "history") == 0) {
            print_history();
        }
       else
       {
       shell_evaluate(line);
       }
    }
}
