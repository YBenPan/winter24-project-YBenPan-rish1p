/* File: interface.c
 * -----------------
 * This file implements the interface for the exchange
 */
#include "gl.h"
#include "malloc.h"
#include "strings.h"
#include "printf.h"
#include "interrupts.h"
#include "gpio.h"
#include "timer.h"
#include "uart.h"
#include "hstimer.h"

extern void memory_report();

#define N_TIME 100
#define N_NEWS_DISPLAY 5
#define N_TICKER_DISPLAY 10
#define MAX_STOCKS 20
#define MAX_NEWS 20

static struct {
    color_t bg_color;
    int time;
    int tick_10s; // tracks (# 10s-ticks) mod 3
    int nrows, ncols, line_height;
} module;

typedef struct stock {
    const char *name;
    const char *symbol;
    int open_price[N_TIME], close_price[N_TIME];
} stock_t;

static struct {
    int n, top;
    stock_t stocks[MAX_STOCKS];
} ticker;

static struct {
    int n, top;
    color_t color;
    const char *text[MAX_NEWS];
} news; 

// Helper functions
static int max(int a, int b) {
    return a >= b ? a : b;
}

static int min(int a, int b) {
    return a <= b ? a : b;
}

static int lprintf(char buf[], size_t max_size) {
    // Left-aligned printf; fills empty parts with ' '
    // TODO: Implement
    return 0;
}

static int rprintf(char buf[], size_t max_size) {
    // Right-aligned printf; fills empty parts with ' '
    // TODO: Implement
    return 0;
}

// Initialization functions

static void news_init() {
    // initialize news
    news.n = 10; // at most MAX_NEWS
    news.color = GL_AMBER;
    news.top = 0;
    news.text[0] = "Citadel LLC becomes the world's largest hedge fund.";
    news.text[1] = "Donald Trump is elected as the President of the United States.";
    news.text[2] = "Archduke Franz Ferdinand is assassinated in Sarajevo.";
    news.text[3] = "President Johnson will not run for a second term.";
    news.text[4] = "President Nixon decouples the U.S. Dollar from gold";
    news.text[5] = "Russia defaults on its foreign debt obligations";
    news.text[6] = "President Reagan announces the first tax cuts";
    news.text[7] = "Apple CEO Steve Jobs has passed away due to pancreatic cancer";
    news.text[8] = "Amazon founder Jeff Bezos steps down from CEO position";
    news.text[9] = "OpenAI CEO Sam Altman is fired";
}

static void stocks_init() {
    // TODO: Write python script to generate code that initializes stocks data
    ticker.n = 20;
    ticker.top = 0;
    ticker.stocks[0] = (stock_t){ .name = "Microsoft Corp", .symbol = "MSFT" };
    ticker.stocks[1] = (stock_t){ .name = "Apple Inc.", .symbol = "AAPL" };
    ticker.stocks[2] = (stock_t){ .name = "NVIDIA Corp", .symbol = "NVDA" };
    ticker.stocks[3] = (stock_t){ .name = "Amazon.com, Inc.", .symbol = "AMZN" };
    ticker.stocks[4] = (stock_t){ .name = "Alphabet Inc.", .symbol = "GOOG" };
    ticker.stocks[5] = (stock_t){ .name = "Meta Platforms, Inc.", .symbol = "META" };
    ticker.stocks[6] = (stock_t){ .name = "Berkshire Hathaway, Inc.", .symbol = "BRKB"};
}

static void data_init() {
    news_init();
    stocks_init();
}

// Core graphics functions
static void draw_news(int x, int y) {
    // x, y are in character units (not pixels)
    printf("Drawing news...\n");

    const static int N_ROWS_REQ = 9, N_COLS_REQ = 60;
    for (int i = 0; i < min(N_NEWS_DISPLAY, news.n - news.top); i++) {
        int ind = (news.top + i) % N_NEWS_DISPLAY; 
        char buf[N_COLS_REQ + 1]; // + 1 for null-terminator
        snprintf(buf, N_COLS_REQ + 1, "%02d) %s", news.top + i, news.text[ind]); 
        int x_pix = gl_get_char_width() * (x + 1), y_pix = module.line_height * (y + 1 + i);
        printf("News i = %d, x_pix = %d, y_pix = %d \n", i, x_pix, y_pix);
        gl_draw_string(x_pix, y_pix, buf, news.color);
    } 
}

static void draw_ticker(int x, int y) {
    // x, y are in character units (not pixels)
    printf("Drawing ticker...\n");
    
    const static int N_ROWS_REQ = 12, N_COLS_REQ = 12;
    for (int i = 0; i < min(N_TICKER_DISPLAY, ticker.n - ticker.top); i++) {
        printf("Ticker i = %d\n", i);
        int ind = (ticker.top + i) % N_TICKER_DISPLAY;
        char buf[N_COLS_REQ + 1]; // + 1 for null-terminator
        int close_price = ticker.stocks[ind].close_price[module.time];
        int open_price = ticker.stocks[ind].open_price[module.time];
        // TODO: change to pct_change after obtaining data + floating point
        // int pct_change = (close_price - open_price) * 100 / open_price;
        int pct_change = close_price - open_price;

        snprintf(buf, N_COLS_REQ + 1, " %s  %03d%% ", ticker.stocks[ind].symbol, pct_change); 
        int x_pix = gl_get_char_width() * (x + 1), y_pix = module.line_height * (y + 1 + i);
        printf("Ticker i = %d, x_pix = %d, y_pix = %d\n", i, x_pix, y_pix);
        gl_draw_string(x_pix, y_pix, buf, (pct_change >= 0 ? GL_GREEN : GL_RED));
    }
}

