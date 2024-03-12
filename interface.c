/* File: interface.c
 * -----------------
 * This file implements the interface for the exchange
 */
#include "gl.h"
#include "malloc.h"
#include "strings.h"
#include "printf.h"

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

    const static int N_ROWS_REQ = 9, N_COLS_REQ = 40;
    for (int i = 0; i < min(N_NEWS_DISPLAY, news.n - news.top); i++) {
        int ind = (news.top + i) % N_NEWS_DISPLAY; 
        char buf[N_COLS_REQ + 1]; // + 1 for null-terminator
        snprintf(buf, N_COLS_REQ + 1, "%02d) %s", news.top + i, news.text[ind]); 
        int x_pix = gl_get_char_height() * (x + 1 + i), y_pix = gl_get_char_width() * y;
        gl_draw_string(x_pix, y_pix, buf, news.color);
    } 
}

static void draw_ticker(int x, int y) {
    // x, y are in character units (not pixels)
    
    const static int N_ROWS_REQ = 12, N_COLS_REQ = 12;
    for (int i = 0; i < min(N_TICKER_DISPLAY, ticker.n - ticker.top); i++) {
        int ind = (ticker.top + i) % N_TICKER_DISPLAY;
        char buf[N_COLS_REQ + 1]; // + 1 for null-terminator
        int close_price = ticker.stocks[ind].close_price[module.time];
        int open_price = ticker.stocks[ind].open_price[module.time];
        int pct_change = (close_price - open_price) * 100 / open_price;

        snprintf(buf, N_COLS_REQ + 1, " %s  %03d%% ", ticker.stocks[ind].symbol, pct_change); 
        int x_pix = gl_get_char_height() * (x + 1 + i), y_pix = gl_get_char_width() * y;
        gl_draw_string(x_pix, y_pix, buf, (pct_change >= 0 ? GL_GREEN : GL_RED));
    }
}

static void draw_all() {
    // draw_graph(3, 12);
    draw_ticker(3, 0);
    draw_news(16, 0);
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

    // settings
    module.bg_color = GL_BLACK;
    module.time = 5;
    module.nrows = nrows;
    module.ncols = ncols;
    module.line_height = gl_get_char_height() + LINE_SPACING;
    module.tick_10s = 0;

    // display
    gl_init(ncols * gl_get_char_width(), nrows * module.line_height, GL_DOUBLEBUFFER);
    draw_all();
    display(); 

    // interrupt settings
    interrupts_enable_source(INTERRUPT_SOURCE_HSTIMER0);
    interrupts_register_handler(INTERRUPT_SOURCE_HSTIMER0, handler_10s, NULL);
    // for 30s: track # calls mod 3 with global variable tick_10s

    interrupts_global_enable(); // everything fully initialized, now turn on interrupts
}

void main(void) {
    interface_init(30, 80);
}

