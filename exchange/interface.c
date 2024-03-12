/* File: interface.c
 * -----------------
 * This file implements the interface for the exchange
 */
#include "gl.h"
#include "malloc.h"
#include "strings.h"
#include "printf.h"

#define N_TIME 100
#define MAX_STOCKS 20
#define MAX_NEWS 20

static struct {
    color_t bg_color;
    int time;
    int nrows, ncols, line_height;
} module;

typedef struct stock {
    char *name;
    char *symbol;
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

static int max(int a, int b) {
    return a >= b ? a : b;
}

static int min(int a, int b) {
    return a <= b ? a : b;
}

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

static void data_init() {
    news_init();
}

static void draw_news(int x, int y) {
    // x, y are in character units (not pixels)
    // TODO: Fix case where not enough news / ticker

    const static int N_ROWS_REQ = 9, N_COLS_REQ = 40;
    const static int N_NEWS_DISPLAY = 5;
    for (int i = 0; i < N_NEWS_DISPLAY; i++) {
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
    int N_TICKER_DISPLAY = max(10, ticker.n);
    for (int i = 0; i < N_TICKER_DISPLAY; i++) {
        int ind = (ticker.top + i) % N_TICKER_DISPLAY;
        char buf[N_COLS_REQ + 1]; // + 1 for null-terminator
        int close_price = ticker.stocks[ind].close_price[module.time];
        int open_price = ticker.stocks[ind].open_price[module.time];
        int pct_change = (close_price - open_price) * 100 / open_price;

        snprintf(buf, N_COLS_REQ + 1, " %s  %03d%%", ticker.stocks[ind].symbol, pct_change); 
        int x_pix = gl_get_char_height() * (x + 1 + i), y_pix = gl_get_char_width() * y;
        gl_draw_string(x_pix, y_pix, buf, (pct_change >= 0 ? GL_GREEN : GL_RED));
    }
}

static void draw_all() {
    // draw_graph(3, 12);
    draw_ticker(3, 0);
    draw_news(16, 0);
}

void interface_init(int nrows, int ncols) {
    const static int LINE_SPACING = 4;

    data_init();

    module.bg_color = GL_BLACK;
    module.time = 5;
    module.nrows = nrows;
    module.ncols = ncols;
    module.line_height = gl_get_char_height() + LINE_SPACING;
    draw_all();

    gl_init(ncols * gl_get_char_width(), nrows * module.line_height, GL_DOUBLEBUFFER);
    gl_clear(module.bg_color);

    gl_swap_buffer();
}

void main(void) {
    interface_init(30, 80);
}