static void draw_graph(int x, int y, int stock_ind) {
    // `stock_ind` = index of stock in ticker.stocks[]
    const static int N_ROWS_REQ = 14, N_COLS_REQ = 28;
    // TODO: Check size fits for all components
    const static int N_TIME_DISPLAY = 10;
    const static int N_PRICE_INTERVALS = 12; // for room 
    int start_time = max(0, module.time - N_TIME_DISPLAY + 1), end_time = module.time;
    int max_interval_price = 0, min_interval_price = 100000;
    for (int i = start_time; i <= end_time; i++) {
        int open_price = ticker.stocks[stock_ind].open_price[i];
        int close_price = ticker.stocks[stock_ind].close_price[i];
        max_interval_price = max(max_interval_price, max(open_price, close_price));
        min_interval_price = min(min_interval_price, min(open_price, close_price));
    }
    // calculate step size
    int step_size, diff = max_interval_price - min_interval_price;
    if (diff <= 10) step_size = 1;
    else if (10 < diff && diff <= 20) step_size = 2;
    else if (20 < diff && diff <= 40) step_size = 4;
    else if (40 < diff && diff <= 50) step_size = 5;
    else if (50 < diff && diff <= 100) step_size = 10;
    else if (100 < diff && diff <= 200) step_size = 20;
    else {
        // TODO: Add resize mechanism for volatile stocks
        printf("WARNING: stock too volatile (max_interval_price - min_interval_price) = %d > 200\n", diff);
        printf("         displaying only portions of the stock graph\n");
        step_size = 20;
    }

    // draw title
    char buf[N_COLS_REQ + 1];
    snprintf(buf, N_COLS_REQ + 1, "%s (%s)\n", ticker.stocks[stock_ind].name, ticker.stocks[stock_ind].symbol);
    gl_draw_string(gl_get_char_width() * x, module.line_height * y, buf, GL_AMBER);

    // draw axes
    // TODO: Implement gl_draw_vertical_line()
    int graph_min = min_interval_price / step_size * step_size;
    int graph_max = graph_min + N_PRICE_INTERVALS * step_size;
    for (int i = 0; i < N_PRICE_INTERVALS; i++) {
        char buf[N_COLS_REQ + 1];
        snprintf(buf, N_COLS_REQ + 1, " %03d-|", graph_max - i * step_size);    
        int x_pix = gl_get_char_width() * x, y_pix = module.line_height * (y + 1 + i);
        gl_draw_string(x_pix, y_pix, buf, GL_WHITE);
    }
    char buf2[] = "--------------------"; 
    int x_pix = gl_get_char_width() * x, y_pix = module.line_height * (y + 1 + N_PRICE_INTERVALS);
    gl_draw_string(x_pix, y_pix, buf2, GL_WHITE);
    
    // draw box plot
    for (int i = 0; i <= end_time - start_time; i++) {
        int open_price = ticker.stocks[stock_ind].open_price[i + start_time];
        int close_price = ticker.stocks[stock_ind].close_price[i + start_time];
        int max_price = max(open_price, close_price), min_price = min(open_price, close_price);
        int bx = (x + 6 + i) * gl_get_char_width() + 4;
        int by = (y + 1) * module.line_height + (graph_max - max_price) * 20 / step_size;
        int w = 2 * (gl_get_char_width() - 4);
        int h = (max_price - min_price) * 20 / step_size;
        color_t color = (open_price >= close_price ? GL_GREEN : GL_RED);
        gl_draw_rect(bx, by, w, h, color);
    }
}

static void draw_all() {
    draw_graph(12, 0, 0);
    draw_ticker(0, 3);
    draw_news(0, 16);
}

static void display() {
    gl_swap_buffer();
}

// Interrupt handlers
static void handler_10s(uintptr_t pc, void *aux_data) {
    module.tick_10s = (module.tick_10s + 1) % 3;
    if (module.tick_10s == 0) {
        module.time++; // increases every 30 seconds, or 3 10-s ticks
    }
    ticker.top += N_TICKER_DISPLAY;
    if (ticker.top > ticker.n) {
        ticker.top = 0;
    }
    news.top += N_NEWS_DISPLAY;
    if (news.top > news.n) {
        news.top = 0;
    }
    draw_all();
    display();
    hstimer_interrupt_clear(HSTIMER0);
}

void interface_init(int nrows, int ncols) {
    const static int LINE_SPACING = 4;

    data_init();
    interrupts_init();
    gpio_init();
    timer_init();
    uart_init();
    printf("Now running interface!\n");

    // settings
    module.bg_color = GL_BLACK;
    module.time = 5;
    module.nrows = nrows;
    module.ncols = ncols;
    module.line_height = gl_get_char_height() + LINE_SPACING;
    module.tick_10s = 0;

    // display
    gl_init(ncols * gl_get_char_width(), nrows * module.line_height, GL_DOUBLEBUFFER);
    gl_clear(module.bg_color);
    draw_all();
    display();

    // interrupt settings
    hstimer_init(HSTIMER0, 10000000);
    hstimer_enable(HSTIMER0);
    interrupts_enable_source(INTERRUPT_SOURCE_HSTIMER0);
    interrupts_register_handler(INTERRUPT_SOURCE_HSTIMER0, handler_10s, NULL);
    // for 30s: track # calls mod 3 with global variable tick_10s

    interrupts_global_enable(); // everything fully initialized, now turn on interrupts
}

void main(void) {
    interface_init(30, 80);
    while (1) {
    };
    memory_report();
}

