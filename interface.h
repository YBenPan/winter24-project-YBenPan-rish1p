#include "gl.h"
#include "malloc.h"
#include "strings.h"
#include "printf.h"
#include "interrupts.h"
#include "gpio.h"
#include "timer.h"
#include "uart.h"
#include "hstimer.h"
#include "mathlib.h"
#include "comm.h"
#include "shell.h"
#include "shell_commands.h"

int exchange_evaluate(const char *line);
