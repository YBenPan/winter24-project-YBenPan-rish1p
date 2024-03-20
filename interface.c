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
#include "mathlib.h"
#include "comm.h"
#include "shell.h"
#include "shell_commands.h"

extern void memory_report();

#define N_TIME 100
#define N_NEWS_DISPLAY 5
#define N_TICKER_DISPLAY 10
#define MAX_STOCKS 20
#define MAX_NEWS 20

static struct {
    color_t bg_color;
    int time;
    int tick; // tracks # ticks mod (time of one refresh)
    int nrows, ncols, line_height;
    int stock_ind; // index of stock on display
} module;

typedef struct stock {
    const char *name;
    const char *symbol;
    float open_price[N_TIME], close_price[N_TIME], high_price[N_TIME], low_price[N_TIME];
} stock_t;

typedef struct date {
    const char *str[N_TIME];
} date_t;

static struct {
    int n, top;
    stock_t stocks[MAX_STOCKS];
} ticker;

static struct {
    int n, top;
    color_t color;
    const char *text[N_TIME][MAX_NEWS];
} news; 

static struct {
    float init_cap, cash;
    int shares[MAX_STOCKS];
} inventory;

static date_t dates;

// Helper functions
static int max(int a, int b) {
    return a >= b ? a : b;
}

static int min(int a, int b) {
    return a <= b ? a : b;
}

static float fmax(float a, float b) {
    return a >= b ? a : b;
}

static float fmin(float a, float b) {
    return a <= b ? a : b;
}

static int lprintf(char dst[], char src[], size_t max_size) {
    // Left-aligned printf; writes `max_size`-length string into `dst;
    // fills with src first and then spaces
    size_t n = strlen(src);
    int n_spaces = max(0, (int)max_size - (int)n);
    memcpy(dst, src, min(max_size, n));
    for (int i = 0; i < n_spaces; i++) {
        dst[n + i] = ' ';
    }
    dst[max_size] = '\0'; 
    return max_size;
}

static int rprintf(char dst[], char src[], size_t max_size) {
    // Right-aligned printf; writes `max_size`-length string into `dst;
    // fills with spaces first and then src
    size_t n = strlen(src);
    int n_spaces = max(0, (int)max_size - (int)n);
    for (int i = 0; i < n_spaces; i++) {
        dst[i] = ' ';
    }
    memcpy(dst + n_spaces, src, min(max_size, n));
    dst[max_size] = '\0';
    return max_size;
}

// Initialization functions prototypes
static void news_init(void);
static void stocks_init(void);
static void dates_init(void);
static void data_init(void);

static void draw_all();

// Commands settings and functions
int cmd_buy(int argc, const char *argv[]) {
    if (argc != 3) {
        comm_putstring("error: buy expects 2 arguments [symbol] [shares]\n");
        return -1;
    }
    char buf[100];
    for (int i = 0; i < ticker.n; i++) {
        if (strcmp(argv[1], ticker.stocks[i].symbol) == 0) {
            int nshares = strtonum(argv[2], NULL);  
            float cost = nshares * ticker.stocks[i].close_price[module.time];
            if (inventory.cash < cost) {
                snprintf(buf, sizeof(buf), "Not enough cash! Need $%.2f to purchase %d shares of %s\n", cost, nshares, argv[1]);
                comm_putstring(buf);
                return -1;
            }
            inventory.shares[i] += nshares;
            inventory.cash -= cost;
            snprintf(buf, sizeof(buf), "Successfully bought %d shares; currently own %d shares of [%s] \n", nshares, inventory.shares[i], argv[1]);
            comm_putstring(buf);
            return 0;
        }
    }
    snprintf(buf, sizeof(buf), "[%s] not a traded stock; Try again!\n", argv[1]);
    comm_putstring(buf);
    return -1;
}

int cmd_sell(int argc, const char *argv[]) {
    if (argc != 3) {
        comm_putstring("error: sell expects 2 arguments [symbol] [shares]\n");
        return -1;
    }
    char buf[100];
    for (int i = 0; i < ticker.n; i++) {
        if (strcmp(argv[1], ticker.stocks[i].symbol) == 0) {
            int nshares = strtonum(argv[2], NULL);
            if (inventory.shares[i] < nshares) {
                snprintf(buf, sizeof(buf), "Only have %d shares in the inventory; Try again!\n", inventory.shares[i]);
                comm_putstring(buf);
                return -1;
            }
            inventory.shares[i] -= nshares;
            inventory.cash += nshares * ticker.stocks[i].close_price[module.time];
            snprintf(buf, sizeof(buf), "Successfully sold %d shares; currently own %d shares of [%s]\n", nshares, inventory.shares[i], argv[1]);
            comm_putstring(buf);
            return 0;
        }    
    }
    snprintf(buf, sizeof(buf), "[%s] not a traded stock; Try again!\n", argv[1]);
    comm_putstring(buf);
    return -1;
}

int cmd_price(int argc, const char *argv[]) {
    if (argc != 2) {
        comm_putstring("error: price expects 1 argument [symbol]\n");
        return -1;
    }
    char buf[100];
    for (int i = 0; i < ticker.n; i++) {
        if (strcmp(argv[1], ticker.stocks[i].symbol) == 0) {
            snprintf(buf, sizeof(buf), "Price of [%s]: $%.2f\n", argv[1], ticker.stocks[i].close_price[module.time]);
            comm_putstring(buf);
            return 0;
        }
    }
    snprintf(buf, sizeof(buf), "[%s] not a traded stock; Try again!\n", argv[1]);
    comm_putstring(buf);
    return -1;
}

int cmd_graph(int argc, const char *argv[]) {
    if (argc != 2) {
        comm_putstring("error: graph expects 1 argument [symbol]\n");
        return -1;
    }
    char buf[100];
    for (int i = 0; i < ticker.n; i++) {
        if (strcmp(argv[1], ticker.stocks[i].symbol) == 0) {
            module.stock_ind = i;
            draw_all();
            gl_swap_buffer();
            snprintf(buf, sizeof(buf), "Now displaying [%s]\n", argv[1]);
            comm_putstring(buf);
            return 0;
        }
    }
    snprintf(buf, sizeof(buf), "[%s] not a traded stock; Try again!\n", argv[1]);
    comm_putstring(buf);
    return -1;
}

static float get_total_val(void) {
    float s = 0;
    for (int i = 0; i < ticker.n; i++) {
        s += ticker.stocks[i].close_price[module.time] * inventory.shares[i];
    }
    return s;
}

int cmd_info(int argc, const char *argv[]) {
    comm_putstring(" STOCK | SHARES | PRICE \n");
    comm_putstring("------------------------\n");
    char buf[100], buf1[100];
    for (int i = 0; i < ticker.n; i++) {
        if (inventory.shares[i] > 0) {
            snprintf(buf, sizeof(buf), "%s", ticker.stocks[i].symbol); 
            lprintf(buf1, buf, 7);
            comm_putstring(buf1);
            comm_putstring("|");
            snprintf(buf, sizeof(buf), "%d", inventory.shares[i]);
            lprintf(buf1, buf, 8);
            comm_putstring(buf1);
            comm_putstring("|");
            snprintf(buf, sizeof(buf), "%.2f\n", ticker.stocks[i].close_price[module.time]);
            lprintf(buf1, buf, 8);
            comm_putstring(buf1);
        }
    }
    return 0;
}

int cmd_pnl(int argc, const char *argv[]) {
    char buf[100], buf1[100];
    float cur_cap = get_total_val();

    comm_putstring("Initial Capital: ");
    snprintf(buf, sizeof(buf), "%.2f\n", inventory.init_cap);
    rprintf(buf1, buf, 12);
    comm_putstring(buf1);

    comm_putstring("Current Capital: ");
    snprintf(buf, sizeof(buf), "%.2f\n", inventory.cash + cur_cap);
    rprintf(buf1, buf, 12);
    comm_putstring(buf1);

    comm_putstring("Cash           : ");
    snprintf(buf, sizeof(buf), "%.2f\n", inventory.cash);
    rprintf(buf1, buf, 12);
    comm_putstring(buf1);

    comm_putstring("Stock          : ");
    snprintf(buf, sizeof(buf), "%.2f\n", cur_cap);
    rprintf(buf1, buf, 12);
    comm_putstring(buf1);

    float pct_change = (cur_cap + inventory.cash - inventory.init_cap) / inventory.init_cap * 100;
    comm_putstring("Profit / Loss:   ");
    snprintf(buf, sizeof(buf), "%.1f%%\n", pct_change);
    rprintf(buf1, buf, 12);
    comm_putstring(buf1);
    return 0;
}

int cmd_bankruptcy(int argc, const char *argv[]) {
    memset(inventory.shares, 0, sizeof(inventory.shares));
    inventory.init_cap = 10000;
    inventory.cash = 10000;
    comm_putstring("Bankruptcy Successful! Starting with $10,000 again.");
    return 0;
}

static const command_t commands[] = { 
    {"buy",  "buy <symbol> <shares>",  "buys shares of a stock with a given ticker symbol", cmd_buy},
    {"sell",  "sell <symbol> <shares>",  "sells a stock with a given ticker symbol", cmd_sell},
    {"price",  "price <symbol>",  "return price a stock with a given ticker symbol", cmd_price},
    {"graph",  "graph <symbol>",  "graphs a price a stock with a given ticker symbol", cmd_graph},
    {"info",  "info",  "returns a table of owned stocks and their information", cmd_info},
    {"pnl",  "pnl",  "returns how much money you have (stonks!)", cmd_pnl},
    {"bankruptcy",  "bankruptcy [please]", "declares bankruptcy! we just print more money and get rid of your debt", cmd_bankruptcy},
};

// Helper functions for `shell_evaluate`
static char *strndup(const char *src, size_t n) {
    //CITATION: This function is from lab 4 exercise 2
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

static bool isspace(char ch) {
    //CITATION: This function is from lab 4 exercise 2
    return ch == ' ' || ch == '\t' || ch == '\n';
}

static int tokenize(const char *line, char *array[],  int max) {
    //CITATION: This function is from lab 4 exercise 2
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

int exchange_evaluate(const char *line) {
    // This function evaluates the input against a 
    // list of commands in the command array
    // Recycled from `shell.c` to process user input from console

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
    char buf[100];
    snprintf(buf, sizeof(buf), "error: no such command '%s'.\n", tokens[0]);
    comm_putstring(buf);

    // Free the allocated tokens
    for (int j = 0; j < num_tokens; ++j) {
        free(tokens[j]);
    }

    return -1;
}


// Core graphics functions
static void draw_date(int x, int y) {
    const static int N_ROWS_REQ = 1, N_COLS_REQ = 8;
    char buf[N_COLS_REQ + 1];
    snprintf(buf, N_COLS_REQ + 1, "%s", dates.str[module.time]);
    gl_draw_string(gl_get_char_width() * x, module.line_height * y, buf, GL_AMBER);
}

static void draw_news(int x, int y) {
    // x, y are in character units (not pixels)
    const static int N_ROWS_REQ = 9, N_COLS_REQ = 80;
    if (x + N_COLS_REQ > module.ncols || y + N_ROWS_REQ > module.nrows) {
        printf("Error: News out of bounds\n");
        return;
    }
    for (int i = 0; i < min(N_NEWS_DISPLAY, news.n - news.top); i++) {
        int ind = news.top + i;
        char buf[N_COLS_REQ + 1]; // + 1 for null-terminator
        snprintf(buf, N_COLS_REQ + 1, "%02d) %s", ind + 1, news.text[module.time][ind]); 
        int x_pix = gl_get_char_width() * (x + 1), y_pix = module.line_height * (y + 1 + i);
        gl_draw_string(x_pix, y_pix, buf, news.color);
    } 
}

static void draw_ticker(int x, int y) {
    // x, y are in character units (not pixels)
    const static int N_ROWS_REQ = 12, N_COLS_REQ = 14;
    if (x + N_COLS_REQ >= module.ncols || y + N_ROWS_REQ >= module.nrows) {
        printf("Error: Ticker out of bounds\n");
        return;
    }
    
    for (int i = 0; i < min(N_TICKER_DISPLAY, ticker.n - ticker.top); i++) {
        int ind = ticker.top + i;
        char buf[N_COLS_REQ + 1]; // + 1 for null-terminator
        int close_price = ticker.stocks[ind].close_price[module.time];
        int open_price = ticker.stocks[ind].open_price[module.time];
        int pct_change = (close_price - open_price) * 100 / open_price;

        // Print symbol and pct change separately in two strings
        snprintf(buf, N_COLS_REQ + 1, "%s", ticker.stocks[ind].symbol); 
        char buf1[N_COLS_REQ + 1];
        lprintf(buf1, buf, 6);
        int x_pix = gl_get_char_width() * (x + 1), y_pix = module.line_height * (y + 1 + i);
        gl_draw_string(x_pix, y_pix, buf1, GL_AMBER);

        buf[0] = '\0';
        buf1[0] = '\0';
        snprintf(buf, N_COLS_REQ + 1, "%d%%", pct_change);
        rprintf(buf1, buf, 6);
        x_pix = gl_get_char_width() * (x + 5);
        y_pix = module.line_height * (y + 1 + i); 
        gl_draw_string(x_pix, y_pix, buf1, (pct_change >= 0 ? GL_GREEN : GL_RED));
    }
}

static void draw_graph(int x, int y, int stock_ind) {
    // `stock_ind` = index of stock in ticker.stocks[]
    const static int N_ROWS_REQ = 18, N_COLS_REQ = 50;
    if (x + N_COLS_REQ >= module.ncols || y + N_ROWS_REQ >= module.nrows) {
        printf("Error: graph out of bounds\n");
        return;
    }

    const static int N_TIME_DISPLAY = 20;
    const static int N_PRICE_INTERVALS = 12; // for room 
    int start_time = max(0, module.time - N_TIME_DISPLAY + 1), end_time = module.time;
    float max_interval_price = 0, min_interval_price = 100000;
    for (int i = start_time; i <= end_time; i++) {
        float open_price = ticker.stocks[stock_ind].open_price[i];
        float close_price = ticker.stocks[stock_ind].close_price[i];
        float high_price = ticker.stocks[stock_ind].high_price[i];
        float low_price = ticker.stocks[stock_ind].low_price[i];
        max_interval_price = fmax(max_interval_price, fmax(open_price, close_price));
        max_interval_price = fmax(max_interval_price, fmax(high_price, low_price));
        min_interval_price = fmin(min_interval_price, fmin(open_price, close_price));
        min_interval_price = fmin(min_interval_price, fmin(high_price, low_price));
    }

    // calculate step size
    float step_size, diff = max_interval_price - min_interval_price; 
    if (1 <= diff && diff <= 10) step_size = ceil(diff) / 10;
    else if (10 < diff && diff <= 100) step_size = ceil(diff / 10);
    else if (100 < diff && diff <= 1000) step_size = ceil(diff / 100) * 10;
    else {
        if (diff < 1) { // two or more digits after decimal point not supported
            printf("WARNING: stock too static (max_interval_price - min_interval_price = %f < 1)\n", diff);
        }
        if (diff > 80) {
            printf("WARNING: stock too volatile (max_interval_price - min_interval_price = %f > 200)\n", diff);
        }
        return; 
    }
    printf("STEP_SIZE: %.2f\n", step_size);

    // draw title
    char buf[N_COLS_REQ + 1], buf1[N_COLS_REQ + 1];
    snprintf(buf, N_COLS_REQ + 1, "%s (%s)\n", ticker.stocks[stock_ind].name, ticker.stocks[stock_ind].symbol);
    gl_draw_string(gl_get_char_width() * x, module.line_height * y, buf, GL_AMBER);

    const int left_space = 9;
    // draw axes
    float graph_min = (int)(min_interval_price / step_size) * step_size;
    float graph_max = graph_min + N_PRICE_INTERVALS * step_size;
    for (int i = 0; i < N_PRICE_INTERVALS; i++) {
        buf[0] = '\0'; buf1[0] = '\0';
        snprintf(buf, N_COLS_REQ + 1, "%.1f ", graph_max - i * step_size);
        rprintf(buf1, buf, left_space);
        int x_pix = gl_get_char_width() * x, y_pix = module.line_height * (y + 2 + i) - 8;
        gl_draw_string(x_pix, y_pix, buf1, GL_WHITE);
        x_pix += gl_get_char_width() * (left_space - 1);
        y_pix += 8;
        gl_draw_line(x_pix + 2, y_pix, x_pix + gl_get_char_width(), y_pix, GL_WHITE);
    }
    int x_pix = gl_get_char_width() * (x + left_space), y_pix = module.line_height * (y + 2);
    gl_draw_line(x_pix, y_pix, x_pix, y_pix + module.line_height * (y + 1 + N_PRICE_INTERVALS - 1) - 1, GL_WHITE);
    y_pix = module.line_height * (y + 2 + N_PRICE_INTERVALS);
    gl_draw_line(x_pix, y_pix, x_pix + (N_TIME_DISPLAY * 2 + 1) * gl_get_char_width(), y_pix, GL_WHITE);
    
    // draw box plot
    printf("OPEN_PRICE: %.2f\n", ticker.stocks[stock_ind].open_price[end_time]);
    printf("CLOSE_PRICE: %.2f\n", ticker.stocks[stock_ind].close_price[end_time]);
    printf("HIGH_PRICE: %.2f\n", ticker.stocks[stock_ind].high_price[end_time]); 
    printf("LOW_PRICE: %.2f\n", ticker.stocks[stock_ind].low_price[end_time]);

    for (int i = 0; i <= end_time - start_time; i++) {
        float open_price = ticker.stocks[stock_ind].open_price[i + start_time];
        float close_price = ticker.stocks[stock_ind].close_price[i + start_time];
        float high_price = ticker.stocks[stock_ind].high_price[i + start_time];
        float low_price = ticker.stocks[stock_ind].low_price[i + start_time];
        float max_price = fmax(open_price, close_price), min_price = fmin(open_price, close_price);
        int bx = (x + left_space + 2 * i) * gl_get_char_width() + 4;
        int by = (y + 2) * module.line_height + (graph_max - max_price) * 20 / step_size;
        int w = 2 * (gl_get_char_width() - 4);
        int h = (max_price - min_price) * 20 / step_size;
        int ly1 = (y + 2) * module.line_height + (graph_max - high_price) * 20 / step_size;
        int ly2 = (y + 2) * module.line_height + (graph_max - low_price) * 20 / step_size;
        int lx = (x + left_space + 2 * i) * gl_get_char_width() + 13;
        color_t color = (open_price >= close_price ? GL_RED : GL_GREEN);
        gl_draw_rect(bx, by, w, h, color);
        gl_draw_line(lx, ly1, lx, ly2, color);
        gl_draw_line(lx + 1, ly1, lx + 1, ly2, color);
        gl_draw_line(lx + 2, ly1, lx + 2, ly2, color);
    }
}

static void draw_all() {
    gl_clear(module.bg_color);
    draw_date(0, 0);
    draw_graph(14, 0, module.stock_ind);
    draw_ticker(0, 3);
    draw_news(0, 14);
}

// Interrupt handlers
static void hstimer0_handler(uintptr_t pc, void *aux_data) {
    module.tick = (module.tick + 1) % 2;
    if (module.tick == 0) {
        module.time++; // increases every 30 seconds, or 3 10-s ticks
        if (module.time == 100) {
            hstimer_interrupt_clear(HSTIMER0);
            hstimer_disable(HSTIMER0);
            interrupts_register_handler(INTERRUPT_SOURCE_HSTIMER0, NULL, NULL);
            interrupts_disable_source(INTERRUPT_SOURCE_HSTIMER0);
            const char *argv[1] = {"pnl"};
            cmd_pnl(1, argv);
            memory_report();
            return;
        }
        ticker.top = 0;
        news.top = 0;
    }
    else {
        ticker.top += N_TICKER_DISPLAY;
        if (ticker.top >= ticker.n) {
            ticker.top = 0;
        }
        news.top += N_NEWS_DISPLAY;
        if (news.top >= news.n) {
            news.top = 0;
        }
    }
    draw_all();
    gl_swap_buffer();
    hstimer_interrupt_clear(HSTIMER0);
}

void interface_init(int nrows, int ncols) {
    const static int LINE_SPACING = 4;

    data_init();
    interrupts_init();
    gpio_init();
    timer_init();
    comm_init();

    // settings
    module.bg_color = GL_BLACK;
    module.time = 5;
    module.nrows = nrows;
    module.ncols = ncols;
    module.line_height = gl_get_char_height() + LINE_SPACING;
    module.tick = 0;
    module.stock_ind = 2; // NVDA

    inventory.init_cap = 10000;
    inventory.cash = 10000;

    // display
    gl_init(ncols * gl_get_char_width(), nrows * module.line_height, GL_DOUBLEBUFFER);
    draw_all();
    gl_swap_buffer();

    // interrupt settings
    hstimer_init(HSTIMER0, 5000000);
    hstimer_enable(HSTIMER0);
    interrupts_enable_source(INTERRUPT_SOURCE_HSTIMER0);
    interrupts_register_handler(INTERRUPT_SOURCE_HSTIMER0, hstimer0_handler, NULL);
    setup_uart_interrupts();

    interrupts_global_enable(); // everything fully initialized, now turn on interrupts
}

void main(void) {
    interface_init(30, 80);
    while (1) {
    }
}

static void news_init(void) {
    // initialize news
    // Data obtained via Claude AI
    news.n = 10; // at most MAX_NEWS
    news.color = GL_AMBER;
    news.top = 0;

    // Dec 2015
    news.text[0][0] = "Federal Reserve raises interest rates for first time in nearly a decade";
    news.text[0][1] = "Oil prices fall below $35 per barrel amid global supply glut";
    news.text[0][2] = "Yahoo scraps plan to spin off Alibaba stake after tax concerns";
    news.text[0][3] = "Toshiba to cut 7,000 jobs after $1.3 billion accounting scandal";
    news.text[0][4] = "McDonald's moves tax base to UK in $3.2 billion restructuring";
    news.text[0][5] = "Puerto Rico defaults on portion of $73 billion debt";
    news.text[0][6] = "Chipotle shares plummet as E. coli outbreak expands across US";
    news.text[0][7] = "DuPont and Dow Chemical agree to $130 billion mega-merger";
    news.text[0][8] = "Valeant discloses criminal probe over pricing practices";
    news.text[0][9] = "Apple shares tumble as iPhone sales growth slows";

    // Jan 2016
    news.text[1][0] = "China's stock market turmoil rattles global markets";
    news.text[1][1] = "Oil prices plunge below $30 per barrel amid supply glut";
    news.text[1][2] = "US imposes new sanctions on Iran after missile test";
    news.text[1][3] = "Volkswagen emissions scandal costs continue to mount";
    news.text[1][4] = "Yahoo explores sale of core internet business";
    news.text[1][5] = "US job growth slows in December as wages rise";
    news.text[1][6] = "Dow drops over 400 points as China slowdown fears persist";
    news.text[1][7] = "Netflix shares tumble on weak subscriber growth forecast";
    news.text[1][8] = "Amazon reports record holiday sales but misses expectations";
    news.text[1][9] = "Apple's quarterly revenue declines for first time since 2003";

    // Feb 2016
    news.text[2][0] = "Stock market rout continues as oil prices slide further";
    news.text[2][1] = "US Fed keeps interest rates unchanged amid market turmoil";
    news.text[2][2] = "Apple's fight with FBI over iPhone encryption escalates";
    news.text[2][3] = "Tesco faces record $6.5 billion overstatement fine";
    news.text[2][4] = "Twitter shares plunge as user growth stalls";
    news.text[2][5] = "Alphabet (Google) overtakes Apple as world's most valuable company";
    news.text[2][6] = "Takata agrees to record $200 million fine over faulty airbags";
    news.text[2][7] = "Pfizer and Allergan scrap $160 billion merger deal";
    news.text[2][8] = "US job growth rebounds in January after weak December";
    news.text[2][9] = "Amazon shares soar as AWS cloud business shines";

    // Mar 2016
    news.text[3][0] = "ECB launches fresh stimulus measures to boost eurozone economy";
    news.text[3][1] = "Oil prices recover but remain volatile amid oversupply";
    news.text[3][2] = "Yahoo set to cut workforce by 15% amid turnaround struggles";
    news.text[3][3] = "FBI unlocks San Bernardino shooter's iPhone without Apple's help";
    news.text[3][4] = "Valeant faces criminal probe over pharmaceutical pricing";
    news.text[3][5] = "US Fed leaves interest rates unchanged, signals caution";
    news.text[3][6] = "Marriott completes $13 billion acquisition of Starwood Hotels";
    news.text[3][7] = "Apple unveils smaller iPhone SE and new iPad Pro models";
    news.text[3][8] = "Tesla aims to become a mass-market automaker with Model 3 launch";
    news.text[3][9] = "Lufthansa strikes ground hundreds of flights across Europe";

    // Apr 2016
    news.text[4][0] = "Saudi Arabia unveils radical plan to reduce oil dependency";
    news.text[4][1] = "Volkswagen reaches $4.3 billion US criminal settlement over emissions";
    news.text[4][2] = "Microsoft sues US government over data privacy violations";
    news.text[4][3] = "OPEC and non-OPEC producers fail to reach output freeze deal";
    news.text[4][4] = "Amazon shares soar after blowing past profit expectations";
    news.text[4][5] = "Twitter shares plummet as revenue growth stalls";
    news.text[4][6] = "Puerto Rico defaults on $422 million debt payment";
    news.text[4][7] = "Chernobyl nuclear disaster costs still emerging after 30 years";
    news.text[4][8] = "Tesla aims for volume production of Model 3 by late 2017";
    news.text[4][9] = "Mitsubishi admits to falsifying fuel economy data for 25 years";

    // May 2016
    news.text[5][0] = "Oil prices surge as Goldman Sachs forecasts supply deficit";
    news.text[5][1] = "Microsoft agrees to buy LinkedIn for $26.2 billion";
    news.text[5][2] = "Verizon strikes $4.8 billion deal to acquire Yahoo's core business";
    news.text[5][3] = "Bayer offers $62 billion to acquire Monsanto in agrochemical mega-merger";
    news.text[5][4] = "Britain's pound hits lowest level since 1985 as Brexit fears mount";
    news.text[5][5] = "Gawker files for bankruptcy after losing Hulk Hogan privacy case";
    news.text[5][6] = "Regulators hit five global banks with $5.7 billion in fines";
    news.text[5][7] = "Alibaba reports booming revenue growth, but profit disappoints";
    news.text[5][8] = "Tesla aims to make history with release of affordable Model 3";
    news.text[5][9] = "Puerto Rico missses debt payment, deepening financial crisis";
    
    // Jun 2016
    news.text[6][0] = "Brexit vote shocks global markets, British pound plunges";
    news.text[6][1] = "Volkswagen to pay $15 billion settlement in emissions scandal";
    news.text[6][2] = "Walmart to acquire online retailer Jet.com for $3 billion";
    news.text[6][3] = "Tesla and SolarCity agree to $2.6 billion merger";
    news.text[6][4] = "Microsoft to buy LinkedIn for $26.2 billion in largest deal";
    news.text[6][5] = "Airbnb raises $1 billion in funding, valued at $30 billion";
    news.text[6][6] = "Verizon to acquire Yahoo's core business for $4.8 billion";
    news.text[6][7] = "Uber raises $3.5 billion from Saudi investment fund";
    news.text[6][8] = "Amazon to open more bookstores after opening first location";
    news.text[6][9] = "Takata recalls 35 million more airbags in largest auto recall";

    // Jul 2016
    news.text[7][0] = "Brexit turmoil deepens as PM David Cameron resigns";
    news.text[7][1] = "Verizon agrees to buy Yahoo's core business for $4.83 billion";
    news.text[7][2] = "Tesla unveils 'master plan' for self-driving cars and trucks";
    news.text[7][3] = "Anheuser-Busch InBev raises offer for SABMiller to $104 billion";
    news.text[7][4] = "Microsoft's cloud business boosts revenue amid PC slump";
    news.text[7][5] = "China's economic growth slows to 6.7%, weakest since 2009";
    news.text[7][6] = "Airbnb faces growing backlash from regulators over rentals";
    news.text[7][7] = "Volkswagen agrees to $14.7 billion settlement in emissions scandal";
    news.text[7][8] = "Amazon unveils Prime Air, a future drone delivery service";
    news.text[7][9] = "Uber loses $1.27 billion in first half of 2016 amid expansion";

    // Aug 2016
    news.text[8][0] = "Apple issues $7 billion bond to fund share buybacks, dividends";
    news.text[8][1] = "Pfizer to buy cancer drug maker Medivation for $14 billion";
    news.text[8][2] = "Uber loses $1.27 billion in first half amid expansion";
    news.text[8][3] = "Mylan's EpiPen pricing raises allegations of price gouging";
    news.text[8][4] = "Walmart to acquire e-commerce startup Jet.com for $3 billion";
    news.text[8][5] = "Oil prices rebound as Saudi Arabia and Russia agree output freeze";
    news.text[8][6] = "Apple hit with $14.5 billion tax bill from European Commission";
    news.text[8][7] = "Mondelez calls off $23 billion bid for Hershey after rejection";
    news.text[8][8] = "WhatsApp to start sharing user data with Facebook after policy change";
    news.text[8][9] = "EpiPen maker Mylan to launch generic version amid pricing backlash";    

    // Sep 2016
    news.text[9][0] = "Wells Fargo fined $185 million for widespread illegal practices";
    news.text[9][1] = "Pfizer decides against splitting into two companies";
    news.text[9][2] = "Uber starts self-driving car pickups in Pittsburgh";
    news.text[9][3] = "Yahoo confirms data breach affecting 500 million accounts";
    news.text[9][4] = "Samsung recalls Galaxy Note 7 after battery explosion reports";
    news.text[9][5] = "Bayer acquires Monsanto for $66 billion in record agriculture deal";
    news.text[9][6] = "OPEC agrees to first oil output cut in 8 years to boost prices";
    news.text[9][7] = "Deutsche Bank shares plunge amid $14 billion U.S. settlement demand";
    news.text[9][8] = "Snapchat unveils video-recording Spectacles, rebrands as Snap Inc.";
    news.text[9][9] = "Alphabet's Google launches smartphone, virtual reality headset";

    // Oct 2016
    news.text[10][0] = "AT&T agrees to buy Time Warner for $85.4 billion";
    news.text[10][1] = "Tesla unveils solar roof tiles, Powerwall 2 energy storage";
    news.text[10][2] = "Samsung halts Galaxy Note 7 production amid fire concerns";
    news.text[10][3] = "Microsoft shares hit all-time high on cloud growth";
    news.text[10][4] = "Rockwell Automation rejects $27.5 billion takeover bid from Emerson";
    news.text[10][5] = "Ethereum blockchain tech firm raises $200 million in record funding";
    news.text[10][6] = "Third cyber attack on Dyn disrupts internet across the U.S.";
    news.text[10][7] = "Twitter axes 9 pct of workforce as revenue growth stalls";
    news.text[10][8] = "Visa shares hit record high after strong Q4 earnings report";
    news.text[10][9] = "Amazon shares surge as revenue beats expectations";

    // Nov 2016
    news.text[11][0] = "Donald Trump wins US presidential election, markets plunge";
    news.text[11][1] = "Walmart agrees to $3.3 billion settlement over bribery charges";
    news.text[11][2] = "Samsung to buy Harman for $8 billion to boost automotive business";
    news.text[11][3] = "Opec agrees to cut oil production in bid to raise prices";
    news.text[11][4] = "UK government approves $24 billion nuclear power plant deal";
    news.text[11][5] = "Alibaba smashes sales records on Singles Day shopping event";
    news.text[11][6] = "Starbucks opens first premium Reserve bakery cafe";
    news.text[11][7] = "Snapchat parent files for $25 billion IPO valuation";
    news.text[11][8] = "Amazon develops AI-powered checkout-free grocery store";
    news.text[11][9] = "Uber faces more legal challenges over self-driving car project";

    // Dec 2016
    news.text[12][0] = "Dow crosses 20,000 milestone for the first time";
    news.text[12][1] = "Yahoo reveals 1 billion user accounts were hacked in 2013 breach";
    news.text[12][2] = "Uber shifts gears with $1 billion deal for self-driving startup";
    news.text[12][3] = "Samsung blames battery flaws for Galaxy Note 7 fires";
    news.text[12][4] = "Apple supplier Foxconn weighs $7 billion investment in US";
    news.text[12][5] = "Trump claims $50 billion SoftBank investment after meeting";
    news.text[12][6] = "Deutsche Bank agrees to $7.2 billion mortgage settlement";
    news.text[12][7] = "Snapchat confidentially files for $25 billion IPO valuation";
    news.text[12][8] = "Coca-Cola appoints Quincey as new CEO to succeed Muhtar Kent";
    news.text[12][9] = "Amazon hits $1,000 stock price with market cap above $475 billion";

    // Jan 2017
    news.text[13][0] = "Dow hits 20,000 for the first time as Trump rally continues";
    news.text[13][1] = "Netflix adds more subscribers than expected, shares surge";
    news.text[13][2] = "Snapchat parent Snap Inc. aims to raise $3 billion in IPO";
    news.text[13][3] = "Trump withdraws from Trans-Pacific Partnership trade deal";
    news.text[13][4] = "Samsung blames battery flaws for Galaxy Note 7 fires";
    news.text[13][5] = "Alibaba becomes major sponsor of Olympic Games through 2028";
    news.text[13][6] = "Starbucks to hire 10,000 refugees over next 5 years";
    news.text[13][7] = "GM to invest $1 billion in U.S. factories, add or keep 7,000 jobs";
    news.text[13][8] = "Apple files $1 billion lawsuit against Qualcomm over royalties";
    news.text[13][9] = "Uber hires NASA engineer to lead flying car project";

    // Feb 2017
    news.text[14][0] = "Snap Inc. prices IPO at $17 per share, valuing it at $24 billion";
    news.text[14][1] = "Kraft Heinz drops $143 billion bid for Unilever after rejection";
    news.text[14][2] = "Tesla swings to profit, driven by record electric vehicle sales";
    news.text[14][3] = "Uber CEO orders 'urgent' investigation into harassment claims";
    news.text[14][4] = "Amazon to create 100,000 full-time jobs in U.S. over next 18 months";
    news.text[14][5] = "Apple joins group working on open-source artificial intelligence";
    news.text[14][6] = "Samsung chief Lee Jae-yong arrested in corruption scandal";
    news.text[14][7] = "Walmart to create 10,000 U.S. jobs as part of investment plan";
    news.text[14][8] = "Verizon announces $350 million investment, reversing job cuts";
    news.text[14][9] = "Burger King and Tim Hortons owner to buy Popeyes for $1.8 billion";

    // Mar 2017
    news.text[15][0] = "Snap shares soar 44 pct on debut after $3.4 billion IPO";
    news.text[15][1] = "Amazon to acquire Middle Eastern online retailer Souq.com";
    news.text[15][2] = "Uber presidency left vacant amid turmoil at the company";
    news.text[15][3] = "Apple unveils new iPad and special edition iPhone 7 & 7 Plus";
    news.text[15][4] = "Google apologizes after ads appear next to extremist videos";
    news.text[15][5] = "Sears warns of 'going concern' doubts amid $2.2 billion loss";
    news.text[15][6] = "Toshiba's Westinghouse nuclear unit files for bankruptcy";
    news.text[15][7] = "Wells Fargo agrees to pay $142 million in fake account scandal";
    news.text[15][8] = "Ford to invest $1.2 billion in three Michigan plants";
    news.text[15][9] = "EU blocks London Stock Exchange's $28 billion merger with Deutsche Boerse";

    // Apr 2017
    news.text[16][0] = "United Airlines passenger violently dragged from overbooked flight";
    news.text[16][1] = "Tesla overtakes GM as most valuable US automaker";
    news.text[16][2] = "Uber caught tracking ex-users after them uninstalling app";
    news.text[16][3] = "Verizon launches unlimited data plan amid fierce competition";
    news.text[16][4] = "Wells Fargo claws back $75 million more from former executives";
    news.text[16][5] = "Bidding war erupts for $24 billion truck maker Navistar";
    news.text[16][6] = "Facebook's Messenger app tops 1.2 billion monthly users";
    news.text[16][7] = "FCC unveils plan to repeal Obama-era 'net neutrality' rules";
    news.text[16][8] = "Nintendo Switch sales top 2.7 million units in less than a month";
    news.text[16][9] = "Amazon acquires Dubai online retailer Souq.com for $650 million";


    // May 2017
    news.text[17][0] = "Uber admits 'major' underpayment blunder for NYC drivers";
    news.text[17][1] = "Amazon shares top $1,000 for the first time amid retail domination";
    news.text[17][2] = "Ford replaces CEO Mark Fields amid plunging share price";
    news.text[17][3] = "Microsoft refreshes Surface Pro, launches Windows 10 Fall update";
    news.text[17][4] = "Moody's downgrades China's credit rating amid debt concerns";
    news.text[17][5] = "Google's Android platform was a billion-dollar transfer from Apple";
    news.text[17][6] = "Cisco to lay off 1,100 more employees in extended restructuring";
    news.text[17][7] = "Bitcoin surges above $2,000 as global demand intensifies";
    news.text[17][8] = "Zuckerberg urges Harvard grads to embrace globalization";
    news.text[17][9] = "OPEC and allies extend production cuts to end of 2018";

    // Jun 2017
    news.text[18][0] = "Amazon to acquire Whole Foods for $13.7 billion";
    news.text[18][1] = "Uber CEO Travis Kalanick forced to resign after investor revolt";
    news.text[18][2] = "Google faces $2.7 billion EU fine for favoring its shopping service";
    news.text[18][3] = "Berkshire Hathaway buys 38.6 pct stake in trucker Pilot Flying J";
    news.text[18][4] = "Walmart reportedly exploring way to offer low-cost streaming service";
    news.text[18][5] = "Boeing strikes deal to sell planes to Iran worth $20 billion";
    news.text[18][6] = "Elon Musk offers to help rebuild Puerto Rico's power grid";
    news.text[18][7] = "Alibaba joins bidding war for India's leading e-commerce company";
    news.text[18][8] = "Petya cyber attack strikes companies across Europe and US";
    news.text[18][9] = "Google to stop scanning Gmail for ad targeting";

    // Jul 2017
    news.text[19][0] = "Amazon's Prime Day sales topped $1 billion despite website issues";
    news.text[19][1] = "FTC approves $13.7 billion acquisition of Whole Foods by Amazon";
    news.text[19][2] = "Elon Musk's Tesla to build world's largest battery in Australia";
    news.text[19][3] = "Blue Apron slashes IPO price as Amazon looms in meal-kit market";
    news.text[19][4] = "Michael Kors to buy luxury shoemaker Jimmy Choo for $1.2 billion";
    news.text[19][5] = "Alphabet beats expectations despite $2.7 billion EU antitrust fine";
    news.text[19][6] = "Uber's legal woes worsen as ex-employee alleges coverup effort";
    news.text[19][7] = "PayPal to integrate with Amazon's Alexa digital assistant";
    news.text[19][8] = "Daimler to recall 3 million Mercedes-Benz diesels in Europe";
    news.text[19][9] = "Foxconn unveils plan to build $10 billion LCD factory in Wisconsin";

    // Aug 2017
    news.text[20][0] = "Amazon issues $16 billion bond sale to fund Whole Foods deal";
    news.text[20][1] = "Tesla bonds price with junk rating, 5.3% yield amid cash burn";
    news.text[20][2] = "Disney to launch ESPN and Disney-branded streaming services";
    news.text[20][3] = "Uber agrees to 20 years of privacy audits to settle charges";
    news.text[20][4] = "Tencent becomes world's 10th largest company by market value";
    news.text[20][5] = "Alibaba beats expectations as online sales in China surge";
    news.text[20][6] = "Sempra Energy to buy Oncor for $9.45 billion after Buffett bows out";
    news.text[20][7] = "Samsung unveils Galaxy Note 8 after battery fiasco";
    news.text[20][8] = "Walmart partners with Google on voice-based shopping";
    news.text[20][9] = "BlackBerry misses sales forecasts, shares tumble";

    // Sep 2017
    news.text[21][0] = "Equifax breach exposed data of 143 million U.S. consumers";
    news.text[21][1] = "Apple unveils $999 iPhone X with face recognition technology";
    news.text[21][2] = "SEC reveals cyber breach, raising concerns about internal lapses";
    news.text[21][3] = "Toys 'R' Us files for bankruptcy amid crushing debt load";
    news.text[21][4] = "Uber loses license to operate in London after sexual assaults";
    news.text[21][5] = "Northrop Grumman buys aerospace firm Orbital ATK for $9.2 billion";
    news.text[21][6] = "Facebook says it will hand Russian ads over to Congress";
    news.text[21][7] = "Tesla working with AMD to develop chip for self-driving cars";
    news.text[21][8] = "Amazon plans second headquarters, opens $5 billion investment bidding";
    news.text[21][9] = "Lufthansa in talks to buy parts of insolvent Air Berlin";

    // Oct 2017
    news.text[22][0] = "Amazon receives 238 proposals for its second headquarters";
    news.text[22][1] = "Kobe Steel faked product data for over 10 years, sending shockwaves";
    news.text[22][2] = "Alphabet's Google uncovers Russia-backed ads on YouTube, Gmail";
    news.text[22][3] = "Tesla fires hundreds of workers as company struggles with production";
    news.text[22][4] = "Airbus to acquire majority stake in Bombardier C Series program";
    news.text[22][5] = "AT&T's $85 billion acquisition of Time Warner hit with lawsuit";
    news.text[22][6] = "CVS Health to acquire health insurer Aetna for $69 billion";
    news.text[22][7] = "Intel products hit with security flaw in widespread 'KRACK' attack";
    news.text[22][8] = "Bitcoin soars above $6,000, doubling in price in less than a month";
    news.text[22][9] = "Disney extends buyout negotiations after hitting a 'slew of issues'";

    // Nov 2017
    news.text[23][0] = "Saudi Arabia's sovereign wealth fund builds $2 billion stake in Uber";
    news.text[23][1] = "Broadcom bids $130 billion for Qualcomm in landmark deal";
    news.text[23][2] = "Alibaba smashes Singles Day record with over $25 billion in sales";
    news.text[23][3] = "Bitcoin surges past $8,000 as investors pile into cryptocurrency";
    news.text[23][4] = "Uber loses lawsuit against Waymo over alleged tech theft";
    news.text[23][5] = "Amazon Web Services outage wreaks havoc across the internet";
    news.text[23][6] = "Toshiba agrees to sell chip unit to Bain Capital for $18 billion";
    news.text[23][7] = "Volvo to supply Uber with up to 24,000 self-driving cars";
    news.text[23][8] = "AT&T's $85 billion acquisition of Time Warner approved by judges";
    news.text[23][9] = "Tesla unveils its first electric semi-truck and new Roadster";
    
    // Dec 2017
    news.text[24][0] = "Bitcoin futures make debut on major exchanges, prices surge";
    news.text[24][1] = "Congressional Republicans reach deal on final tax overhaul bill";
    news.text[24][2] = "Disney agrees to buy key 21st Century Fox assets for $52.4 billion";
    news.text[24][3] = "Fed raises interest rates amid stronger economic growth";
    news.text[24][4] = "Apple faces lawsuits after admitting to slowing down older iPhones";
    news.text[24][5] = "Uber sells stake to SoftBank achieving $48 billion valuation";
    news.text[24][6] = "Cryptocurrency bitcoin breaks through $20,000 milestone";
    news.text[24][7] = "Overstock becomes first major retailer to accept cryptocurrency";
    news.text[24][8] = "Trump blocks $1.2 billion Moneygram acquisition by Alibaba affiliate";
    news.text[24][9] = "Facebook to book tax expenses related to Republican tax reforms";

    // Jan 2018
    news.text[25][0] = "Cryptocurrency markets suffer historic price crash and volatility";
    news.text[25][1] = "Apple plans to pay $38 billion in tax on overseas cash";
    news.text[25][2] = "Amazon climbs into elite club of $1 trillion companies";
    news.text[25][3] = "Facebook overhauls News Feed to focus on friends and family";
    news.text[25][4] = "Intel hit with three lawsuits over security flaw in chip designs";
    news.text[25][5] = "Meltdown and Spectre flaws affect virtually all modern processors";
    news.text[25][6] = "Tesla delays Model 3 production targets yet again";
    news.text[25][7] = "YouTube pulls ads from Logan Paul videos over suicide forest clip";
    news.text[25][8] = "SpaceX launches secret Zuma payload, but mission status is unconfirmed";
    news.text[25][9] = "GM launches new Chevrolet Silverado pickup to take on Ford F-150";

    // Feb 2018
    news.text[26][0] = "Dow plunges nearly 1,600 points, its biggest intraday point drop ever";
    news.text[26][1] = "Jeff Bezos tops Bill Gates as world's richest person";
    news.text[26][2] = "Snapchat update leads to backlash and 1.2 million signed petition";
    news.text[26][3] = "Elon Musk's Tesla Roadster launched into space aboard SpaceX rocket";
    news.text[26][4] = "Amazon reportedly laying off hundreds of corporate employees";
    news.text[26][5] = "Bill Gates could no longer be world's richest if he paid off US debt";
    news.text[26][6] = "Bitcoin tumbles below $6,000 amid broader cryptocurrency selloff";
    news.text[26][7] = "Roku outperformed all other major technology stocks in 2017";
    news.text[26][8] = "Warren Buffett outlines 'terrible' mistake of not investing in Amazon";
    news.text[26][9] = "Ford ousts chief of North American operations amid misconduct probe";

    // Mar 2018
    news.text[27][0] = "Facebook stock plunges amid Cambridge Analytica data scandal";
    news.text[27][1] = "Uber exits Southeast Asia in new swap deal with Grab";
    news.text[27][2] = "Trump blocks Broadcom's $117 billion bid for Qualcomm on security grounds";
    news.text[27][3] = "Dropbox raises $756 million in biggest tech IPO of 2018 so far";
    news.text[27][4] = "Tesla says autopilot was active during fatal Model X crash";
    news.text[27][5] = "Amazon is now second most valuable US company, tops Alphabet";
    news.text[27][6] = "Alibaba develops car vending machine for buyers in China";
    news.text[27][7] = "YouTube tries to crack down on video creators for conspiracy theories";
    news.text[27][8] = "Volkswagen stored data on 3.3 million truck owners in unauthorized way";
    news.text[27][9] = "Trump ousts Secretary of State Rex Tillerson in another shakeup";

    // Apr 2018
    news.text[28][0] = "Facebook reveals data on 87 million users was improperly shared";
    news.text[28][1] = "Amazon tops 100 million Prime subscribers, pushing towards profitability";
    news.text[28][2] = "Uber welcomes Toyota as latest investor with $500 million stake";
    news.text[28][3] = "T-Mobile and Sprint restart merger talks after years of battling";
    news.text[28][4] = "Tesla struggles to meet Model 3 production goals, Musk details excessive toil";
    news.text[28][5] = "Apple reports strong earnings but warns of higher prices ahead";
    news.text[28][6] = "FAA orders inspection of 352 Boeing 737 engines after Southwest accident";
    news.text[28][7] = "Alibaba buys Pakistani e-commerce leader Daraz in latest play";
    news.text[28][8] = "YouTube targets conspiracy videos in widespread account purge";
    news.text[28][9] = "Twitter amps up efforts to crack down on toxic content, fake accounts";

    // May 2018
    news.text[29][0] = "Elon Musk says Tesla will fix production issues by reorganizing management";
    news.text[29][1] = "Uber unveils plans to launch food delivery drones by 2021";
    news.text[29][2] = "Warren Buffett criticizes bitcoin as nonproductive and illicit";
    news.text[29][3] = "Cambridge Analytica files for bankruptcy in US after Facebook data scandal";
    news.text[29][4] = "Amazon goes head-to-head with Tencent in Southeast Asian cloud war";
    news.text[29][5] = "Microsoft launches $25 million program to use AI for disabilities";
    news.text[29][6] = "Instagram launches video chat and new explore features";
    news.text[29][7] = "US and China put trade war on hold after talks in Washington";
    news.text[29][8] = "Tesla Autopilot update to include first 'full self-driving features'";
    news.text[29][9] = "Philip Morris gets approval to market alternative cigarette as less risky";

    // Jun 2018
    news.text[30][0] = "Tesla cuts 9 percent of workforce in quest to post profit";
    news.text[30][1] = "Disney raises bid for Fox assets to $71.3 billion, outflanking Comcast";
    news.text[30][2] = "Uber begins using artificial intelligence to detect drunk passengers";
    news.text[30][3] = "Microsoft employees revolt against working with US on 'cruel' deportations";
    news.text[30][4] = "EU hits Google with $5 billion antitrust fine over Android practices";
    news.text[30][5] = "Supreme Court rules states can collect internet sales tax";
    news.text[30][6] = "Amazon gains new online footing with $1 billion PillPack acquisition";
    news.text[30][7] = "Starbucks to accelerate US store closings next year";
    news.text[30][8] = "BlackBerry bids $1.4 billion for cybersecurity firm Cylance";
    news.text[30][9] = "Slack staff cracks down on hate speech by racist tech fraternity members";

    // Jul 2018
    news.text[31][0] = "Broadcom agrees to acquire software company CA Technologies for $18.9 billion";
    news.text[31][1] = "Netflix shares take hit as subscriber slip hints at 'hubris' problem";
    news.text[31][2] = "Amazon Prime Day becomes an internet traffic nightmare";
    news.text[31][3] = "Microsoft secures $4.6 billion contract to supply US army with AR headsets";
    news.text[31][4] = "Elon Musk faces backlash after insulting Thai cave rescue diver";
    news.text[31][5] = "Comcast outbids Fox in $39 billion battle for Sky TV in Britain";
    news.text[31][6] = "Facebook hit with first fine in Cambridge Analytica data leak scandal";
    news.text[31][7] = "Tesla achieves Model 3 production goals, but cuts costs remain";
    news.text[31][8] = "Twitter purges more locked accounts from follower counts";
    news.text[31][9] = "Uber gets approval to put self-driving cars on roads again";

    // Aug 2018
    news.text[32][0] = "Tesla CEO Elon Musk considers taking company private, faces SEC scrutiny";
    news.text[32][1] = "Apple hits $1 trillion market cap before sliding back";
    news.text[32][2] = "Alibaba's quarterly revenue surges 61 percent amid e-commerce boom";
    news.text[32][3] = "MoviePass slashes access to block availability amid cash crisis";
    news.text[32][4] = "Uber narrows losses but growth slows amid cash burn";
    news.text[32][5] = "Twitter CEO commits to fixing toxic content after racist attack";
    news.text[32][6] = "Microsoft soars past $800 billion in market value for the first time";
    news.text[32][7] = "Google staff protest over censored China search engine 'Dragonfly'";
    news.text[32][8] = "Coca-Cola to buy UK coffee chain Costa for $5.1 billion";
    news.text[32][9] = "Amazon joins $1 trillion club, becomes second company after Apple";

    // Sep 2018
    news.text[33][0] = "Amazon becomes the second $1 trillion company in the U.S";
    news.text[33][1] = "Tesla says it will remain a public company after Musk's controversial tweet";
    news.text[33][2] = "Apple unveils iPhone XS, XS Max and lower-cost XR models";
    news.text[33][3] = "Ticketmaster recruits ethicists to review its business practices";
    news.text[33][4] = "Alibaba's Jack Ma to step down as chairman in September 2019";
    news.text[33][5] = "Uber agrees to pay $148 million over allegation of data breach coverup";
    news.text[33][6] = "Walmart opens first e-commerce distribution center to take on Amazon";
    news.text[33][7] = "Spotify announces it will now work with Alexa and Amazon devices";
    news.text[33][8] = "Uber driver pay data for 92 million drivers exposed in hack";
    news.text[33][9] = "Google staff discussed tweaking search results to counter travel ban";

    // Oct 2018
    news.text[34][0] = "Saudi Arabia invites global investors to participate in IPO of Aramco";
    news.text[34][1] = "Tesla launches new $45,000 Model 3 sedan for the masses";
    news.text[34][2] = "Facebook security breach exposes 50 million user accounts";
    news.text[34][3] = "Canada follows Uruguay in legalizing recreational marijuana nationwide";
    news.text[34][4] = "Amazon launches first AI hybrid online/physical shopping experience";
    news.text[34][5] = "SoftBank's massive $100 billion Vision Fund struggles on some bets";
    news.text[34][6] = "Microsoft co-founder Paul Allen dies at 65 after battle with cancer";
    news.text[34][7] = "Google unveils new Pixel 3 phones, Home Hub and AI computer titles";
    news.text[34][8] = "Elon Musk says first tunnel for LA 'disturbingly long' underground bus route almost done";
    news.text[34][9] = "Uber IPO could put company value at $120 billion, sources say";

    // Nov 2018
    news.text[35][0] = "Amazon picks New York City and Virginia for $5 billion new headquarters";
    news.text[35][1] = "Walmart beats Amazon's online prices for the first time, study shows";
    news.text[35][2] = "Microsoft overtakes Apple as the most valuable US company";
    news.text[35][3] = "Facebook CEO Mark Zuckerberg rejects criticism over handling of scandals";
    news.text[35][4] = "Uber reports $1 billion loss as comeback from scandals proves painful";
    news.text[35][5] = "Disney set to gain ground with newly approved Fox deal";
    news.text[35][6] = "Elon Musk's Boring Company scraps plans for LA Westside tunnel";
    news.text[35][7] = "Bitcoin sinks below $5,000, half its 2018 peak of near $20,000";
    news.text[35][8] = "Juul halts store sales of some flavored e-cigarette pods amid outcry";
    news.text[35][9] = "GM to lay off up to 14,000 factory and white-collar workers";

    // Dec 2018
    news.text[36][0] = "Dow suffers worst Christmas Eve massacre, dropping 653 points";
    news.text[36][1] = "Apple shares drop 7% after warning of weak holiday iPhone sales";
    news.text[36][2] = "Jeff and MacKenzie Bezos announce plans to divorce after 25 years";
    news.text[36][3] = "Dow caps off worst year since 2008 financial crisis amid trade tensions";
    news.text[36][4] = "Tesla misses Model 3 delivery targets yet again, stock plunges";
    news.text[36][5] = "Amazon issues no-fee salary advance amid worker cash crisis";
    news.text[36][6] = "Ford recalls 874,000 F-Series pickup trucks for fire risks";
    news.text[36][7] = "Wells Fargo agrees to $575 million settlement over improper practices";
    news.text[36][8] = "SpaceX raises $500 million from investors as internet satellite production ramps";
    news.text[36][9] = "Facebook refutes claims of sharing user data without permission";

    // Jan 2019
    news.text[37][0] = "Apple issues rare revenue warning citing weak China sales";
    news.text[37][1] = "Stocks suffer worst December since the Great Depression";
    news.text[37][2] = "PG&E files for bankruptcy amid wildfire lawsuits and billions in liabilities";
    news.text[37][3] = "Billionaire Jeff Bezos announces divorce from wife MacKenzie after 25 years";
    news.text[37][4] = "Huawei employee arrested in Poland over spying allegations";
    news.text[37][5] = "Tesla misses Model 3 delivery targets again, cuts prices";
    news.text[37][6] = "Snap employees fear being left behind in US government shutdown";
    news.text[37][7] = "Netflix raises subscription prices to fund new content";
    news.text[37][8] = "World Bank issues warning over global 'storm' of debt";
    news.text[37][9] = "Cryptocurrency bitcoin drops below $3,500, down 80% from highs";

    // Feb 2019
    news.text[38][0] = "Amazon scraps plans to build headquarters in New York City";
    news.text[38][1] = "Tesla loses $700 million after another difficult year";
    news.text[38][2] = "Payless ShoeSource to close all remaining US stores";
    news.text[38][3] = "Kraft Heinz discloses $15 billion writedown, SEC investigation";
    news.text[38][4] = "Honda to shutdown UK plant in latest Brexit blow";
    news.text[38][5] = "Samsung unveils foldable phone, the Galaxy Fold";
    news.text[38][6] = "Elon Musk's security clearance under review over marijuana use";
    news.text[38][7] = "Fiat Chrysler to pay $700 million for emissions cheating";
    news.text[38][8] = "SpaceX launches triple core rocket and crew capsule";
    news.text[38][9] = "Warren Buffett says Kraft Heinz problems are 'my fault'";   
    
    // Mar 2019
    news.text[39][0] = "Apple launches news.text subscription app Apple News+ for $9.99 a month";
    news.text[39][1] = "Lyft kicks off investor road show for IPO expected to value it at $23bn";
    news.text[39][2] = "Facebook suffers its worst outage in over a decade";
    news.text[39][3] = "Pinterest joins rush of tech companies planning 2019 IPOs";
    news.text[39][4] = "Cruise ship company Virgin Voyages turns to Boeing 737 crashes";
    news.text[39][5] = "FAA grounds Boeing 737 Max planes after fatal Ethiopia crash";
    news.text[39][6] = "Bayer's $63 billion Monsanto takeover upended by Roundup cancer ruling";
    news.text[39][7] = "Facebook faced criminal investigation for data sharing deals";
    news.text[39][8] = "Volkswagen CEO apologizes after evoking Nazi slogan 'Arbeit macht frei'";
    news.text[39][9] = "Levi Strauss valued at $6.6 billion for iconic jeans maker's return to markets";

    // Apr 2019
    news.text[40][0] = "Uber reveals IPO terms that could value company at up to $84 billion";
    news.text[40][1] = "Pinterest valued at $12.7 billion after IPO, rallies in market debut";
    news.text[40][2] = "Samsung retrieving all Galaxy Fold samples after defective screens";
    news.text[40][3] = "Tesla says investigating incident of car catching fire in Shanghai";
    news.text[40][4] = "Amazon to pull plug on China domestic e-commerce business";
    news.text[40][5] = "Boeing says it knew about 737 Max sensor problem before crashes";
    news.text[40][6] = "Elon Musk, SEC go to court over his Tesla tweets";
    news.text[40][7] = "Microsoft hits $1 trillion market cap after reporting strong earnings";
    news.text[40][8] = "Disney unveils $6.99 streaming bundle with Hulu, ESPN+ for Netflix attack";
    news.text[40][9] = "Anadarko Petroleum rejects Occidental's $38 billion offer";

    // May 2019
    news.text[41][0] = "Uber bumps up its IPO pricing to raise $8.1 billion";
    news.text[41][1] = "Trump increases tariffs on $200 billion of Chinese goods to 25%";
    news.text[41][2] = "Huawei hit by US export controls, potential import ban";
    news.text[41][3] = "Uber begins trading at $42 per share in anticipated IPO";
    news.text[41][4] = "Fiat Chrysler proposes merger with Renault to create auto giant";
    news.text[41][5] = "GameStop shares plunge 36% as console sales miss estimates";
    news.text[41][6] = "Amazon shareholders reject motion to ban facial recognition sales";
    news.text[41][7] = "Tesla stock and bonds tumble after Musk's 'Eferesced' tweet for beer";
    news.text[41][8] = "Facebook takes step to integrate apps for encrypted messaging";
    news.text[41][9] = "Avengers: Endgame kicks off a huge summer for Disney";

    // Jun 2019
    news.text[42][0] = "Uber loses $1 billion in Q1 as revenue growth slows";
    news.text[42][1] = "YouTube updates hate speech policies to prohibit 'supremacist' videos";
    news.text[42][2] = "Target summons unicorn magic amid turnaround with share price at record high";
    news.text[42][3] = "Apple faces triple threat as Iran revokes developer licenses";
    news.text[42][4] = "ByteDance, TikTok's parent, is Apple's newest threat with addictive apps";
    news.text[42][5] = "Hackers steal over $40 million worth of Bitcoin from Binance exchange";
    news.text[42][6] = "Microsoft pulls Huawei laptop after adding it overnight";
    news.text[42][7] = "Amazon's Jeff Bezos wants to spend 'Lord of the Rings' money on space";
    news.text[42][8] = "FedEx no longer will fly your Amazon packages on express plane";
    news.text[42][9] = "Tesla CEO Elon Musk earned a compensation of exactly $2.3 billion last year";

    // Jul 2019
    news.text[43][0] = "Facebook hit with $5 billion privacy violation fine by FTC";
    news.text[43][1] = "Amazon workers strike as 'Prime' shopping frenzy hits";
    news.text[43][2] = "Capital One data breach hits about 100 million people";
    news.text[43][3] = "Samsung's folding phone, the Galaxy Fold, finally goes on sale";
    news.text[43][4] = "Equifax's $700 million data breach settlement spurs criticism";
    news.text[43][5] = "Twitter shares fall as hacking claims account for slowing user growth";
    news.text[43][6] = "Apple beats profit estimates amid declining iPhone sales";
    news.text[43][7] = "FaceApp mystery man's digital 'robacle' provides AI wake up call";
    news.text[43][8] = "James Paterek sued by Elon Musk after calling him a 'pedo guy'";
    news.text[43][9] = "Trump administration takes action against France for tech tax";

    // Aug 2019
    news.text[44][0] = "Apple's credit card being launched in partnership with Goldman Sachs";
    news.text[44][1] = "Disney emerges as winner of Fox entertainment assets after Comcast drops bid";
    news.text[44][2] = "Global economic growth is 'fragile', says former Fed Chair Janet Yellen";
    news.text[44][3] = "Google staff demand company issues climate plan amid worker departures";
    news.text[44][4] = "Overstock CEO resigns after 'deep state' comments cause investor revolt";
    news.text[44][5] = "Billionaire David Koch, conservative donor, dies at age 79";
    news.text[44][6] = "Twitter, Facebook accuse China of harboring Hong Kong disinformation campaign";
    news.text[44][7] = "Nasdaq confirms its systems were knocked offline by data center issue";
    news.text[44][8] = "GM workers call for strike amid contract negotiations at UAW";
    news.text[44][9] = "Sony buys 'Spider-Man' video game developer Insomniac Games";

    // Sep 2019
    news.text[45][0] = "WeWork postpones IPO after investors question value and corporate governance";
    news.text[45][1] = "Saudi Aramco gives green light for world's biggest IPO";
    news.text[45][2] = "General Motors workers in U.S. go on nationwide strike";
    news.text[45][3] = "Uber makes biggest labor deal by giving 70,000 drivers in Canada union membership";
    news.text[45][4] = "Google wins legal battle over hugely profitable Android mobile operating system";
    news.text[45][5] = "Amazon's Alexa will start giving medical advice from the NHS website";
    news.text[45][6] = "Huawei founder says he wouldn't protest if daughter Meng were freed";
    news.text[45][7] = "Fed lowers rates again amid slowing growth but signals potential pause";
    news.text[45][8] = "Tesla's Model 3 earns top safety rating from IIHS";
    news.text[45][9] = "Anheuser-Busch accuses MillerCoors of stealing secret recipes";
    
    // Oct 2019
    news.text[46][0] = "Mark Zuckerberg defends Facebook's policies amid criticism and protests";
    news.text[46][1] = "WeWork accepts SoftBank takeover that will oust founder Adam Neumann";
    news.text[46][2] = "California hits Uber and Lyft with gig economy labor law";
    news.text[46][3] = "Boeing aims to resume 737 MAX production by early 2020";
    news.text[46][4] = "SoftBank to take control of WeWork with nearly $10 billion rescue payday";
    news.text[46][5] = "Biogen's shocking Alzheimer's data blew away expectations, vaulting stock 26%";
    news.text[46][6] = "Apple says streaming TV service Apple TV Plus will be $4.99 per month";
    news.text[46][7] = "FAA panel says Boeing 737 MAX software is 'operationally suitable'";
    news.text[46][8] = "Johnson & Johnson agrees to settle Ohio opioid lawsuits for $20.4 million";
    news.text[46][9] = "McDonald's CEO fired for consensual relationship with employee";

    // Nov 2019
    news.text[47][0] = "Uber loses $1.2 billion in one quarter as growth slows";
    news.text[47][1] = "Disney+ streaming service hits 10 million subscribers on first day";
    news.text[47][2] = "Google partners with health firm to secretly gather millions of patient records";
    news.text[47][3] = "Bill Gates overtakes Jeff Bezos as world's richest person";
    news.text[47][4] = "Tesla unveils its electric 'Cybertruck' pickup amid doubts";
    news.text[47][5] = "Amazon fights Microsoft's $10 billion 'war cloud' deal with protest";
    news.text[47][6] = "Apple co-founder Steve Wozniak joins legal battle against YouTube ripper";
    news.text[47][7] = "China receives 37 aircraft from Boeing following trade truce with US";
    news.text[47][8] = "Alibaba smashes sales records on Singles Day amid Hong Kong protests";
    news.text[47][9] = "Uber says it'll pratice truth on TV: mileage, emissions impact";

    // Dec 2019
    news.text[48][0] = "Saudi Aramco's record IPO raises $25.6 billion, hitting top of range";
    news.text[48][1] = "Uber receives approval to test self-driving cars on California roads";
    news.text[48][2] = "New law signed by Trump could allow US to monitor Huawei equipment";
    news.text[48][3] = "Boeing fires CEO Dennis Muilenburg amid 737 MAX crisis";
    news.text[48][4] = "Amazon blaming 'technical issue' for removing some employee criticism";
    news.text[48][5] = "Nike ousts Amazon to become most valuable apparel company";
    news.text[48][6] = "Facebook agrees to pay $550 million to end Illinois facial recognition lawsuit";
    news.text[48][7] = "Elon Musk wins defamation trial over 'pedo guy' insult";
    news.text[48][8] = "Tesla moving ahead with using in-car video fromDriverCam";
    news.text[48][9] = "Google founders Larry Page and Sergey Brin resign as Alphabet leaders";

    // Jan 2020
    news.text[49][0] = "US stocks suffer worst day in a month on coronavirus fears";
    news.text[49][1] = "Boeing's 737 MAX crisis deepens as production stops for first time";
    news.text[49][2] = "Tesla's stock rockets higher, leaving 'smart money' investors bewildered";
    news.text[49][3] = "Microsoft looks to outsource more video game work";
    news.text[49][4] = "Jeff Bezos net worth hits $109 billion after successful holiday season";
    news.text[49][5] = "International panel faults Boeing and FAA over 737 MAX crisis";
    news.text[49][6] = "Google parent company Alphabet joins $1 trillion market cap club";
    news.text[49][7] = "McDonald's rolls out two new crispy chicken sandwiches nationwide";
    news.text[49][8] = "Grounded 737 MAX cancellations make Boeing's first 2020 loss likely";
    news.text[49][9] = "Jeff Bezos iPhone X breach increases pressure on Saudi Arabia";

    // Feb 2020
    news.text[50][0] = "Dow plunges over 1,000 points amid coronavirus fears";
    news.text[50][1] = "T-Mobile and Sprint merger approved by federal judge";
    news.text[50][2] = "Coronavirus disrupts Apple's manufacturing schedule in China";
    news.text[50][3] = "Facebook to pay $550 million over Illinois privacy violation suit";
    news.text[50][4] = "Apple warns of quarterly revenue shortfall due to coronavirus impact";
    news.text[50][5] = "ViacomCBS begins trading after Viacom, CBS merger completed";
    news.text[50][6] = "Tesla becomes world's most valuable automaker amid stock rally";
    news.text[50][7] = "Amazon files protest as Microsoft wins $10 billion Pentagon contract";
    news.text[50][8] = "Coronavirus declared global health emergency by WHO";
    news.text[50][9] = "Jeff Bezos launches $10 billion fund to fight climate change";

    // Mar 2020
    news.text[51][0] = "Stocks enter bear market as coronavirus pandemic devastates economy";
    news.text[51][1] = "Biden and Sanders cancel rallies as coronavirus upends 2020 race";
    news.text[51][2] = "NBA suspends season amid coronavirus outbreak after player tests positive";
    news.text[51][3] = "Apple reopens all stores outside of coronavirus hotspot China";
    news.text[51][4] = "NYSE to temporarily close iconic trading floor amid COVID-19 outbreak";
    news.text[51][5] = "Fed slashes interest rates to near zero to support US economy";
    news.text[51][6] = "US airlines seek over $50 billion in federal aid amid coronavirus crisis";
    news.text[51][7] = "Amazon struggles with mass homebound shopping demand amid COVID-19";
    news.text[51][8] = "Uber, Lyft drivers promised sick pay if they have COVID-19";
    news.text[51][9] = "Tesla Fremont factory remains open despite shelter-in-place order";

    // Apr 2020
    news.text[52][0] = "Coronavirus leaves millions of Americans without jobs, income";
    news.text[52][1] = "Oil prices turn negative as coronavirus obliterates demand";
    news.text[52][2] = "Google launches nationwide COVID-19 community mobility reports";
    news.text[52][3] = "Amazon fired worker who led strike for time off, higher pay amid COVID-19";
    news.text[52][4] = "Zoom is now worth more than the world's 7 biggest airlines combined";
    news.text[52][5] = "AMC Theatres says it has enough cash to reopen this summer as theaters go dark";
    news.text[52][6] = "Airbnb raising $1 billion amid COVID-19 disruption to travel business";
    news.text[52][7] = "Apple and Google team up for COVID-19 contact tracing technology";
    news.text[52][8] = "Ford, GE plan to produce 50,000 ventilators in 100 days";
    news.text[52][9] = "Twitter employees can work from home 'forever' if they wish";

    // May 2020
    news.text[53][0] = "Uber cuts 3,700 jobs as pandemic shuts down ride-sharing business";
    news.text[53][1] = "Jeff Bezos could become world's first trillionaire by 2026 at current rate";
    news.text[53][2] = "Hertz files for bankruptcy amid pandemic rental car crisis";
    news.text[53][3] = "SpaceX's historic astronaut launch kicked off new era of spaceflight";
    news.text[53][4] = "Elon Musk defies orders, reopens Tesla plant in California";
    news.text[53][5] = "JCPenney Files for Bankruptcy as COVID-19 Pushes Retail Chain Over Edge";
    news.text[53][6] = "Zuckerberg says Facebook staff can work from anywhere for the next decade";
    news.text[53][7] = "EU lays out $824B coronavirus pandemic aid plan 'befitting' crisis";
    news.text[53][8] = "Amazon expects to spend all $4 billion earned in Q2 on COVID-19 costs";
    news.text[53][9] = "Sony reportedly entering billion-dollar deal to acquire Warzone developer";

    // Jun 2020
    news.text[54][0] = "Coronavirus recession ends after 3 months, making it shortest on record";
    news.text[54][1] = "PepsiCo to rebrand Aunt Jemima products, remove logo over racist origins";
    news.text[54][2] = "Amazon buying self-driving startup Zoox for $1.2 billion";
    news.text[54][3] = "McDonald's faces racial discrimination lawsuit from former franchisees";
    news.text[54][4] = "Amazon aims to hire 33,000 people as demand continues surging";
    news.text[54][5] = "Samsung plans to discontinue its premium Galaxy Note smartphones";
    news.text[54][6] = "Google makes WFH permanent, gives workers $1,000 allowance to outfit home offices";
    news.text[54][7] = "Tesla hits goal of 500,000 vehicle deliveries in 2020 two years early";
    news.text[54][8] = "Spotify touts podcast gains, Disney partnership as COVID-19 weighs on ad revenue";
    news.text[54][9] = "Apple pays $175 million for violating patent on ebooks purchased via apps";

    // Jul 2020
    news.text[55][0] = "Apple becomes first US company to reach $2 trillion market value";
    news.text[55][1] = "SpaceX wins NASA contract to build spacecraft for future Artemis moon mission";
    news.text[55][2] = "Uber buys Postmates for $2.65 billion in all-stock deal";
    news.text[55][3] = "US tech giants testify before Congress in historic antitrust hearing";
    news.text[55][4] = "NASA launches new Perseverance Mars rover from Florida's Cape Canaveral";
    news.text[55][5] = "Samsung unveils the Galaxy Z Fold2 with improved foldable screen";
    news.text[55][6] = "Twitter hit with large hack targeting Biden, Obama, Musk and others";
    news.text[55][7] = "UK bans Huawei from its 5G network, angering China and pleasing Trump";
    news.text[55][8] = "Philip Morris wins dismissal of $109 million patent case against Swisher";
    news.text[55][9] = "Microsoft to keep working from home until at least January 2021";

    // Aug 2020
    news.text[56][0] = "Apple becomes first US public company to hit $2 trillion market cap";
    news.text[56][1] = "Big US companies form union to acquire Covid-19 testing capabilities";
    news.text[56][2] = "US deficit climbed to $2.8 trillion in first 10 months of 2020";
    news.text[56][3] = "Epic Games sues Apple and Google after Fortnite is kicked off app stores";
    news.text[56][4] = "Derek Jeter's startup raises $125 million and continues building Yankees brand";
    news.text[56][5] = "FAA outlines rules for Boeing to certify 737 MAX fixes";
    news.text[56][6] = "FedEx overcharged businesses $21.5 million for years, audit shows";
    news.text[56][7] = "US senators revolt after Trump picks Louis DeJoy as postmaster general";
    news.text[56][8] = "OpenText acquires cloud security firm Carbonite for $1.42 billion";
    news.text[56][9] = "EU slams Apple for corporate tax shenanigans in Ireland, hits with $14.9B bill";

    // Sep 2020
    news.text[57][0] = "Tesla stages massive 'Battery Day' rally after stock split takes effect";
    news.text[57][1] = "UK to challenge Nvidia's $40 billion ARM acquisition over security risks";
    news.text[57][2] = "Fortune reports the 600 richest people saw $637B increase in 2020";
    news.text[57][3] = "Chewy stock pops after retailer reports $3.1 billion in quarterly sales";
    news.text[57][4] = "Microsoft to acquire video game company Bethesda for $7.5 billion";
    news.text[57][5] = "TikTok asks US court to intervene as deadline for sale nears";
    news.text[57][6] = "Oracle beats Microsoft in deal to buy TikTok's US business operations";
    news.text[57][7] = "Nikola founder Trevor Milton resigns amid fraud allegations";
    news.text[57][8] = "Amazon bans foreign seed sales in US after mysterious packages arrive";
    news.text[57][9] = "Walmart takes ownership stake in TikTok Global as Oracle deal is finalized";

    // Oct 2020
    news.text[58][0] = "Ant Group IPO pricing puts value at $316B, set for largest offering ever";
    news.text[58][1] = "Google to pay $1 billion to publishers over three years for news content";
    news.text[58][2] = "Regeneron antibody drug reduces virus levels in non-hospitalized patients";
    news.text[58][3] = "iPhone 12 and 5G wireless networks could boost economic growth, says analyst";
    news.text[58][4] = "DOJ approves $16 billion MWAA utility sale, despite opposition from Congress";
    news.text[58][5] = "Beyond Meat strikes Walmart, Target sushi deals, plots future steak release";
    news.text[58][6] = "Apple unveils first over-ear headphones and smaller HomePod mini speaker";
    news.text[58][7] = "Facebook accused of censorship amid complicated fact-checking partnerships";
    news.text[58][8] = "Toyota recalling up to 5.84 million vehicles for defective fuel pumps";
    news.text[58][9] = "SpaceX's Crew-1 astronaut launch marks the beginning of a new era";

    // Nov 2020
    news.text[59][0] = "Dow crosses 30,000 for first time as investors cheer Trump exit, Biden win";
    news.text[59][1] = "Elon Musk overtakes Bill Gates to become world's second richest man";
    news.text[59][2] = "Airbnb aims for $35 billion IPO valuation in expectation-defying debut";
    news.text[59][3] = "DoorDash launches IPO after COVID-19 delivery boom, valued at $32 billion";
    news.text[59][4] = "Slack sold to Salesforce for $27.7 billion in cloud computing's latest megadeal";
    news.text[59][5] = "Warner Bros. to stream all 2021 movies on HBO Max on same day as theaters";
    news.text[59][6] = "Tesla to be added to S&P 500, sparking epic stock rally";
    news.text[59][7] = "Former Zappos CEO Tony Hsieh died from smoke inhalation in house fire";
    news.text[59][8] = "Elon Musk says he has moved from California to Texas, criticizing Silicon Valley";
    news.text[59][9] = "Black Friday online shopping hits new record as pandemic upends holiday season";

    // Dec 2020
    news.text[60][0] = "Facebook faces antitrust lawsuit from Federal Trade Commission and 48 states";
    news.text[60][1] = "Airbnb IPO pricing at $68 per share, valuing company at $47 billion";
    news.text[60][2] = "AWS outage causes major issues for Roku, Flickr, Philadelphia's court system";
    news.text[60][3] = "Alibaba hit with antitrust probe, sending its shares and chips makers' tumbling";
    news.text[60][4] = "Bitcoin prices hit new high above $25,000 amid rising institutional interest";
    news.text[60][5] = "Kodak loan deal halted by federal court amid allegations of misconduct";
    news.text[60][6] = "SolarWinds faces SEC investigation after massive hack of US government agencies";
    news.text[60][7] = "Spotify unveils 2020 wrapped with Bad Bunny as most-streamed global artist";
    news.text[60][8] = "Apple targets car production by 2024 and eyes 'next level' battery technology";
    news.text[60][9] = "Elon Musk says it's 'impossible' to take Tesla private, denies reports";

    // Jan 2021
    news.text[61][0] = "Tesla's market value tops $800 billion for the first time";
    news.text[61][1] = "Reddit Day Traders wage battle against Wall Street over GameStop stock";
    news.text[61][2] = "Jeff Bezos to step down as Amazon CEO, Andy Jassy to take over";
    news.text[61][3] = "Robinhood app sued for restricting trading amid GameStop frenzy";
    news.text[61][4] = "Bitcoin drops over 10% after hitting record high near $42,000";
    news.text[61][5] = "Intel ousts CEO Bob Swan after seven turbulent quarters";
    news.text[61][6] = "Larry King, legendary TV interviewer, dies at 87";
    news.text[61][7] = "Boeing 737 Max aircraft ungrounded in US after 20-month halt";
    news.text[61][8] = "Hyundai and Boston Dynamics reveal new factory safety robot";
    news.text[61][9] = "Amazon sues e-commerce consultant over insider trading claims";

    // Feb 2021
    news.text[62][0] = "GameStop shares surge over 100% amid buying frenzy";
    news.text[62][1] = "Jeff Bezos announces 'success and revolution' in his final letter as Amazon CEO";
    news.text[62][2] = "Elon Musk loses spot as world's richest person to Jeff Bezos";
    news.text[62][3] = "Ted Cruz takes heat for Cancun trip amid Texas storm crisis";
    news.text[62][4] = "Volkswagen to reboot 'Scout' brand in US with electric trucks and SUVs";
    news.text[62][5] = "Google fires another leading AI ethics researcher amid tensions";
    news.text[62][6] = "Boeing recommits to twin-aisle jets amid airshow drought";
    news.text[62][7] = "Huawei unveils high-end Mate X2 foldable smartphone";
    news.text[62][8] = "Texas sues Griddy, electric utility behind $9,000+ bills amid freeze crisis";
    news.text[62][9] = "Ford recalls 3 million vehicles over air bag defect linked to 6 deaths";

    // Mar 2021
    news.text[63][0] = "Archegos Capital meltdown starts fire sale of $30 billion in stocks";
    news.text[63][1] = "Tesla's market value tops $650 billion amid tech stock selloff";
    news.text[63][2] = "Massive cargo ship blocks Egypt's Suez Canal for nearly a week";
    news.text[63][3] = "Microsoft reportedly in talks to acquire Discord for over $10 billion";
    news.text[63][4] = "Billionaire Mike Novogratz blasts mask 'passport' madness in rant";
    news.text[63][5] = "Nike exec resigns after reports of an undisclosed relationship";
    news.text[63][6] = "Cruise company debt swells to $50+ billion amid Covid-19 catastrophe";
    news.text[63][7] = "Volkswagen says it's not too late to become global EV leader despite Tesla";
    news.text[63][8] = "Amazon workers in Alabama get a do-over on unionization vote";
    news.text[63][9] = "Robinhood confidentially files to go public after GameStop saga";

    // Apr 2021
    news.text[64][0] = "Global chip shortage forces automakers to cut production";
    news.text[64][1] = "Amazon defeats union push at Alabama warehouse in major win";
    news.text[64][2] = "Facebook data on 533 million users reemerges after years underground";
    news.text[64][3] = "Goldman Sachs begins trading digital assets as prices surge";
    news.text[64][4] = "Microsoft wins $22 billion contract making augmented reality headsets for US Army";
    news.text[64][5] = "Coinbase goes public at $86 billion valuation in crypto's biggest listing";
    news.text[64][6] = "Jeff Bezos' Blue Origin launches NASA's New Shepard rocket on a test flight";
    news.text[64][7] = "Google claims big leap forward in blazingly fast computing with new chip";
    news.text[64][8] = "Founders Fund backs $19M Series A for Autonomy's Tesla EV subscription service";
    news.text[64][9] = "SpaceX Crew-2 launches new crew for six-month stay on International Space Station";

    // May 2021
    news.text[65][0] = "Bill Gates and Melinda Gates announce divorce after 27 years of marriage";
    news.text[65][1] = "Elon Musk's SpaceX launches rocket with global internet array";
    news.text[65][2] = "AT&T announces $43 billion deal to merge WarnerMedia with Discovery";
    news.text[65][3] = "Amazon wins legal battle over $10 billion Pentagon cloud computing contract";
    news.text[65][4] = "Tesla no longer accepts Bitcoin for purchases, citing environmental concerns";
    news.text[65][5] = "Colonial Pipeline restarts operations after crippling cyber attack";
    news.text[65][6] = "Amazon to hire 75,000 workers across US amid housing crunch near warehouses";
    news.text[65][7] = "Elon Musk trolls Bitcoin again, meme-inspired dogecoin leaps";
    news.text[65][8] = "Warren Buffett slams Bitcoin again at Berkshire Hathaway meeting";
    news.text[65][9] = "JPMorgan's BlockChain payments platform gets first Singapore bank";

    // Jun 2021
    news.text[66][0] = "El Salvador becomes first country to adopt Bitcoin as legal tender";
    news.text[66][1] = "Microsoft announces Windows 11, first major software upgrade in 6 years";
    news.text[66][2] = "China launches spacecraft in historic mission to Mars";
    news.text[66][3] = "Jeff Bezos' Blue Origin to auction off seat on first human spaceflight";
    news.text[66][4] = "Lordstown Motors execs resign amid disruptions as electric truck launch delayed";
    news.text[66][5] = "Krispy Kreme IPO prices at $17 a share, valuing doughnut chain at $2.7 billion";
    news.text[66][6] = "WeWork starts trading after SPAC merger values it at $9 billion";
    news.text[66][7] = "John McAfee, antivirus software pioneer, found dead in Spanish prison";
    news.text[66][8] = "Supreme Court sides with Facebook in data mining privacy case";
    news.text[66][9] = "Trump's blog closed down a month after launch amid 21st century 'glitch'";

    // Jul 2021
    news.text[67][0] = "Richard Branson beats Jeff Bezos to space in Virgin Galactic flight";
    news.text[67][1] = "Jeff Bezos' Blue Origin completes first crewed space flight";
    news.text[67][2] = "Elon Musk cites data digging effort behind delay in Tesla's India entry";
    news.text[67][3] = "GM recalling Chevy Bolt EVs again for battery fire risks with $1B impact";
    news.text[67][4] = "Robinhood IPO stock prices at $38 per share in $32 billion Nasdaq debut";
    news.text[67][5] = "Google delays return-to-office plan amid surge in COVID cases";
    news.text[67][6] = "China opens cyberspace administration amid tech crackdown";
    news.text[67][7] = "Blue Origin employees blast leaders after Bezos spaceflight";
    news.text[67][8] = "Pfizer in $3.2 billion cash deal to help develop new Sanofi vaccines";
    news.text[67][9] = "Jeff Bezos offers to cover $2 billion in NASA costs in exchange for contract";

    // Aug 2021
    news.text[68][0] = "Facebook rebrands its Workplace software as Meta";
    news.text[68][1] = "Bill Gates admits affairs with employees, as Microsoft investigates";
    news.text[68][2] = "U.S. gives Huawei a temporary reprieve on some licenses";
    news.text[68][3] = "Uber sees revenue in ride-hailing business finally top pre-pandemic levels";
    news.text[68][4] = "YouTubers spark backlash for traveling to Pakistan's 'Hingol' national park";
    news.text[68][5] = "Walmart assembles SpaceX rideshare satellite with focus on rural internet";
    news.text[68][6] = "Amazon to hire 55,000 tech and corporate employees";
    news.text[68][7] = "Intel seeks $10 billion in subsidies for a planned Ohio semiconductor plant";
    news.text[68][8] = "Fed Chair Jerome Powell signals start of tapering bond purchases this year";
    news.text[68][9] = "Computer chip shortage grounds American Airlines flights, exacerbates chip woes";

    // Sep 2021
    news.text[69][0] = "El Salvador's Bitcoin digital wallet 'Chivo' sees bumpy rollout";
    news.text[69][1] = "SpaceX's all-civilian crew returns from three-day orbital mission";
    news.text[69][2] = "Startups race to develop human coronavirus challenge trials";
    news.text[69][3] = "Toyota to slash production by 40% amid chip shortage";
    news.text[69][4] = "Hyundai to launch dedicated EV brand IONIQ in major EV push";
    news.text[69][5] = "Trump in talks to launch media company after Facebook ban upheld";
    news.text[69][6] = "Tesla drivers can now request Full Self-Driving software";
    news.text[69][7] = "Russia antitrust agency fines Google over Play Store fees";
    news.text[69][8] = "Theranos founder Elizabeth Holmes's criminal fraud trial begins";
    news.text[69][9] = "Evergrande, China's beleaguered real estate colossus, inches toward default";

    // Oct 2021
    news.text[70][0] = "Facebook whistleblower testifies about company's algorithms 'tearing our societies apart'";
    news.text[70][1] = "Elon Musk becomes first person on Earth worth over $300 billion";
    news.text[70][2] = "JPMorgan kicks off Bitcoin fund for institutional clients as crypto acceptance grows";
    news.text[70][3] = "Facebook announces plans to hire 10,000 in EU to build its 'metaverse'";
    news.text[70][4] = "Tesla earns $1 billion profit for the first time riding electric vehicle sales";
    news.text[70][5] = "Amazon faces fines, increasing concerns from Black managers over racial inequities";
    news.text[70][6] = "Microsoft agrees to never again use 'abusive, restrictive' tactics after Activision deal";
    news.text[70][7] = "Chinese property giant Evergrande misses $84 million interest payment";
    news.text[70][8] = "Apple unveils AirPods 3 with shorter stems, Spatial Audio and more battery life";
    news.text[70][9] = "Elon Musk's Tesla hits $1 trillion valuation after Hertz's blockbuster EV order";

    // Nov 2021
    news.text[71][0] = "Elon Musk sells $5 billion worth of Tesla shares amid Twitter poll";
    news.text[71][1] = "Rivian, electric truck maker, raises $11.9 billion in blockbuster IPO";
    news.text[71][2] = "Biden signs infrastructure bill directing $1T toward broadband, electric vehicles";
    news.text[71][3] = "Roblox revenue soars as gaming platform draws over 50 million daily users";
    news.text[71][4] = "Musk tweets he's considering spinoff of Tesla's solar energy business";
    news.text[71][5] = "Amazon ripped over 'code name' criticism for Black Lives Matter protest";
    news.text[71][6] = "Americans quit their jobs at a record pace in September amid COVID chaos";
    news.text[71][7] = "Baidu unveils metaverse concept car without a steering wheel";
    news.text[71][8] = "NBCUniversal sells $3.6 billion of ad inventory in cryptic digital currency 'Kreds'";
    news.text[71][9] = "Elizabeth Holmes testifies that Theranos founder was raped on Stanford campus";

    // Dec 2021
    news.text[72][0] = "Elon Musk officially crowned world's richest person after Tesla's value hits $1 trillion";
    news.text[72][1] = "SpaceX could spin off Starlink in several years and take it public, Musk says";
    news.text[72][2] = "Apple lets iPhone users get Covid pass through revised system";
    news.text[72][3] = "Roblox and National Football League to create metaverse experience";
    news.text[72][4] = "Toyota overtakes GM to become top-selling automaker in the US";
    news.text[72][5] = "Apple hits lawsuit over iPhone throttling, agrees to pay users $113M settlement";
    news.text[72][6] = "SEC charges crypto lender BlockFi with outright violation of securities law";
    news.text[72][7] = "Months after sexual assault allegations, Riot paid its CEO $8 million";
    news.text[72][8] = "Jack Dorsey steps down from Twitter as CEO, Parag Agrawal takes over";
    news.text[72][9] = "US lawmakers increase pressure on big tech firms to tackle undercompensation";

    // Jan 2022
    news.text[73][0] = "Microsoft to acquire video game giant Activision Blizzard for $68.7 billion";
    news.text[73][1] = "Airlines cancel thousands of flights amid COVID-19 staffing shortages";
    news.text[73][2] = "NFT sales hit $25 billion in 2021 as craze turns into bubble";
    news.text[73][3] = "Intel to invest $20 billion in new chip factories in Ohio";
    news.text[73][4] = "Peloton's stock crashes 25% after dismal production outlook";
    news.text[73][5] = "AMD agrees to buy Xilinx for $35 billion in a chipmaking megamerger";
    news.text[73][6] = "General Motors launches new $1.3 billion electric vehicle plant in Mexico";
    news.text[73][7] = "U.S. FAA clears 62% of U.S. commercial airplane fleet after 5G deployments";
    news.text[73][8] = "Bitcoin falls by half from November record as crypto selloff worsens";
    news.text[73][9] = "Google loses bid to toss out DOJ's big antitrust case over ad empire";

    // Feb 2022
    news.text[74][0] = "Meta tanks 26% in one day, erasing over $200 billion in market value";
    news.text[74][1] = "US hits Huawei with new criminal charges over alleged trade secret theft";
    news.text[74][2] = "Apple's market cap briefly tops $3 trillion, then slips back down";
    news.text[74][3] = "Amazon raises Prime prices for US members by $20, first hike since 2018";
    news.text[74][4] = "SpaceX launches 49 satellites for its Starlink internet service";
    news.text[74][5] = "Twitter announces sale to Elon Musk for $44 billion";
    news.text[74][6] = "Shell exits Russia in one of biggest sovereign exits yet over Ukraine invasion";
    news.text[74][7] = "GM to sell new Chevy Silverado electric pickup with ultium batteries";
    news.text[74][8] = "Peloton to fire 2,800 employees, replace CEO as demand falls";
    news.text[74][9] = "Saudi Aramco overtakes Apple as world's most valuable company";

    // Mar 2022
    news.text[75][0] = "Musk says Twitter legal battle could head to trial over spam bot accounts";
    news.text[75][1] = "Amazon workers vote to join first U.S. union in historic labor win";
    news.text[75][2] = "Biden administration demands TikTok's Chinese owners sell stakes or face US ban";
    news.text[75][3] = "Netflix shares fall over loss of 200,000 subscribers amid stalled growth";
    news.text[75][4] = "Tesla opens Gigafactory in Berlin amid supply chain headwinds";
    news.text[75][5] = "Intel announces $88 billion Ohio semiconductor factory amid global shortage";
    news.text[75][6] = "Amazon union heads to second election at New York warehouse";
    news.text[75][7] = "Uber drivers across U.S. go on 24-hour strike to protest low pay";
    news.text[75][8] = "Apple supplier Foxconn resumes some operations at China plant amid COVID lockdown";
    news.text[75][9] = "Russia faces deepening economic crisis as sanctions take hold";

    // Apr 2022
    news.text[76][0] = "Musk secures $46.5 billion in funding to buy Twitter, traders cheer";
    news.text[76][1] = "Russia edges closer to historic debt default as payment period lapses";
    news.text[76][2] = "Elon Musk secures $7 billion in funding for his $44 billion Twitter bid";
    news.text[76][3] = "Jeff Bezos blasts Biden in latest tangle over inflation blame";
    news.text[76][4] = "Amazon workers in second NYC facility vote to unionize";
    news.text[76][5] = "Twitter accepts Elon Musk's $44 billion buyout deal";
    news.text[76][6] = "Google's Russian branch plans to file for bankruptcy after ad sales suspended";
    news.text[76][7] = "Boeing loses $1.1 billion on impacts of war, slower 787 production";
    news.text[76][8] = "Starbucks announces it's leaving Russia after 15 years";
    news.text[76][9] = "YouTube tests ultra-expensive ad packages for streaming TV shows";

    // May 2022
    news.text[77][0] = "Cryptocurrency markets in turmoil as stablecoin TerraUSD plunges";
    news.text[77][1] = "Musk says $44 billion Twitter deal 'temporarily on hold'";
    news.text[77][2] = "McDonald's to sell Russian business after over 30 years";
    news.text[77][3] = "YouTube to make billion-dollar investment in India's Bharti Airtel";
    news.text[77][4] = "Redbox agrees to entertainment company rebrand and go public";
    news.text[77][5] = "Elon Musk sued by Twitter investors for delaying disclosure of stake";
    news.text[77][6] = "Microsoft set to become third US tech firm worth over $2 trillion";
    news.text[77][7] = "Instagram tests new age verification methods for youth safety";
    news.text[77][8] = "Subway's $9.9 billion sale to private equity firm clears final hurdle";
    news.text[77][9] = "Grindr goes public via $2.1 billion SPAC deal to expand dating app business";

    // Jun 2022
    news.text[78][0] = "Tesla lays off 10% of salaried workers amid 'super bad' economic outlook";
    news.text[78][1] = "FCC authorizes SpaceX to use its Starlink satellite internet for vehicles";
    news.text[78][2] = "Spirit Airlines rejects JetBlue's $3.6 billion bid, sticks with Frontier deal";
    news.text[78][3] = "Disney+ ad-supported tier coming in late 2022 as streaming losses mount";
    news.text[78][4] = "Mark Zuckerberg's wealth reportedly took a $31 billion hit in 2022";
    news.text[78][5] = "Tesla's office employees ordered to work remotely over security concerns";
    news.text[78][6] = "Google agrees to $90 million settlement in ad-tracking privacy case";
    news.text[78][7] = "Amazon announces plan to overtake UPS and FedEx in package delivery race";
    news.text[78][8] = "Netflix admits it released an 'unprecedented level' of content in 2022";
    news.text[78][9] = "Toyota set to top General Motors as US sales crown goes up for grabs";

    // Jul 2022
    news.text[79][0] = "Elon Musk terminates $44 billion Twitter deal amid bot battle";
    news.text[79][1] = "Biden signs executive order aimed at limiting US investment in China AI, tech";
    news.text[79][2] = "UK imposes 25% windfall tax on oil and gas producers' profits";
    news.text[79][3] = "Twitter sues Elon Musk for terminating $44 billion acquisition deal";
    news.text[79][4] = "Apple employee fired after leading movement against return-to-office rules";
    news.text[79][5] = "Amazon announces plan to acquire primary healthcare provider One Medical for $3.9B";
    news.text[79][6] = "Microsoft reportedly laying off employees across multiple divisions";
    news.text[79][7] = "Elon Musk's SpaceX hit by cyber attack, $240,000 of liquid stolen";
    news.text[79][8] = "ESPN explores sports betting partnerships as industry matures";
    news.text[79][9] = "Netflix confirms it is bringing video games to its platform";

    // Aug 2022
    news.text[80][0] = "Elon Musk sells $6.9 billion of Tesla shares ahead of Twitter legal battle";
    news.text[80][1] = "Southwest Airlines kicks off massive hiring event for 8,000 workers";
    news.text[80][2] = "SoftBank reports $37 billion investment loss, Masayoshi Son remorseful";
    news.text[80][3] = "Bed Bath & Beyond's CFO Gustavo Arnal dies after falling from NYC skyscraper";
    news.text[80][4] = "Amazon expanding grocery footprint with $3.9 billion buyout of One Medical";
    news.text[80][5] = "Disney World got caught photoshopping maskless rider photo during pandemic";
    news.text[80][6] = "Ford hikes price of its electric F-150 Lightning by $8,500 due to supply costs";
    news.text[80][7] = "Zoom lays off 1,300 employees amid sagging demand for video software";
    news.text[80][8] = "Facebook settles lawsuit in revenge porn case, agreeing to bolster practices";
    news.text[80][9] = "Microsoft confirms plans to put Call of Duty games on Steam if Activision deal closes";

    // Sep 2022
    news.text[81][0] = "JPMorgan Chase begins prepping for 'potentially harsh recession' in 2023";
    news.text[81][1] = "Uber investigates 'cybersecurity incident' after breaching involving hacker";
    news.text[81][2] = "Disney CEO Bob Chapek ousted as Bob Iger makes shocking return as chief executive";
    news.text[81][3] = "FedEx warns of a looming global recession, shares plunge 21%";
    news.text[81][4] = "Meta shares tumble 9% as Zuckerberg doubles down on spending for metaverse";
    news.text[81][5] = "Elon Musk revives $44 billion Twitter deal after legal battle";
    news.text[81][6] = "Jeff Bezos donates $100 million to the Obama Foundation leadership program";
    news.text[81][7] = "Porsche takes off in one of the biggest debuts for a sports car brand";
    news.text[81][8] = "Dismal Amazon growth stokes recession fears and shaves $80 billion off value";
    news.text[81][9] = "Peloton slashing 500 more jobs as new CEO pursues restructuring";

    // Oct 2022
    news.text[82][0] = "Elon Musk takes over Twitter after $44 billion acquisition";
    news.text[82][1] = "US stocks tumble after latest Fed rate hike, Microsoft and Alphabet tank";
    news.text[82][2] = "Meta loses $700 billion in market value amid 'Metaverse' spending";
    news.text[82][3] = "Boeing on track to produce at least 31 Dreamliners monthly in 2024";
    news.text[82][4] = "Apple launches its most expensive iPhone lineup with iPhone 14 Pro Max";
    news.text[82][5] = "Jeff Bezos' rocket company Blue Origin suffers first big launch failure";
    news.text[82][6] = "Binance pitches crypto regulation to Biden administration amid industry turmoil";
    news.text[82][7] = "Elon Musk plans to eliminate half of Twitter's workforce after acquisition";
    news.text[82][8] = "Tesla misses Q3 expectations but offers bullish 2023 outlook";
    news.text[82][9] = "Google loses defamation case over 'criminal' search image";

    // Nov 2022
    news.text[83][0] = "Elon Musk cuts half of Twitter workforce amid effort to find profits";
    news.text[83][1] = "Jeff Bezos warns of impending 'economic hurricane,' Biden blames inflation on greed";
    news.text[83][2] = "Southwest Airlines cuts over 20,000 flights as meltdown enters fifth day";
    news.text[83][3] = "Apple threatened to kick Elon Musk's Twitter off App Store amid content turmoil";
    news.text[83][4] = "US senators call for a ban on Google's Ivy AI research amid sentience concerns";
    news.text[83][5] = "Tesla shares fall after Musk tells staff to not be 'bothered by stock market craziness'";
    news.text[83][6] = "Largest NFT mint in history aims to preserve Ukraine's cultural heritage";
    news.text[83][7] = "Elon Musk opens fire at Twitter's ex-employees who were critical of mass layoffs";
    news.text[83][8] = "TikTok accused of promoting pro-eating disorder content to vulnerable teens";
    news.text[83][9] = "Taylor Swift's 'Anti-Hero' smashes Spotify streaming records for viral hit song";

    // Dec 2022
    news.text[84][0] = "Elon Musk has Neuralink put brain chips in pigs and monkeys during company event";
    news.text[84][1] = "Apple faces full-blown Los Angeles labor dispute after workers walked off job";
    news.text[84][2] = "Amazon to spend $1 billion on warehouses in 2022 as it resets operations";
    news.text[84][3] = "Apple missing plan for hybrid work from home actually hurts diversity, say employees";
    news.text[84][4] = "Google settles Arizona legal case over tracking of Android users' locations";
    news.text[84][5] = "FCC bans imports and sales of Huawei and ZTE telecom equipment citing national security";
    news.text[84][6] = "Southwest meltdown leaves harried travelers to fend for themselves over busiest period";
    news.text[84][7] = "Elon Musk and Apple reach agreement amid battles over Twitter moderation";
    news.text[84][8] = "Tesla stock crashes nearly 65% in 2022, trapping option traders in brutal losing trades";
    news.text[84][9] = "Microsoft threatens to restrict data from rival AI search services";

    // Jan 2023
    news.text[85][0] = "Elon Musk faces deposition ahead of upcoming trial over 'funding secured' tweet";
    news.text[85][1] = "Microsoft unveils multibillion-dollar investment in AI computing power with OpenAI";
    news.text[85][2] = "Amazon begins latest round of job cuts affecting 18,000 employees";
    news.text[85][3] = "Google parent Alphabet to lay off 12,000 workers globally as AI race heats up";
    news.text[85][4] = "Foxconn apologizes for technical error that triggered a factory worker protest";
    news.text[85][5] = "Intel hit by shareholder lawsuit over alleged defective chip misinformation";
    news.text[85][6] = "Microsoft invests billions more in ChatGPT maker OpenAI amid AI frenzy";
    news.text[85][7] = "Uber restarts robotaxi service in Las Vegas after two-year pause amid pandemic";
    news.text[85][8] = "FTX founder Sam Bankman-Fried objects to tighter bail, says he's being mistreated";
    news.text[85][9] = "Fully autonomous trucking startup Gatik partners with Isuzu for new mass production deal";

    // Feb 2023
    news.text[86][0] = "Spotify cuts 6% of its workforce as tech layoffs intensify";
    news.text[86][1] = "Elon Musk defiantly defends himself in Tesla tweet trial testimony";
    news.text[86][2] = "Disney to cut 7,000 jobs as streaming losses mount and CEO prioritizes profits";
    news.text[86][3] = "SoFi to cut more than 7% of workforce amid mounting loan losses";
    news.text[86][4] = "Airbnb records over $1.9 billion in revenue, beating estimates in fourth quarter";
    news.text[86][5] = "Electric vehicles outsold gas-powered cars in Norway for the first time in 2022";
    news.text[86][6] = "TikTok CEO grilled by US lawmakers amid potential nationwide ban over security fears";
    news.text[86][7] = "Boeing defense arm to be arraigned in case over ill-fated Air Force One deal";
    news.text[86][8] = "Mercedes aims to capture 15% of upcoming electric truck market with new model";
    news.text[86][9] = "Mark Zuckerberg assures no plans to sell Instagram or WhatsApp after Meta layoffs";

    // Mar 2023
    news.text[87][0] = "Morgan Stanley kicks off Wall Street job cut of over 3,000 employees";
    news.text[87][1] = "Sam Altman's OpenAI launches paid version of ChatGPT with new capabilities";
    news.text[87][2] = "Musk scores minor win in 'funding secured' trial over 2018 Tesla tweets";
    news.text[87][3] = "Netflix launches new double thumbs up rating to improve recommendations";
    news.text[87][4] = "Coinbase to cut 20% of workforce in 'brutal' crypto winter";
    news.text[87][5] = "EU hits Microsoft with fresh antitrust charge amid Activision deal scrutiny";
    news.text[87][6] = "Starbucks enlists former Uber executive to lead company's AI efforts";
    news.text[87][7] = "Internet pioneer Michael Saylor steps down as MicroStrategy's executive chairman";
    news.text[87][8] = "Google cuts cloud computing area in latest round of staff layoffs";
    news.text[87][9] = "Biden, Republicans dig in over debt ceiling with no solution in sight";

    // Apr 2023
    news.text[88][0] = "US economic growth slows sharply to 0.6% as banking turmoil takes toll";
    news.text[88][1] = "YouTube to allow creators to sell ad space for its premium service";
    news.text[88][2] = "General Electric plans to shed health unit for $21 billion in spinoff deal";
    news.text[88][3] = "Emirates inks deal for 40 Boeing Dreamliner jets worth $16 billion";
    news.text[88][4] = "Disney undergoes another round of layoffs, impacting 4,000 employees";
    news.text[88][5] = "TikTok CEO grilled again by lawmakers amid looming nationwide ban over security";
    news.text[88][6] = "First Citizens Bank to acquire Silicon Valley Bank deposits and loans";
    news.text[88][7] = "Impossible Foods partners with McDonald's for test of new plant-based burger";
    news.text[88][8] = "VP Kamala Harris involved in Air Force Two airport incident and probe";
    news.text[88][9] = "Elon Musk celebrates court victory in battle over 2018 Tesla tweets";

    // May 2023
    news.text[89][0] = "Volkswagen to invest $193 billion to overtake Tesla in EV production";
    news.text[89][1] = "Apple suppliers reportedly seeking 25% increase for next iPhone";
    news.text[89][2] = "Adobe wins approval to buy Figma in $20 billion buyout after concessions";
    news.text[89][3] = "T-Mobile to buy Ryan Reynolds' Mint Mobile brand for $1.35 billion";
    news.text[89][4] = "Bankrupt crypto lender Celsius won't return customer crypto deposits";
    news.text[89][5] = "Disney delays several Marvel movies, adds new 'Avengers' and 'Fantastic Four' dates";
    news.text[89][6] = "Twitter board backs keeping Elon Musk's 2020 acquisition deal intact";
    news.text[89][7] = "Trump mugshot released after being booked at Fulton County jail in Georgia";
    news.text[89][8] = "SpaceX achieves key 'launch and catch' milestone for fully reusable rockets";
    news.text[89][9] = "Uber and Google's Waymo announce long-term self-driving partnership";

    // Jun 2023
    news.text[90][0] = "Nvidia stock notches huge rally on booming AI demand, $1 trillion market cap";
    news.text[90][1] = "Microsoft unveils premium version of Teams with AI enhancements from ChatGPT";
    news.text[90][2] = "NASA's Boeing Starliner capsule docks to ISS for first time after years of delays";
    news.text[90][3] = "Amazon to slash another 9,000 jobs across AWS, advertising and Twitch units";
    news.text[90][4] = "Airbnb announces new 'anti-party technology' to spot potential unruly bookings";
    news.text[90][5] = "HBO Max and Discovery+ merging to form new streaming service 'Max'";
    news.text[90][6] = "Nintendo Switch successor reportedly coming in 2024 with 8K support";
    news.text[90][7] = "Intel faces renewed pressure as rival AMD gains ground with new server chips";
    news.text[90][8] = "Elon Musk's SpaceX raising $1.7 billion to build out Starlink global internet";
    news.text[90][9] = "Ford, GM split on when to go all-electric in lineup amid legislative pushes";

    // Jul 2023
    news.text[91][0] = "Microsoft and Samsung join forces on AI and mobile gaming partnership";
    news.text[91][1] = "Twitter sues Elon Musk to enforce $44 billion buyout after he backs out";
    news.text[91][2] = "Tesla reports record revenue and profit but misses delivery expectations";
    news.text[91][3] = "FAA computer system failure grounds flights across US over safety concerns";
    news.text[91][4] = "Meta shares surge on signs metaverse investments may be paying off";
    news.text[91][5] = "Amazon to upend medical supply delivery with $3.9 billion acquisition of Intermountain";
    news.text[91][6] = "Bill Gates says AI like ChatGPT is as revolutionary as the internet";
    news.text[91][7] = "Netflix confirms plans to crackdown on password sharing for ad-supported tier";
    news.text[91][8] = "HBO hit series 'The Last of Us' greenlit for Season 2 after breakout success";
    news.text[91][9] = "Foxconn's India iPhone plant remains shuttered amid worker unrest";

    // Aug 2023
    news.text[92][0] = "US debt ceiling deal reached, averting catastrophic default";
    news.text[92][1] = "Virgin Orbit's final mission ends in failure, company reportedly winds down operations";
    news.text[92][2] = "TikTok could face $29 million fine from UK for failing to protect children's privacy";
    news.text[92][3] = "Intel wins first major cloud customer with historical Microsoft partnership";
    news.text[92][4] = "Tesla's robotaxi service to charge much less than Uber and Lyft";
    news.text[92][5] = "Netflix explores introduction of live streaming for reality shows and stand-up";
    news.text[92][6] = "Twitch CEO says company is 'evaluating' banning gambling streams amid backlash";
    news.text[92][7] = "Peloton shakes up leadership again with new CEO heading subscription push";
    news.text[92][8] = "Samsung teases new foldable phones as it seeks to cement market leadership";
    news.text[92][9] = "DOJ lawsuit aims to block $85 billion Microsoft-Activision Blizzard merger";

    // Sep 2023
    news.text[93][0] = "Federal Reserve hikes interest rates again to combat inflation";
    news.text[93][1] = "Tesla faces scrutiny over self-driving car accidents";
    news.text[93][2] = "Amazon announces major workforce cuts amid economic slowdown";
    news.text[93][3] = "Apple unveils new iPhone models with improved cameras and 5G";
    news.text[93][4] = "Elon Musk's Twitter deal faces regulatory hurdles";
    news.text[93][5] = "Global stock markets tumble on recession fears";
    news.text[93][6] = "Cryptocurrency market rebounds after months of turmoil";
    news.text[93][7] = "Oil prices surge as OPEC+ considers production cuts";
    news.text[93][8] = "Google faces antitrust lawsuits over alleged advertising monopoly";
    news.text[93][9] = "Meta (Facebook) lays off thousands as metaverse investments falter";

    // Oct 2023
    news.text[94][0] = "Inflation remains stubbornly high, raising recession fears";
    news.text[94][1] = "Major tech companies announce more layoffs amid economic downturn";
    news.text[94][2] = "United Auto Workers strike against Ford ends with new contract";
    news.text[94][3] = "Bitcoin nears record highs as institutional investors pile in";
    news.text[94][4] = "Saudi Aramco announces record profits on high oil prices";
    news.text[94][5] = "EU imposes new sanctions on Russia over Ukraine war escalation";
    news.text[94][6] = "US-China tensions rise over Taiwan and trade disputes";
    news.text[94][7] = "Disney's new streaming service faces stiff competition";
    news.text[94][8] = "Uber and Lyft face regulatory challenges over gig worker status";
    news.text[94][9] = "Boeing's troubled 737 MAX jet receives approval for key markets";

    // Nov 2023
    news.text[95][0] = "Black Friday sales surge as consumers hunt for bargains";
    news.text[95][1] = "Cryptocurrencies plummet as major exchange faces liquidity crisis";
    news.text[95][2] = "Retailers brace for supply chain disruptions during holidays";
    news.text[95][3] = "Disney's new Marvel film shatters box office records";
    news.text[95][4] = "Apple faces worker protests over strict COVID policies";
    news.text[95][5] = "Elon Musk's Twitter turmoil continues with advertiser exodus";
    news.text[95][6] = "Energy prices soar as OPEC considers deeper production cuts";
    news.text[95][7] = "US job growth slows amid rising interest rates";
    news.text[95][8] = "Major automakers announce ambitious electric vehicle plans";
    news.text[95][9] = "Amazon faces antitrust scrutiny over third-party seller policies";

    // Dec 2023
    news.text[96][0] = "Federal Reserve raises interest rates again to cool inflation";
    news.text[96][1] = "Tech giants announce more layoffs as recession fears mount";
    news.text[96][2] = "Holiday retail sales disappoint amid economic headwinds";
    news.text[96][3] = "Crypto market ends 2023 on a low note after FTX collapse";
    news.text[96][4] = "Elon Musk's Twitter deal faces renewed scrutiny from regulators";
    news.text[96][5] = "Boeing's 737 MAX troubles persist with new safety concerns";
    news.text[96][6] = "OPEC agrees to further oil production cuts, sending prices higher";
    news.text[96][7] = "China's economic growth slows amid zero-COVID policy challenges";
    news.text[96][8] = "Disney's Avatar sequel shatters box office records";
    news.text[96][9] = "Trade tensions rise as US targets Chinese semiconductor industry";

    // Jan 2024
    news.text[97][0] = "Global stock markets tumble on recession fears";
    news.text[97][1] = "Major tech layoffs continue as companies brace for downturn";
    news.text[97][2] = "Federal Reserve signals more interest rate hikes to come";
    news.text[97][3] = "Tesla misses delivery targets, stoking demand concerns";
    news.text[97][4] = "Boeing halts 737 MAX deliveries over new electrical issue";
    news.text[97][5] = "Crypto market volatility persists after FTX collapse";
    news.text[97][6] = "Disney's streaming subscriber growth slows amid competition";
    news.text[97][7] = "US unemployment rate ticks up as job market cools";
    news.text[97][8] = "Saudi Aramco announces record annual profits on high oil prices";
    news.text[97][9] = "China's reopening boosts economic outlook but risks remain";

    // Feb 2024
    news.text[98][0] = "Inflation remains stubbornly high despite Fed rate hikes";
    news.text[98][1] = "Major banks announce massive job cuts as recession looms";
    news.text[98][2] = "Tesla faces intensifying competition in electric vehicle market";
    news.text[98][3] = "Amazon's holiday quarter disappoints as consumer spending slows";
    news.text[98][4] = "Google hit with billions in fines over antitrust violations in Europe";
    news.text[98][5] = "Oil prices surge as Russia-Ukraine conflict escalates";
    news.text[98][6] = "GameStop stock soars as meme frenzy reignites";
    news.text[98][7] = "Disney's new Star Wars series faces mixed reviews";
    news.text[98][8] = "Boeing's 737 MAX faces renewed grounding threat over wing issue";
    news.text[98][9] = "US-China trade tensions flare up over Taiwan and chip restrictions";

    // Mar 2024
    news.text[99][0] = "Federal Reserve pauses interest rate hikes amid banking turmoil";
    news.text[99][1] = "Silicon Valley Bank collapse sparks fears of broader crisis";
    news.text[99][2] = "Major tech firms announce further layoffs to cut costs";
    news.text[99][3] = "Oil prices whipsaw as OPEC debates supply cuts";
    news.text[99][4] = "Disney's new Marvel film breaks box office records";
    news.text[99][5] = "US imposes new sanctions on Russia over continued Ukraine aggression";
    news.text[99][6] = "Tesla faces growing competition in electric vehicle market";
    news.text[99][7] = "Amazon's labor union battles intensify across warehouses";
    news.text[99][8] = "Cryptocurrency market shows signs of recovery after turbulent year";
    news.text[99][9] = "US-China tensions rise over Taiwan, technology restrictions";
}

static void stocks_init(void) {
    // Data Source: Yahoo Finance scraped with Python script
    ticker.n = 20;
    ticker.top = 0;
        ticker.stocks[0] = (stock_t){
            .name = "Microsoft Corporation", 
        .symbol = "MSFT", 
        .open_price = { 54.41,54.32,54.88,50.97,55.05,50.0,52.44,51.13,56.6,57.01,57.41,59.97,60.11,62.79,64.36,64.13,65.81,68.68,70.24,69.33,73.1,74.71,74.71,83.68,83.6,86.13,94.79,93.99,90.47,93.21,99.28,98.1,106.03,110.85,114.75,107.05,113.0,99.55,103.78,112.89,118.95,130.53,123.85,136.63,137.0,136.61,139.66,144.26,151.81,158.78,170.43,165.31,153.0,175.8,182.54,203.14,211.52,225.51,213.49,204.29,214.51,222.53,235.06,235.9,238.47,253.4,251.23,269.61,286.36,302.87,282.12,331.36,335.13,335.35,310.41,296.4,309.37,277.71,275.2,256.39,277.82,258.87,235.41,234.6,253.87,243.08,248.0,250.76,286.52,306.97,325.93,339.19,335.19,331.31,316.28,339.79,376.76,373.86,401.83,411.27 }, 
        .close_price = { 55.48,55.09,50.88,55.23,49.87,53.0,51.17,56.68,57.46,57.6,59.92,60.26,62.14,64.65,63.98,65.86,68.46,69.84,68.93,72.7,74.77,74.49,83.18,84.17,85.54,95.01,93.77,91.27,93.52,98.84,98.61,106.08,112.33,114.37,106.81,110.89,101.57,104.43,112.03,117.94,130.6,123.68,133.96,136.27,137.86,139.03,143.37,151.38,157.7,170.23,162.01,157.71,179.21,183.25,203.51,205.01,225.53,210.33,202.47,214.07,222.42,231.96,232.38,235.77,252.18,249.68,270.9,284.91,301.88,281.92,331.62,330.59,336.32,310.98,298.79,308.31,277.52,271.87,256.83,280.74,261.47,232.9,232.13,255.14,239.82,247.81,249.42,288.3,307.26,328.39,340.54,335.92,327.76,315.75,338.11,378.91,376.04,397.58,413.64,421.41 }, 
        .high_price = { 56.85,55.39,55.09,55.64,56.77,53.0,52.95,57.29,58.7,58.19,61.37,61.41,64.1,65.91,65.24,66.19,69.14,70.74,72.89,74.42,74.96,75.97,86.2,85.06,87.5,95.45,96.07,97.24,97.9,99.99,102.69,111.15,112.78,115.29,116.18,112.24,113.42,107.9,113.24,120.82,131.37,130.65,138.4,141.68,140.94,142.37,145.67,152.5,159.55,174.05,190.7,175.0,180.4,187.51,204.4,216.38,231.15,232.86,225.21,228.12,227.18,242.64,246.13,241.05,263.19,254.35,271.65,290.15,305.84,305.32,332.0,349.67,344.3,338.0,315.12,315.95,315.11,290.88,277.69,282.0,294.18,267.45,251.04,255.33,263.92,249.83,276.76,289.27,308.93,335.94,351.47,366.78,338.54,340.86,346.2,384.3,378.16,415.32,420.82,427.82 }, 
        .low_price = { 53.68,49.1,48.19,50.58,49.35,49.46,48.04,50.39,56.14,55.61,56.32,57.28,58.8,61.95,62.75,63.62,64.85,67.14,68.09,68.02,71.28,72.92,73.71,82.24,80.7,85.5,83.83,87.08,87.51,92.45,97.26,98.0,104.84,107.23,100.11,99.35,93.96,97.2,102.35,108.8,118.1,123.04,119.01,134.67,130.78,134.51,133.22,142.97,146.65,156.51,152.0,132.52,150.36,173.8,181.35,197.51,203.14,196.25,199.62,200.12,209.11,211.94,227.88,224.26,238.05,238.07,243.0,269.6,283.74,281.62,280.25,326.37,317.25,276.05,271.52,270.0,270.0,246.44,241.51,245.94,260.66,232.73,219.13,213.43,233.87,219.35,245.47,245.61,275.37,303.4,322.5,327.0,311.55,309.45,311.21,339.65,362.9,366.5,397.22,398.39 } 
    }; 

    ticker.stocks[1] = (stock_t){
        .name = "Apple Inc.", 
        .symbol = "AAPL", 
        .open_price = { 29.69,25.65,24.12,24.41,27.19,23.49,24.75,23.87,26.1,26.53,28.18,28.36,27.59,28.95,31.76,34.47,35.93,36.28,38.29,36.22,37.28,41.2,38.56,42.47,42.49,42.54,41.79,44.63,41.66,41.6,47.0,45.96,49.78,57.1,56.99,54.76,46.12,38.72,41.74,43.57,47.91,52.47,43.9,50.79,53.47,51.61,56.27,62.38,66.82,74.06,76.07,70.57,61.62,71.56,79.44,91.28,108.2,132.76,117.64,109.11,121.01,133.52,133.75,123.75,123.66,132.04,125.08,136.6,146.36,152.83,141.9,148.99,167.48,177.83,174.01,164.7,174.03,156.71,149.9,136.04,161.01,156.64,138.21,155.08,148.21,130.28,143.97,146.83,164.27,169.28,177.7,193.78,196.24,189.49,171.22,171.0,190.33,187.15,183.99,179.55 }, 
        .close_price = { 26.32,24.33,24.17,27.25,23.43,24.97,23.9,26.05,26.52,28.26,28.39,27.63,28.95,30.34,34.25,35.92,35.91,38.19,36.01,37.18,41.0,38.53,42.26,42.96,42.31,41.86,44.53,41.94,41.31,46.72,46.28,47.57,56.91,56.44,54.72,44.65,39.44,41.61,43.29,47.49,50.17,43.77,49.48,53.26,52.19,55.99,62.19,66.81,73.41,77.38,68.34,63.57,73.45,79.49,91.2,106.26,129.04,115.81,108.86,119.05,132.69,131.96,121.26,122.15,131.46,124.61,136.96,145.86,151.83,141.5,149.8,165.3,177.57,174.78,165.12,174.61,157.65,148.84,136.72,162.51,157.22,138.2,153.34,148.03,129.93,144.29,147.41,164.9,169.68,177.25,193.97,196.45,187.87,171.21,170.77,189.95,192.53,184.4,180.75,176.08 }, 
        .high_price = { 29.97,26.46,24.72,27.6,28.1,25.18,25.47,26.14,27.56,29.05,29.67,28.44,29.5,30.61,34.37,36.12,36.37,39.16,38.99,38.5,41.13,41.24,42.41,44.06,44.3,45.03,45.15,45.88,44.74,47.59,48.55,48.99,57.22,57.42,58.37,55.59,46.24,42.25,43.97,49.42,52.12,53.83,50.39,55.34,54.51,56.6,62.44,67.0,73.49,81.96,81.81,76.0,73.63,81.06,93.1,106.42,131.0,137.98,125.39,121.99,138.79,145.09,137.88,128.72,137.07,134.07,137.41,150.0,153.49,157.26,153.17,165.7,182.13,182.94,176.65,179.61,178.49,166.48,151.74,163.63,176.15,164.26,157.5,155.45,150.92,147.23,157.38,165.0,169.85,179.35,194.48,198.23,196.73,189.98,182.34,192.93,199.62,196.38,191.05,180.53 }, 
        .low_price = { 26.2,23.1,23.15,24.35,23.13,22.37,22.88,23.59,26.0,25.63,28.07,26.02,27.06,28.69,31.75,34.26,35.01,36.07,35.55,35.6,37.1,37.29,38.12,41.32,41.62,41.17,37.56,41.24,40.16,41.32,45.18,45.85,49.33,53.83,51.52,42.56,36.65,35.5,41.48,42.38,47.1,43.75,42.57,49.6,48.15,51.06,53.78,62.29,64.07,73.19,64.09,53.15,59.22,71.46,79.3,89.14,107.89,103.1,107.72,107.32,120.01,126.38,118.39,116.21,122.49,122.25,123.13,135.76,144.5,141.27,138.27,147.48,157.8,154.7,152.0,150.1,155.38,132.61,129.04,135.66,157.14,138.0,134.37,134.38,125.87,124.17,141.32,143.9,159.78,164.31,176.93,186.6,171.96,167.62,165.67,170.12,187.45,180.17,179.25,168.49 } 
    }; 

    ticker.stocks[2] = (stock_t){
        .name = "NVIDIA Corporation", 
        .symbol = "NVDA", 
        .open_price = { 8.0,8.07,7.32,7.86,8.85,8.97,11.62,11.69,14.34,15.35,17.13,17.85,23.02,26.1,27.65,25.95,27.24,26.18,36.25,36.26,40.53,42.49,45.2,52.34,49.83,48.94,59.63,60.48,57.19,56.14,63.5,58.52,61.53,70.04,71.04,53.08,43.15,32.66,36.12,39.07,45.81,45.78,33.98,43.14,42.28,41.15,43.75,49.9,54.12,59.69,58.92,69.22,63.91,71.09,88.33,95.21,107.32,134.8,137.58,126.58,134.92,131.04,130.53,138.75,135.72,151.25,162.7,201.25,197.0,224.85,207.5,256.49,332.19,298.15,251.04,242.91,273.75,185.41,187.24,148.99,181.82,142.09,123.47,138.11,169.99,148.51,196.91,231.92,275.09,278.4,384.89,425.17,464.6,497.62,440.3,408.84,465.25,492.44,621.0,800.0 }, 
        .close_price = { 8.24,7.32,7.84,8.91,8.88,11.68,11.75,14.27,15.34,17.13,17.79,23.05,26.68,27.3,25.37,27.23,26.08,36.09,36.14,40.63,42.36,44.69,51.7,50.18,48.38,61.45,60.5,57.9,56.22,63.05,59.22,61.22,70.17,70.25,52.71,40.86,33.38,35.94,38.56,44.89,45.25,33.87,41.06,42.18,41.88,43.52,50.26,54.19,58.83,59.11,67.52,65.9,73.07,88.75,94.98,106.15,133.74,135.3,125.34,134.01,130.55,129.9,137.15,133.48,150.1,162.45,200.02,194.99,223.85,207.16,255.67,326.76,294.11,244.86,243.85,272.86,185.47,186.72,151.59,181.63,150.94,121.39,134.97,169.23,146.14,195.37,232.16,277.77,277.49,378.34,423.02,467.29,493.55,434.99,407.8,467.7,495.22,615.27,791.12,893.98 }, 
        .high_price = { 8.48,8.36,8.03,9.06,9.36,11.7,12.14,14.31,15.88,17.3,18.24,23.81,29.98,27.97,30.23,27.5,27.41,36.75,42.12,42.48,43.64,47.8,51.97,54.67,50.08,62.32,62.99,63.62,59.81,65.12,67.3,64.15,70.43,71.31,73.19,55.5,43.67,40.22,41.32,46.25,48.37,46.22,41.34,44.72,43.36,47.1,52.22,55.35,60.45,64.88,79.08,71.22,76.05,91.82,96.43,107.92,135.75,147.27,143.49,146.91,137.31,139.99,153.73,139.25,162.14,162.77,201.62,208.75,230.43,229.86,257.09,346.47,332.89,307.11,269.25,289.46,275.58,204.0,196.19,182.44,192.74,145.47,138.5,169.98,187.9,206.28,238.88,278.34,281.1,419.38,439.9,480.88,502.66,498.0,476.09,505.48,504.33,634.93,823.94,974.0 }, 
        .low_price = { 7.99,6.61,6.19,7.76,8.66,8.6,11.14,11.51,13.88,14.33,15.93,16.65,21.19,24.78,23.92,23.79,23.87,25.58,35.53,34.65,38.23,40.68,44.25,47.81,45.15,48.62,51.0,54.25,52.58,55.55,58.75,58.31,59.68,64.67,44.0,33.33,31.11,31.92,35.65,36.2,43.33,33.85,33.15,38.75,36.85,40.81,42.53,49.65,50.09,57.82,58.86,45.17,59.6,70.21,86.58,94.13,107.15,117.04,123.0,123.95,127.63,125.86,129.03,115.67,135.11,134.59,159.03,178.65,187.62,204.67,195.55,252.27,271.45,208.88,208.9,206.5,182.9,155.67,148.62,140.55,149.59,119.46,108.13,129.56,138.84,140.34,196.11,222.97,262.2,272.4,373.56,413.46,403.11,409.8,392.3,408.69,450.1,473.2,616.5,794.35 } 
    }; 

    ticker.stocks[3] = (stock_t){
        .name = "Amazon.com, Inc.", 
        .symbol = "AMZN", 
        .open_price = { 33.69,32.81,28.91,27.81,29.52,33.2,36.04,35.87,37.99,38.54,41.8,39.95,37.62,37.9,41.46,42.65,44.4,46.39,49.93,48.64,49.81,49.21,48.2,55.27,58.6,58.6,72.25,75.68,70.88,78.16,81.85,84.14,89.2,101.32,101.1,81.18,88.47,73.26,81.94,82.76,90.01,96.65,88.0,96.15,93.59,88.5,87.3,89.4,90.22,93.75,100.53,95.32,96.65,116.84,122.4,137.9,159.03,174.48,160.4,153.09,159.43,163.5,162.12,156.39,155.9,174.24,162.18,171.73,167.65,174.82,164.45,168.09,177.25,167.55,150.0,152.73,164.15,122.4,122.26,106.29,134.96,126.0,113.58,103.99,96.99,85.46,102.53,93.87,102.3,104.95,120.69,130.82,133.55,139.46,127.28,133.96,146.0,151.54,155.87,176.75 }, 
        .close_price = { 33.79,29.35,27.63,29.68,32.98,36.14,35.78,37.94,38.46,41.87,39.49,37.53,37.49,41.17,42.25,44.33,46.25,49.73,48.4,49.39,49.03,48.07,55.26,58.84,58.47,72.54,75.62,72.37,78.31,81.48,84.99,88.87,100.64,100.15,79.9,84.51,75.1,85.94,81.99,89.04,96.33,88.75,94.68,93.34,88.81,86.8,88.83,90.04,92.39,100.44,94.19,97.49,123.7,122.12,137.94,158.23,172.55,157.44,151.81,158.4,162.85,160.31,154.65,154.7,173.37,161.15,172.01,166.38,173.54,164.25,168.62,175.35,166.72,149.57,153.56,163.0,124.28,120.21,106.21,134.95,126.77,113.0,102.44,96.54,84.0,103.13,94.23,103.29,105.45,120.58,130.36,133.68,138.01,127.12,133.09,146.09,151.94,155.2,176.76,175.9 }, 
        .high_price = { 34.82,32.89,29.09,30.16,33.5,36.21,36.58,38.3,38.75,42.0,42.36,40.04,39.12,42.19,43.04,44.52,47.48,50.06,50.85,54.17,50.32,50.0,56.14,60.67,59.74,73.63,76.43,80.88,81.9,81.75,88.15,94.0,101.28,102.53,101.66,89.2,88.92,86.82,83.65,91.19,97.82,98.22,96.76,101.79,94.9,92.68,89.94,91.23,95.07,102.79,109.3,99.82,123.75,126.27,139.8,167.21,174.75,177.61,174.81,168.34,167.53,168.19,171.7,159.1,177.7,174.33,176.24,188.65,173.63,177.5,173.95,188.11,177.99,171.4,163.83,170.83,168.39,126.22,128.99,137.65,146.57,136.49,123.0,104.58,97.23,103.49,114.0,103.49,110.86,122.92,131.49,136.65,143.63,145.86,134.48,149.26,155.63,161.73,177.22,180.14 }, 
        .low_price = { 31.76,27.36,23.7,26.93,29.26,32.8,34.11,35.83,37.52,37.8,38.73,35.51,36.83,37.38,40.15,41.67,44.22,46.39,46.35,47.55,46.82,46.59,47.52,54.34,56.24,58.53,63.3,68.26,67.64,77.3,81.75,83.9,88.8,93.25,73.82,71.0,65.35,73.05,78.34,79.33,89.94,88.64,83.6,92.47,87.18,85.46,84.25,86.14,86.75,90.77,90.56,81.3,94.46,112.82,121.86,137.7,153.65,143.55,150.95,147.51,153.64,154.3,151.84,144.05,155.78,156.37,158.61,165.35,158.79,163.7,158.81,164.18,165.2,135.35,138.33,133.57,121.62,101.26,101.43,105.85,126.74,112.06,97.66,85.87,81.69,81.43,92.32,88.12,97.71,101.15,119.93,125.92,126.41,123.04,118.35,133.71,142.81,144.05,155.62,171.47 } 
    }; 

    ticker.stocks[4] = (stock_t){
        .name = "Meta Platforms, Inc.", 
        .symbol = "META", 
        .open_price = { 104.83,101.95,112.27,107.83,113.75,117.83,118.5,114.2,123.85,126.38,128.38,131.41,118.38,116.03,132.25,136.47,141.93,151.74,151.75,151.72,169.82,172.4,171.39,182.36,176.03,177.68,188.22,179.01,157.81,172.0,193.07,193.37,173.93,173.5,163.03,151.52,143.0,128.99,165.84,162.6,167.83,194.78,175.0,195.21,194.17,184.0,179.15,192.85,202.13,206.75,203.44,194.03,161.62,201.6,224.59,228.5,252.65,294.71,265.35,264.6,279.16,274.78,259.52,260.82,298.4,326.17,330.15,346.82,358.1,379.59,341.61,326.04,330.29,338.3,314.56,209.87,224.55,201.17,196.51,160.31,157.25,163.58,137.14,94.33,119.2,122.82,148.03,174.59,208.84,238.62,265.9,286.7,317.54,299.37,302.74,301.85,325.48,351.32,393.94,492.11 }, 
        .close_price = { 104.66,112.21,106.92,114.1,117.58,118.81,114.28,123.94,126.12,128.27,130.99,118.42,115.05,130.32,135.54,142.05,150.25,151.46,150.98,169.25,171.97,170.87,180.06,177.18,176.46,186.89,178.32,159.79,172.0,191.78,194.32,172.58,175.73,164.46,151.79,140.61,131.09,166.69,161.45,166.69,193.4,177.47,193.0,194.23,185.67,178.08,191.65,201.64,205.25,201.91,192.47,166.8,204.71,225.09,227.07,253.67,293.2,261.9,263.11,276.97,273.16,258.33,257.62,294.53,325.08,328.73,347.71,356.3,379.38,339.39,323.57,324.46,336.35,313.26,211.03,222.36,200.47,193.64,161.25,159.1,162.93,135.68,93.16,118.1,120.34,148.97,174.94,211.94,240.32,264.72,286.98,318.6,295.89,300.21,301.27,327.15,353.96,390.14,490.13,496.24 }, 
        .high_price = { 107.92,112.84,117.59,116.99,120.79,121.08,119.44,128.33,126.73,131.98,133.5,131.94,122.5,133.14,137.18,142.95,151.53,153.6,156.5,175.49,173.05,174.0,180.8,184.25,182.28,190.66,195.32,186.1,177.1,192.72,203.55,218.62,188.3,173.89,165.88,154.13,147.19,171.68,172.47,174.3,198.48,196.18,198.88,208.66,198.47,193.1,198.09,203.8,208.93,224.2,218.77,197.24,209.69,240.9,245.19,255.85,304.67,303.6,285.24,297.38,291.78,286.79,276.6,299.71,331.81,333.78,358.14,377.55,382.76,384.33,345.02,353.83,352.71,343.09,328.0,231.15,236.86,224.3,202.03,183.85,183.1,171.39,142.39,118.74,124.67,153.19,197.16,212.17,241.69,268.65,289.79,326.2,324.14,312.87,330.54,342.92,361.9,406.36,494.36,523.57 }, 
        .low_price = { 101.46,89.37,96.82,104.4,106.31,115.88,108.23,112.97,122.07,125.6,126.75,113.55,114.0,115.51,130.3,136.08,138.81,144.42,144.56,147.8,165.0,161.56,168.29,174.0,169.01,175.8,167.18,149.02,150.51,170.23,186.43,166.56,170.27,158.87,139.03,126.85,123.02,128.56,159.59,159.28,167.28,177.16,160.84,191.93,176.66,175.66,173.09,188.54,193.17,201.06,181.82,137.1,150.83,198.76,207.11,226.9,247.43,244.13,254.82,257.34,264.63,244.61,254.04,253.5,296.04,298.19,323.48,334.5,347.7,338.15,308.11,323.2,299.5,289.01,190.22,185.82,169.0,176.11,154.25,154.85,155.23,134.12,92.6,88.09,112.46,122.28,147.06,171.43,207.13,229.85,258.88,284.85,274.38,286.79,279.4,301.85,313.66,340.01,393.05,476.0 } 
    }; 

    ticker.stocks[5] = (stock_t){
        .name = "Alphabet Inc.", 
        .symbol = "GOOGL", 
        .open_price = { 38.35,38.11,38.56,36.06,37.86,35.6,37.42,35.26,39.33,39.6,40.13,40.54,38.93,40.03,41.2,42.57,42.44,46.21,49.55,46.66,47.39,47.87,48.78,51.82,51.52,52.65,58.8,55.48,51.38,50.81,55.64,55.77,61.96,61.13,60.65,54.57,56.61,51.36,56.11,56.55,59.38,59.88,53.35,55.05,60.88,59.09,61.12,63.29,65.13,67.42,73.08,67.57,56.2,66.2,71.29,70.96,74.55,81.61,74.18,81.18,88.33,88.0,92.23,102.4,104.61,118.25,118.72,121.72,135.12,145.0,134.45,148.05,144.0,145.05,137.59,134.88,139.5,113.4,114.86,107.93,115.3,108.28,96.76,95.45,101.02,89.59,98.71,89.98,102.39,106.84,122.82,119.24,130.78,137.46,131.21,124.07,131.86,138.55,142.12,138.43 }, 
        .close_price = { 38.9,38.07,35.86,38.15,35.39,37.44,35.18,39.57,39.49,40.2,40.49,38.79,39.62,41.01,42.25,42.39,46.23,49.35,46.48,47.28,47.76,48.69,51.65,51.81,52.67,59.11,55.2,51.86,50.93,55.0,56.46,61.36,61.59,60.35,54.53,55.48,52.25,56.29,56.33,58.84,59.95,55.33,54.14,60.91,59.53,61.06,62.94,65.2,66.97,71.64,66.96,58.1,67.33,71.68,70.9,74.4,81.48,73.28,80.81,87.72,87.63,91.37,101.1,103.13,117.68,117.84,122.09,134.73,144.7,133.68,148.05,141.9,144.85,135.3,135.06,139.07,114.11,113.76,108.96,116.32,108.22,95.65,94.51,100.99,88.23,98.84,90.06,103.73,107.34,122.87,119.7,132.72,136.17,130.86,124.08,132.53,139.69,140.1,138.46,147.03 }, 
        .high_price = { 39.93,38.46,40.52,38.87,39.55,37.67,37.57,40.2,40.69,40.95,41.95,40.8,41.22,43.35,42.69,43.72,46.79,49.98,50.43,50.31,47.86,48.79,53.18,54.0,54.32,59.9,59.37,58.91,54.88,55.91,60.07,64.57,63.6,61.39,61.23,55.5,56.75,56.38,57.7,61.82,64.85,59.96,56.33,63.42,61.81,62.4,64.96,66.7,68.35,75.03,76.54,70.41,68.01,72.26,73.79,79.35,82.64,86.31,84.07,90.84,92.19,96.6,107.26,105.69,121.57,119.45,123.1,138.3,145.97,146.25,148.65,150.97,149.1,146.49,151.55,143.79,143.71,122.85,119.35,119.68,122.43,111.62,104.82,101.04,102.25,100.32,108.18,106.59,109.17,126.43,129.04,133.74,138.0,139.16,141.22,139.42,142.68,153.78,149.44,152.15 }, 
        .low_price = { 36.81,34.39,34.1,35.2,35.16,35.2,33.63,34.95,39.25,39.17,39.81,37.18,37.67,39.84,40.6,41.22,41.73,46.04,46.48,45.77,45.93,46.23,48.1,51.43,50.12,52.65,49.85,49.2,49.71,50.39,55.3,55.33,60.2,57.58,50.36,50.11,48.88,51.12,54.68,56.51,59.15,55.17,51.35,54.77,57.14,58.19,58.16,62.99,63.85,67.32,63.41,50.44,53.75,64.8,67.58,70.71,73.2,70.11,71.66,80.61,84.7,84.81,92.23,99.7,104.57,109.68,116.48,121.53,133.32,133.56,131.05,141.6,139.32,124.5,124.95,125.28,112.74,101.88,105.05,104.07,107.8,95.56,91.8,83.34,85.94,84.86,88.58,89.42,101.93,103.71,116.1,115.35,126.38,127.22,120.21,123.72,127.9,135.15,135.41,130.67 } 
    }; 

    ticker.stocks[6] = (stock_t){
        .name = "Berkshire Hathaway Inc.", 
        .symbol = "BRK-B", 
        .open_price = { 134.86,130.16,128.94,135.11,141.21,145.77,140.97,144.59,144.62,150.7,144.27,144.68,157.58,164.34,164.75,173.7,166.72,165.8,165.8,170.4,175.93,181.6,183.45,188.1,193.59,198.87,214.49,206.82,199.01,193.76,192.9,186.09,198.8,209.21,215.92,205.6,221.98,201.73,206.52,203.15,202.16,217.22,197.62,214.25,205.63,201.19,208.94,213.55,220.6,227.51,225.48,207.25,176.18,185.21,185.53,178.41,197.28,216.92,214.3,204.84,230.23,231.73,229.97,246.86,255.74,278.55,291.52,278.2,279.31,286.59,273.02,288.05,279.54,300.1,312.64,320.26,353.65,324.11,316.0,272.5,299.7,279.95,269.52,298.45,319.0,310.07,309.63,304.02,309.25,329.16,321.42,340.75,352.03,362.0,349.64,341.21,359.94,356.32,384.0,409.48 }, 
        .close_price = { 132.04,129.77,134.17,141.88,145.48,140.54,144.79,144.27,150.49,144.47,144.3,157.44,162.98,164.14,171.42,166.68,165.21,165.28,169.37,174.97,181.16,183.32,186.94,193.01,198.22,214.38,207.2,199.48,193.73,191.53,186.65,197.87,208.72,214.11,205.28,218.24,204.18,205.54,201.3,200.89,216.71,197.42,213.17,205.43,203.41,208.02,212.58,220.3,226.5,224.43,206.34,182.83,187.36,185.58,178.51,195.78,218.04,212.94,201.9,228.91,231.87,227.87,240.51,255.47,274.95,289.44,277.92,278.29,285.77,272.94,287.01,276.69,299.0,313.02,321.45,352.91,322.83,315.98,273.02,300.6,280.8,267.02,295.09,318.6,308.9,311.52,305.18,308.77,328.55,321.08,341.0,351.96,360.2,350.3,341.33,360.0,356.66,383.74,409.4,411.76 }, 
        .high_price = { 136.74,131.76,135.11,143.4,148.03,147.14,146.0,146.99,150.9,151.05,145.71,159.09,167.25,165.3,172.2,177.86,168.95,168.04,171.95,175.6,181.87,184.0,190.68,193.81,200.5,217.62,217.5,213.36,202.77,202.41,196.74,201.4,211.32,223.0,224.07,223.52,223.59,208.01,209.4,207.75,217.32,219.16,213.33,216.58,206.98,214.58,213.71,223.37,228.23,231.61,230.08,218.8,197.23,187.28,203.33,196.67,219.45,223.24,217.43,234.99,232.28,236.24,250.56,267.5,277.79,295.08,293.27,282.22,291.82,287.14,292.22,295.65,301.65,324.4,325.63,362.1,354.58,327.28,316.79,302.4,308.15,289.24,299.98,319.12,319.56,321.32,314.15,317.29,328.81,333.94,342.5,352.33,364.63,373.34,350.0,363.19,364.05,387.92,430.0,412.19 }, 
        .low_price = { 129.53,123.9,123.55,134.32,140.27,139.68,136.65,140.95,142.92,143.36,141.92,142.35,157.51,158.61,162.15,165.8,162.28,160.93,164.75,168.0,174.81,172.61,182.93,180.44,189.72,195.96,189.3,191.87,192.02,188.62,184.75,185.72,196.77,208.04,197.29,203.39,186.1,191.04,198.21,197.03,202.0,197.08,196.89,204.36,195.4,200.19,201.33,213.4,216.4,221.11,199.68,159.5,174.19,167.04,174.63,177.34,196.0,206.58,197.81,203.06,221.26,226.1,228.04,243.23,254.8,276.78,272.46,270.73,276.77,271.36,272.22,275.89,274.79,294.81,299.51,313.59,320.5,298.11,263.68,271.2,280.44,261.55,259.85,282.38,297.0,303.86,300.01,292.42,307.07,317.41,319.53,338.41,349.39,348.55,330.58,340.58,350.85,355.94,381.48,398.78 } 
    }; 

    ticker.stocks[7] = (stock_t){
        .name = "Eli Lilly and Company", 
        .symbol = "LLY", 
        .open_price = { 84.21,83.4,78.21,72.63,71.6,75.95,74.9,78.89,83.02,77.75,80.0,73.81,67.29,73.94,77.89,83.44,84.13,82.09,79.6,82.41,82.86,81.54,85.78,82.17,84.98,84.46,81.54,77.06,76.88,80.68,85.2,85.06,98.74,105.31,107.67,108.74,118.64,114.79,120.32,127.25,130.71,117.05,116.53,111.31,109.02,112.54,111.94,114.07,117.52,131.77,140.53,127.74,134.0,153.72,154.47,164.32,152.8,148.61,148.33,132.54,146.69,169.02,209.46,205.78,186.82,182.99,200.32,229.51,245.73,258.54,231.0,255.11,249.44,274.41,247.05,247.56,286.15,291.23,313.44,323.88,327.51,301.0,325.99,345.83,374.79,366.26,342.79,310.0,343.24,397.26,430.27,466.26,455.35,556.32,536.01,555.0,591.7,580.41,647.33,769.02 }, 
        .close_price = { 84.26,79.1,72.0,72.01,75.53,75.03,78.75,82.89,77.75,80.26,73.84,67.12,73.55,77.03,82.81,84.11,82.06,79.57,82.3,82.66,81.29,85.54,81.94,84.64,84.46,81.45,77.02,77.37,81.07,85.04,85.33,98.81,105.65,107.31,108.44,118.64,115.72,119.86,126.29,129.76,117.04,115.94,110.79,108.95,112.97,111.83,113.95,117.35,131.43,139.64,126.13,138.72,154.64,152.95,164.18,150.29,148.39,148.02,130.46,145.65,168.84,207.97,204.89,186.82,182.77,199.74,229.52,243.5,258.29,231.05,254.76,248.04,276.22,245.39,249.95,286.37,292.13,313.44,324.23,329.69,301.23,323.35,362.09,371.08,365.84,344.15,311.22,343.42,395.86,429.46,468.98,454.55,554.2,537.13,553.93,591.04,582.92,645.61,753.68,772.78 }, 
        .high_price = { 88.16,85.4,79.16,74.95,78.5,78.71,78.81,83.59,83.79,81.46,83.24,79.7,74.49,78.12,83.48,86.14,86.72,83.11,84.76,85.53,83.15,85.61,89.09,85.48,89.09,88.33,83.03,80.49,83.56,85.11,87.27,99.2,106.49,107.84,116.61,118.71,119.84,120.14,127.77,132.13,131.35,119.53,118.94,115.47,116.14,117.23,114.67,118.46,137.0,143.72,147.87,144.0,164.9,162.37,167.43,170.75,157.49,154.5,157.15,151.98,173.9,218.0,209.89,212.16,193.5,203.62,239.37,248.4,275.87,260.99,256.75,271.11,283.9,274.41,252.9,295.33,314.0,324.08,330.85,335.33,330.43,341.7,363.92,372.35,375.25,369.0,353.82,343.65,404.31,454.95,469.87,467.6,557.75,601.84,629.97,625.87,601.97,663.55,794.47,800.78 }, 
        .low_price = { 82.56,76.54,70.43,67.88,71.53,73.34,71.87,78.37,77.53,76.45,73.8,64.18,65.66,73.54,76.55,81.37,79.88,76.85,78.75,80.77,76.89,79.05,81.72,81.42,84.18,81.27,73.69,74.51,75.4,77.09,84.36,84.71,97.84,103.66,104.17,104.95,105.67,111.1,116.86,121.96,113.65,113.87,110.02,105.15,107.44,106.4,101.36,110.51,115.92,130.23,121.51,117.06,132.98,143.62,139.68,148.74,146.98,145.06,129.21,129.99,143.13,161.78,195.64,179.82,178.58,182.92,196.68,228.66,243.5,220.2,224.22,246.48,239.47,232.68,231.87,245.44,276.83,283.0,283.11,315.51,296.53,296.32,317.05,340.12,354.61,339.38,309.64,309.2,342.3,392.26,428.13,434.34,446.89,532.2,516.57,551.26,561.65,579.05,643.17,727.62 } 
    }; 

    ticker.stocks[8] = (stock_t){
        .name = "Broadcom Inc.", 
        .symbol = "AVGO", 
        .open_price = { 131.17,142.07,133.34,135.37,154.04,146.16,153.41,154.47,162.78,176.88,173.01,170.72,170.01,178.29,202.25,213.07,219.0,222.09,241.6,234.51,248.23,252.33,245.31,264.24,275.21,259.77,241.72,245.86,233.95,230.85,254.37,240.09,223.26,218.59,247.88,225.16,245.2,248.85,269.15,277.73,303.11,320.5,252.55,301.68,289.13,280.0,278.27,292.99,317.72,319.32,305.63,276.76,227.99,266.0,290.42,315.11,318.0,350.0,368.98,355.04,403.46,439.33,455.85,479.7,472.07,459.75,475.44,477.84,489.05,496.27,487.85,530.34,563.48,666.32,585.87,584.79,631.69,557.14,587.58,479.41,531.43,491.47,449.24,475.8,551.03,565.0,583.57,594.0,639.0,626.5,800.62,868.62,898.98,901.87,829.06,842.0,922.46,1092.12,1187.35,1325.93 }, 
        .close_price = { 145.15,133.71,133.97,154.5,145.75,154.36,155.4,161.98,176.42,172.52,170.28,170.49,176.77,199.5,210.93,218.96,220.81,239.48,233.05,246.66,252.07,242.54,263.91,277.94,256.9,248.03,246.46,235.65,229.42,252.07,242.64,221.77,219.03,246.73,223.49,237.41,254.28,268.25,275.36,300.71,318.4,251.64,287.86,289.99,282.64,276.07,292.85,316.21,316.02,305.16,272.62,237.1,271.62,291.27,315.61,316.75,347.15,364.32,349.63,401.58,437.85,450.5,469.87,463.66,456.2,472.33,476.84,485.4,497.21,484.93,531.67,553.68,665.41,585.88,587.44,629.68,554.39,580.13,485.81,535.48,499.11,444.01,470.12,551.03,559.13,585.01,594.29,641.54,626.5,807.96,867.43,898.65,922.89,830.58,841.37,925.73,1116.25,1180.0,1300.49,1238.01 }, 
        .high_price = { 149.72,143.3,138.69,157.37,159.65,154.95,166.0,167.6,179.42,177.67,177.0,178.02,183.99,205.79,215.96,227.75,224.09,242.89,256.78,258.49,259.36,255.34,266.7,285.68,275.7,274.26,255.74,273.85,252.85,255.29,271.81,251.8,223.94,250.1,252.14,242.62,261.59,273.75,286.63,303.3,322.45,323.2,291.75,305.75,297.82,302.33,294.08,325.67,331.2,331.58,325.7,288.48,276.99,292.0,328.11,324.33,350.58,378.96,387.8,402.16,438.5,470.0,495.14,490.86,489.64,474.62,478.59,494.02,507.85,510.7,536.07,577.21,677.76,672.19,614.64,645.31,636.72,609.0,590.94,537.83,560.56,531.26,489.7,551.66,585.65,601.67,617.01,648.5,644.24,921.78,889.95,923.18,923.67,901.87,925.91,999.87,1151.82,1284.55,1319.62,1438.17 }, 
        .low_price = { 130.91,117.17,114.25,134.3,143.25,139.18,142.27,147.16,161.28,158.75,166.8,163.02,160.62,173.31,200.21,210.56,208.44,219.91,230.57,227.5,238.7,231.53,237.51,248.87,253.39,237.01,224.9,234.76,225.44,221.98,242.27,197.46,202.77,213.92,208.23,213.71,217.61,230.33,264.89,259.2,299.75,250.09,251.2,272.41,262.5,270.0,267.22,292.09,303.13,298.8,262.74,155.67,219.68,254.75,287.37,304.18,317.28,343.48,344.42,346.66,398.1,420.54,453.59,419.26,449.0,419.14,458.44,455.71,462.66,484.48,472.78,524.93,544.0,513.4,549.02,563.61,553.4,512.44,480.71,463.91,496.54,443.64,415.07,441.36,516.05,549.99,572.1,586.13,603.23,601.29,776.38,844.33,812.0,795.09,808.91,835.57,903.1,1041.51,1179.11,1204.02 } 
    }; 

    ticker.stocks[9] = (stock_t){
        .name = "JPMorgan Chase & Co.", 
        .symbol = "JPM", 
        .open_price = { 67.34,63.95,59.16,56.76,59.02,63.69,64.76,61.66,64.15,67.64,66.35,69.48,80.65,87.34,85.54,92.79,87.99,87.36,82.46,91.56,92.49,91.25,95.77,101.1,104.9,107.63,115.77,115.48,109.96,108.45,108.34,103.72,115.75,114.34,113.37,109.62,112.38,95.95,104.0,105.1,102.15,115.72,105.8,113.23,115.33,108.98,118.4,126.2,132.31,139.79,132.66,116.63,85.1,93.5,97.75,94.89,97.02,99.55,97.12,99.39,120.34,127.5,129.4,149.52,151.9,154.85,165.87,156.26,152.03,160.22,164.0,172.04,161.0,159.86,148.69,140.04,137.4,119.88,132.87,112.65,114.5,113.29,105.62,126.87,138.18,135.24,138.21,142.1,129.91,142.26,136.52,146.19,157.43,146.09,144.83,139.25,155.82,169.09,173.64,185.7 }, 
        .close_price = { 66.03,59.5,56.3,59.22,63.2,65.27,62.14,63.97,67.5,66.59,69.26,80.17,86.29,84.63,90.62,87.84,87.0,82.15,91.4,91.8,90.89,95.51,100.61,104.52,106.94,115.67,115.5,109.97,108.78,107.01,104.2,114.95,114.58,112.84,109.02,111.19,97.62,103.5,104.36,101.23,116.05,105.96,111.8,116.0,109.86,117.69,124.92,131.76,139.4,132.36,116.11,90.03,95.76,97.31,94.06,96.64,100.19,96.27,98.04,117.88,127.07,128.67,147.17,152.23,153.81,164.24,155.54,151.78,159.95,163.69,169.89,158.83,158.35,148.6,141.8,136.32,119.36,132.23,112.61,115.36,113.73,104.5,125.88,138.18,134.1,139.96,143.35,130.31,138.24,135.71,145.44,157.96,146.33,145.02,139.06,156.08,170.1,174.36,186.06,193.79 }, 
        .high_price = { 68.0,64.13,59.65,60.97,64.66,66.2,65.92,64.98,67.77,67.9,69.77,80.53,87.39,88.17,91.34,93.98,89.13,88.09,92.65,94.51,95.22,95.88,102.42,106.66,108.46,117.35,119.33,118.75,115.15,114.73,111.91,117.61,118.29,119.24,116.81,112.93,112.89,105.24,107.27,108.4,117.16,117.0,112.43,117.24,116.8,120.4,127.42,132.43,140.08,141.1,139.29,122.95,104.39,102.95,115.77,101.29,106.43,105.21,104.45,123.5,127.33,142.75,154.9,161.69,157.25,165.7,167.44,159.16,163.83,169.3,172.96,172.33,163.39,169.81,159.03,143.93,137.41,133.15,132.87,116.5,124.24,121.55,127.43,138.18,138.66,143.49,144.34,144.04,141.78,143.37,146.0,159.38,158.0,150.25,153.11,156.13,170.69,178.3,186.43,193.93 }, 
        .low_price = { 63.51,54.66,52.5,56.67,57.07,60.59,57.05,58.76,63.38,65.11,66.1,67.64,80.65,83.03,84.16,85.23,84.36,81.64,81.65,90.32,90.16,88.08,94.96,95.95,102.2,106.81,103.98,106.65,106.08,104.96,103.11,102.2,112.97,112.52,102.73,105.98,91.11,95.94,100.06,98.09,102.12,104.84,105.3,112.15,104.34,107.32,110.52,126.02,128.59,129.71,112.66,76.91,82.77,82.4,92.0,90.78,95.03,91.38,95.09,97.86,118.11,123.77,128.48,147.97,146.69,152.14,147.56,145.71,149.52,150.49,160.06,158.29,151.84,139.57,139.78,127.27,118.9,115.02,110.93,106.06,111.02,104.4,101.28,125.91,128.41,133.55,137.44,123.11,126.22,131.81,135.45,141.44,145.46,142.65,135.19,138.47,155.82,164.3,171.43,184.27 } 
    }; 

    ticker.stocks[10] = (stock_t){
        .name = "Tesla, Inc.", 
        .symbol = "TSLA", 
        .open_price = { 15.4,15.38,12.58,12.95,16.32,16.1,14.77,13.74,15.7,13.93,14.15,13.2,12.55,14.32,16.87,16.95,19.13,20.99,22.93,24.68,21.53,23.74,22.83,22.15,20.36,20.8,23.4,23.0,17.08,19.57,19.06,24.0,19.87,19.8,20.38,22.55,24.0,20.41,20.36,20.46,18.84,15.92,12.37,15.35,16.18,14.94,16.1,21.09,21.96,28.3,44.91,47.42,33.6,50.33,57.2,72.2,96.61,167.38,146.92,131.33,199.2,239.82,271.43,230.04,229.46,234.6,209.27,227.97,233.33,244.69,259.47,381.67,386.9,382.58,311.74,289.89,360.38,286.92,251.72,227.0,301.28,272.58,254.5,234.05,197.08,118.47,173.89,206.21,199.91,163.17,202.59,276.49,266.26,257.26,244.81,204.04,233.14,250.08,188.5,200.52 }, 
        .close_price = { 16.0,12.75,12.8,15.32,16.05,14.88,14.15,15.65,14.13,13.6,13.18,12.63,14.25,16.8,16.67,18.55,20.94,22.73,24.11,21.56,23.73,22.74,22.1,20.59,20.76,23.62,22.87,17.74,19.59,18.98,22.86,19.88,20.11,17.65,22.49,23.37,22.19,20.47,21.33,18.66,15.91,12.34,14.9,16.11,15.04,16.06,20.99,22.0,27.89,43.37,44.53,34.93,52.13,55.67,71.99,95.38,166.11,143.0,129.35,189.2,235.22,264.51,225.17,222.64,236.48,208.41,226.57,229.07,245.24,258.49,371.33,381.59,352.26,312.24,290.14,359.2,290.25,252.75,224.47,297.15,275.61,265.25,227.54,194.7,123.18,173.22,205.71,207.46,164.31,203.93,261.77,267.43,258.08,250.22,200.84,240.08,248.48,187.29,201.88,171.32 }, 
        .high_price = { 16.24,15.43,13.3,15.99,17.96,16.21,16.06,15.69,15.78,14.07,14.38,13.29,14.92,17.23,19.16,18.8,20.99,22.86,25.8,24.76,24.67,25.97,24.2,22.17,23.16,24.03,24.0,23.24,20.63,20.87,24.92,24.32,25.83,21.0,23.14,24.45,25.3,23.47,21.62,20.48,19.74,17.22,15.65,17.74,16.3,16.9,22.72,24.08,29.02,43.53,64.6,53.8,57.99,56.22,72.51,119.67,166.71,167.5,155.3,202.6,239.57,300.13,293.5,240.37,260.26,235.33,232.54,233.33,246.8,266.33,371.74,414.5,390.95,402.67,315.92,371.59,384.29,318.5,264.21,298.32,314.67,313.8,257.5,237.4,198.92,180.68,217.65,207.79,202.69,204.48,276.99,299.29,266.47,278.98,268.94,252.75,265.13,251.25,205.6,204.52 }, 
        .low_price = { 14.32,12.16,9.4,12.1,15.55,13.58,12.52,13.73,13.91,12.9,12.8,11.88,12.0,14.06,16.13,16.19,18.97,19.38,22.28,20.21,20.75,22.36,21.11,19.51,20.0,20.38,19.65,16.55,16.31,18.23,18.92,19.08,19.21,16.82,16.52,21.67,19.61,18.62,19.25,16.96,15.41,12.27,11.8,14.81,14.07,14.56,14.95,20.62,21.82,28.11,40.77,23.37,29.76,45.54,56.94,72.03,91.0,109.96,126.37,130.77,180.4,239.06,206.33,179.83,219.81,182.33,190.41,206.82,216.28,236.28,254.53,326.2,295.37,264.0,233.33,252.01,273.9,206.86,208.69,216.17,271.81,262.47,198.59,166.19,108.24,101.81,169.93,163.91,152.37,158.83,199.37,254.12,212.36,234.58,194.07,197.85,228.2,180.06,175.01,160.51 } 
    }; 

    ticker.stocks[11] = (stock_t){
        .name = "UnitedHealth Group Incorporated", 
        .symbol = "UNH", 
        .open_price = { 113.53,116.91,114.83,119.49,128.69,132.61,133.58,141.16,143.42,136.82,139.35,141.51,159.12,161.13,162.75,166.72,164.62,175.0,175.78,186.29,193.4,199.79,196.59,211.62,228.89,221.02,235.26,225.7,218.46,237.0,243.74,245.0,256.1,268.0,267.25,262.92,283.0,245.0,268.47,243.56,249.71,233.07,241.49,245.95,249.19,231.74,219.19,253.99,281.78,293.98,275.13,257.34,238.69,288.39,304.02,295.83,303.59,310.16,312.91,312.64,344.77,351.45,335.03,334.36,372.2,401.0,413.73,402.03,413.57,416.54,391.6,461.82,452.89,500.0,475.0,470.89,510.68,510.81,498.32,512.32,542.27,519.33,507.08,555.0,552.36,525.13,499.95,473.61,485.2,494.59,487.79,478.1,507.5,479.0,505.53,529.98,550.42,526.84,508.83,489.42 }, 
        .close_price = { 117.64,115.16,119.1,128.9,131.68,133.67,141.2,143.2,136.05,140.0,141.33,158.32,160.04,162.1,165.38,164.01,174.88,175.18,185.42,191.81,198.9,195.85,210.22,228.17,220.46,236.78,226.16,214.0,236.4,241.51,245.34,253.22,268.46,266.04,261.35,281.36,249.12,270.2,242.22,247.26,233.07,241.8,244.01,249.01,234.0,217.32,252.7,279.87,293.98,272.45,254.96,249.38,292.47,304.85,294.95,302.78,312.55,311.77,305.14,336.34,350.68,333.58,332.22,372.07,398.8,411.92,400.44,412.22,416.27,390.74,460.47,444.22,502.14,472.57,475.87,509.97,508.55,496.78,513.63,542.34,519.33,505.04,555.15,547.76,530.18,499.19,475.94,472.59,492.09,487.24,480.64,506.37,476.58,504.19,535.56,552.97,526.47,511.74,493.6,493.32 }, 
        .high_price = { 121.09,117.89,122.26,131.1,135.11,134.75,141.31,144.48,144.16,141.78,146.36,159.76,164.0,163.8,166.76,172.14,176.07,178.89,188.66,193.0,199.49,200.76,212.77,228.75,231.77,250.79,237.82,231.27,241.67,249.17,256.73,259.01,270.17,271.16,272.81,285.45,287.94,272.44,272.49,259.25,250.2,251.18,253.49,268.69,251.58,236.56,255.72,283.0,300.0,302.54,306.71,295.84,304.0,309.66,315.84,310.97,324.57,323.82,335.65,367.95,354.1,367.49,344.64,380.5,402.16,425.98,413.73,422.53,431.36,424.4,461.39,466.0,509.23,503.75,500.93,521.89,553.29,513.51,518.7,544.34,553.13,535.02,558.1,555.69,553.0,525.63,504.38,486.29,530.45,500.85,502.9,515.86,513.65,514.15,546.78,553.94,554.7,549.0,532.81,496.0 }, 
        .low_price = { 113.05,107.51,108.83,119.37,125.26,128.53,133.02,139.32,135.55,132.39,133.03,136.22,156.23,156.09,156.49,162.74,164.25,166.65,175.19,183.86,190.63,188.25,186.0,208.92,218.19,220.0,208.48,212.5,214.63,228.23,241.29,244.12,252.23,257.48,253.32,258.28,231.81,236.13,239.15,234.51,208.07,227.18,234.83,239.54,220.78,213.12,212.08,249.09,273.85,271.18,245.3,187.72,226.03,275.56,273.71,287.1,299.2,289.64,299.6,307.36,329.4,329.01,320.35,332.67,360.55,400.53,387.25,401.81,404.3,390.46,383.12,436.0,439.22,447.27,445.74,467.73,504.53,463.33,449.7,492.25,519.16,499.0,487.74,500.77,515.72,474.75,463.89,457.59,478.36,472.54,445.68,447.18,476.29,472.12,503.14,526.8,515.87,479.0,484.39,468.19 } 
    }; 

    ticker.stocks[12] = (stock_t){
        .name = "Visa Inc.", 
        .symbol = "V", 
        .open_price = { 79.53,76.06,74.08,72.99,76.25,77.81,78.69,74.5,78.31,81.14,82.42,82.64,77.57,78.76,82.9,88.74,89.14,91.29,95.4,94.38,100.36,104.04,105.54,110.5,112.38,114.57,124.74,123.26,119.27,126.86,131.84,131.96,137.74,146.93,150.89,139.0,145.0,130.0,135.39,149.46,157.53,165.54,161.54,175.33,179.19,180.52,173.02,180.13,184.24,189.0,199.94,186.32,156.32,174.45,194.71,193.85,191.8,212.21,202.21,184.51,212.13,220.25,195.14,214.97,213.78,234.05,229.44,234.2,246.24,229.1,224.17,213.49,196.03,217.52,226.9,214.48,223.08,211.77,212.05,196.79,208.45,198.72,179.34,208.91,217.0,209.28,229.37,219.46,225.23,232.87,222.73,237.0,237.14,247.47,229.24,236.14,255.79,259.61,273.39,283.2 }, 
        .close_price = { 77.55,74.49,72.39,76.48,77.24,78.94,74.17,78.05,80.9,82.7,82.51,77.32,78.02,82.71,87.94,88.87,91.22,95.23,93.78,99.56,103.52,105.24,109.98,112.59,114.02,124.23,122.94,119.62,126.88,130.72,132.45,136.74,146.89,150.09,137.85,141.71,131.94,135.01,148.12,156.19,164.43,161.33,173.55,178.0,180.82,172.01,178.86,184.51,187.9,198.97,181.76,161.12,178.72,195.24,193.17,190.4,211.99,199.97,181.71,210.35,218.73,193.25,212.39,211.73,233.56,227.3,233.82,246.39,229.1,222.75,211.77,193.77,216.71,226.17,216.12,221.77,213.13,212.17,196.89,212.11,198.71,177.65,207.16,217.0,207.76,230.21,219.94,225.46,232.73,221.03,237.48,237.73,245.68,230.01,235.1,256.68,260.35,273.26,282.64,287.35 }, 
        .high_price = { 80.49,76.51,74.78,77.0,81.73,79.87,81.71,80.17,81.76,83.79,83.7,83.96,80.39,84.27,88.49,92.05,92.8,95.53,96.6,101.18,104.2,106.84,110.74,113.62,114.92,126.88,126.26,125.44,127.9,132.5,136.69,143.14,147.71,150.64,151.56,145.46,145.72,139.9,148.82,156.82,165.7,165.77,174.94,184.07,182.4,187.05,180.18,184.85,189.89,210.13,214.17,194.49,182.25,198.29,202.18,200.95,216.16,217.35,207.97,217.65,220.39,220.25,220.53,228.23,237.5,235.74,238.48,252.67,247.83,233.33,236.96,221.61,219.73,228.12,235.85,228.81,229.24,214.8,217.58,218.07,217.61,207.19,211.52,217.0,219.98,232.84,234.3,227.42,235.57,234.81,238.28,245.37,248.23,250.06,241.48,256.77,263.25,279.99,286.13,289.04 }, 
        .low_price = { 75.52,68.76,66.12,69.58,75.8,76.22,73.25,73.83,77.73,80.97,81.11,77.28,75.17,78.49,81.57,87.85,88.13,91.14,92.8,93.19,99.43,102.26,104.9,106.9,106.6,113.95,111.02,116.03,116.71,125.32,129.53,131.15,137.0,142.54,129.79,129.54,121.6,127.88,135.26,144.5,156.32,156.42,156.75,172.74,166.98,172.01,168.59,175.18,179.66,187.16,172.98,133.93,150.6,171.72,186.21,187.18,190.08,193.13,179.23,183.89,204.5,192.81,195.02,205.78,212.3,220.31,226.28,234.05,228.66,216.31,208.54,192.55,190.1,195.65,201.45,186.67,201.1,189.95,185.91,194.14,198.64,174.83,174.6,193.33,202.13,206.16,217.46,208.76,224.12,216.14,221.02,227.68,235.25,227.92,227.78,235.68,252.14,256.86,272.76,276.16 } 
    }; 

    ticker.stocks[13] = (stock_t){
        .name = "Exxon Mobil Corporation", 
        .symbol = "XOM", 
        .open_price = { 81.76,77.5,76.66,80.56,82.4,88.24,88.43,93.36,88.08,86.72,86.94,83.5,87.98,90.94,84.0,81.7,82.02,81.51,80.37,80.79,80.16,76.37,81.3,83.39,83.44,83.82,87.5,75.53,74.27,77.26,81.87,81.89,80.89,80.41,85.35,79.83,80.24,67.35,74.92,79.38,81.23,79.94,71.09,77.13,73.74,67.89,70.83,68.39,68.5,70.24,61.38,52.59,36.86,45.63,45.32,44.49,42.05,39.75,33.79,33.14,38.96,41.45,45.58,56.47,56.32,57.98,59.45,64.33,57.55,54.49,59.41,65.07,60.9,61.24,76.45,78.77,81.99,85.01,97.02,86.74,94.79,94.42,90.04,112.37,111.64,109.78,115.83,109.31,113.39,115.99,101.75,107.49,106.95,112.2,117.53,106.53,102.5,100.92,103.57,105.72 }, 
        .close_price = { 77.95,77.85,80.15,83.59,88.4,89.02,93.74,88.95,87.14,87.28,83.32,87.3,90.26,83.89,81.32,82.01,81.65,80.5,80.73,80.04,76.33,81.98,83.35,83.29,83.64,87.3,75.74,74.61,77.75,81.24,82.73,81.51,80.17,85.02,79.68,79.5,68.19,73.28,79.03,80.8,80.28,70.77,76.63,74.36,68.48,70.61,67.57,68.13,69.78,62.12,51.44,37.97,46.47,45.47,44.72,42.08,39.94,34.33,32.62,38.13,41.22,44.84,54.37,55.83,57.24,58.37,63.08,57.57,54.52,58.82,64.47,59.84,61.19,75.96,78.42,82.59,85.25,96.0,85.64,96.93,95.59,87.31,110.81,111.34,110.3,116.01,109.91,109.66,118.34,102.18,107.25,107.24,111.19,117.58,105.85,102.74,99.98,102.81,104.52,113.09 }, 
        .high_price = { 82.13,79.92,83.44,85.1,89.78,90.46,93.83,95.55,88.94,89.37,88.67,88.19,93.22,91.34,84.16,84.25,83.55,83.23,83.69,82.49,80.82,82.45,84.24,84.14,84.36,89.3,89.25,76.98,80.9,82.65,83.79,84.4,81.59,87.36,86.89,83.75,81.95,73.49,79.75,82.0,83.49,80.26,77.76,77.93,74.27,75.18,70.91,73.12,70.54,71.37,63.01,54.15,47.68,47.15,55.36,45.38,46.42,40.03,35.95,42.08,44.47,51.08,57.25,62.55,59.48,64.02,64.93,64.42,59.06,60.48,65.94,66.38,63.35,76.42,83.08,91.51,89.8,99.78,105.57,97.52,101.56,99.19,112.91,114.66,112.07,117.78,119.63,113.84,119.92,117.3,109.14,108.46,112.07,120.7,117.79,109.19,104.22,104.88,105.43,113.49 }, 
        .low_price = { 73.79,71.55,73.55,80.31,81.99,87.23,87.61,86.12,85.58,82.29,82.99,82.76,86.6,83.13,80.76,80.31,80.3,80.47,79.26,78.27,76.05,76.32,81.25,80.01,82.17,83.66,73.9,72.67,72.16,75.4,79.3,80.71,76.51,79.6,76.22,74.7,64.65,67.26,72.73,77.86,79.56,70.63,70.97,74.18,66.53,67.63,66.31,67.32,67.52,61.86,48.01,30.11,36.34,40.2,43.16,40.91,39.31,33.76,31.11,32.53,38.34,41.0,44.29,54.45,54.3,57.74,59.45,54.6,52.1,52.96,59.41,59.54,57.96,61.21,74.03,76.25,79.29,83.4,83.52,80.69,86.28,83.89,89.72,107.48,102.37,104.76,108.64,98.02,113.12,101.74,101.26,100.22,104.57,112.2,104.54,101.15,97.48,95.77,100.42,104.03 } 
    }; 

    ticker.stocks[14] = (stock_t){
        .name = "Johnson & Johnson", 
        .symbol = "JNJ", 
        .open_price = { 101.73,101.71,103.61,105.9,108.0,112.22,112.69,121.3,125.31,119.19,118.0,114.76,111.36,115.78,112.48,122.49,124.73,123.4,128.32,132.79,133.17,132.6,130.16,139.83,139.57,139.66,137.53,129.11,127.82,126.32,120.38,121.34,132.39,134.69,138.26,140.07,145.57,128.13,134.02,137.22,139.99,140.95,131.5,140.2,130.26,127.99,130.02,132.05,137.72,145.87,149.42,134.78,127.7,149.62,147.29,140.69,146.39,153.87,149.31,138.98,146.29,157.24,165.31,161.45,162.6,163.6,170.15,164.74,172.47,172.9,161.53,163.16,156.88,170.21,171.74,163.04,177.05,180.47,179.15,177.45,174.17,161.49,164.29,174.06,179.0,176.16,162.99,153.01,154.95,163.6,154.54,164.34,166.37,161.42,155.42,149.19,156.44,156.93,158.16,161.83 }, 
        .close_price = { 102.72,104.44,105.21,108.2,112.08,112.69,121.3,125.23,119.34,118.13,115.99,111.3,115.21,113.25,122.21,124.55,123.47,128.25,132.29,132.72,132.37,130.01,139.41,139.33,139.72,138.19,129.88,128.15,126.49,119.62,121.34,132.52,134.69,138.17,139.99,146.9,129.05,133.08,136.64,139.79,141.2,131.15,139.28,130.22,128.36,129.38,132.04,137.49,145.87,148.87,134.48,131.13,150.04,148.75,140.63,145.76,153.41,148.88,137.11,144.68,157.38,163.13,158.46,164.35,162.73,169.25,164.74,172.2,173.13,161.5,162.88,155.93,171.07,172.29,164.57,177.23,180.46,179.53,177.51,174.52,161.34,163.36,173.97,178.0,176.65,163.42,153.26,155.0,163.7,155.06,165.52,167.53,161.68,155.75,148.34,154.66,156.74,158.9,161.38,156.21 }, 
        .high_price = { 105.49,104.75,106.92,109.56,114.19,115.0,121.41,126.07,125.9,119.97,120.2,122.5,117.3,117.0,122.88,129.0,125.81,128.8,137.0,137.08,134.97,135.79,144.35,141.87,143.8,148.32,140.67,135.7,132.88,127.61,124.85,132.64,137.43,143.13,141.43,148.75,148.99,135.19,137.95,140.0,141.45,142.35,144.98,142.47,134.1,132.78,137.49,138.63,147.84,151.19,154.5,143.64,157.0,153.62,150.03,151.67,154.4,155.47,153.14,151.3,157.66,173.65,167.94,167.03,167.79,172.74,170.2,173.38,179.92,175.22,166.03,167.62,173.51,174.3,173.62,180.21,186.69,181.74,183.35,179.99,175.49,167.67,175.39,178.12,181.04,180.93,166.34,156.25,167.23,166.18,166.27,175.36,175.97,165.27,159.27,155.14,160.02,163.58,162.25,163.11 }, 
        .low_price = { 100.31,94.28,99.78,105.44,107.69,111.7,112.07,120.79,118.33,117.04,112.99,111.3,109.32,110.76,112.47,122.39,120.95,122.34,128.12,129.57,130.9,129.05,130.02,136.6,138.6,138.1,122.15,124.9,123.54,118.62,119.97,120.11,128.93,133.44,132.23,139.0,121.0,125.0,131.26,135.74,134.42,128.52,131.04,127.84,126.63,126.34,126.1,129.68,136.16,141.38,130.82,109.16,125.5,143.01,137.02,140.06,145.82,142.96,133.65,137.49,145.86,154.13,157.97,151.47,156.53,163.12,161.79,164.63,171.3,161.41,157.34,155.85,156.25,158.26,155.72,162.41,175.52,172.69,167.26,169.76,161.27,160.81,159.17,166.82,174.07,161.05,153.04,150.11,153.94,153.32,153.15,157.33,161.28,155.26,144.95,145.64,151.77,156.79,154.84,155.66 } 
    }; 

    ticker.stocks[15] = (stock_t){
        .name = "Mastercard Incorporated", 
        .symbol = "MA", 
        .open_price = { 98.21,95.37,88.52,87.77,93.65,97.15,95.9,89.16,95.23,96.67,101.39,107.0,102.33,104.41,106.63,111.42,112.7,116.54,122.8,122.25,128.66,133.84,141.9,149.95,150.4,152.01,172.51,176.35,174.64,178.27,192.03,195.74,199.27,215.68,224.84,198.89,206.01,185.83,211.99,226.88,238.3,254.9,251.8,269.99,273.93,279.99,271.49,279.0,290.59,300.46,318.8,298.89,230.94,268.7,300.8,295.96,311.0,357.71,342.24,294.24,339.67,358.0,320.91,360.68,357.04,385.47,364.48,366.05,389.3,347.38,349.83,335.25,320.78,359.79,385.76,357.85,359.22,363.0,358.2,314.1,347.81,323.81,287.85,332.22,357.99,349.96,368.57,354.0,362.61,380.49,367.16,391.34,393.78,413.84,393.6,378.67,412.89,424.09,455.0,474.91 }, 
        .close_price = { 97.36,89.03,86.92,94.5,96.99,95.9,88.06,95.24,96.63,101.77,107.02,102.2,103.25,106.33,110.46,112.47,116.32,122.88,121.45,127.8,133.3,141.2,148.77,150.47,151.36,169.0,175.76,175.16,178.27,190.12,196.52,198.0,215.56,222.61,197.67,201.07,188.65,211.13,224.77,235.45,254.24,251.49,264.53,272.27,281.37,271.57,276.81,292.23,298.59,315.94,290.25,241.56,274.97,300.89,295.7,308.53,358.19,338.17,288.64,336.51,356.94,316.29,353.85,356.05,382.06,360.58,365.09,385.94,346.23,347.68,335.52,314.92,359.32,386.38,360.82,357.38,363.38,357.87,315.48,353.79,324.37,284.34,328.18,356.4,347.73,370.6,355.29,363.41,380.03,365.02,393.3,394.28,412.64,395.91,376.35,413.83,426.51,449.23,474.76,484.0 }, 
        .high_price = { 100.8,95.83,89.2,94.94,100.0,97.94,97.99,96.5,97.19,102.31,108.93,107.14,105.71,111.07,111.0,113.5,117.37,122.98,126.19,132.2,134.5,143.59,152.0,154.65,154.65,170.81,179.17,183.73,180.0,194.72,204.0,214.28,215.86,224.36,225.35,208.86,209.91,212.97,225.6,237.08,257.43,258.86,269.85,283.33,282.96,293.69,280.44,293.0,301.53,327.09,347.25,314.59,285.0,310.0,316.06,317.24,367.25,361.6,355.0,357.0,359.41,358.13,368.79,389.5,401.5,386.87,380.92,395.28,389.98,362.59,367.35,371.13,364.65,386.55,399.92,370.76,381.97,369.24,368.31,356.8,361.95,339.48,331.8,356.4,369.26,390.0,380.47,369.15,381.93,392.2,395.17,405.19,417.78,418.6,405.34,414.16,428.36,462.0,479.14,484.61 }, 
        .low_price = { 94.5,81.0,78.52,85.93,93.04,94.08,87.59,86.65,94.41,96.51,99.78,100.36,99.51,104.11,104.01,110.13,111.01,115.55,119.89,120.65,127.59,131.68,141.32,145.28,140.61,151.12,156.8,168.55,167.94,176.91,191.9,194.75,197.87,209.7,183.75,177.4,171.89,180.98,211.2,215.93,233.12,239.96,240.25,264.87,253.9,266.61,258.51,268.42,281.5,296.02,273.55,199.99,227.1,263.01,285.15,288.65,309.3,320.81,281.2,288.12,325.53,312.38,317.58,344.68,355.24,355.37,359.54,363.37,344.68,335.62,328.87,310.11,306.0,330.59,341.31,305.61,342.87,312.77,303.65,309.46,324.28,281.69,276.87,308.6,336.43,343.94,349.59,340.21,355.97,357.85,365.85,387.13,386.42,391.48,359.77,375.04,404.32,416.53,450.12,464.62 } 
    }; 

    ticker.stocks[16] = (stock_t){
        .name = "The Procter & Gamble Company", 
        .symbol = "PG", 
        .open_price = { 74.87,78.36,81.21,80.54,82.0,80.02,80.95,84.52,85.44,87.36,89.35,86.58,82.21,83.88,87.03,91.05,89.86,87.38,88.02,87.4,91.03,92.42,91.26,86.33,90.18,91.92,86.15,78.4,79.26,72.05,73.33,77.5,80.42,82.48,83.31,88.81,94.67,91.03,96.35,98.61,104.23,106.15,103.15,109.92,118.56,119.79,124.36,124.83,121.94,124.5,124.66,113.19,107.95,117.6,116.0,119.65,130.47,137.86,139.58,138.51,139.16,139.66,129.0,124.16,135.05,134.03,135.79,135.43,141.77,142.33,139.93,143.36,144.85,161.69,160.79,154.31,153.52,161.6,148.0,144.24,138.34,137.83,127.25,134.7,149.53,150.95,142.08,138.05,148.43,156.03,143.25,151.48,155.88,154.9,144.78,150.68,153.33,146.36,156.77,158.05 }, 
        .close_price = { 79.41,81.69,80.29,82.31,80.12,81.04,84.67,85.59,87.31,89.75,86.8,82.46,84.08,87.6,91.07,89.85,87.33,88.09,87.15,90.82,92.27,90.98,86.34,89.99,91.88,86.34,78.52,79.28,72.34,73.17,78.06,80.88,82.95,83.23,88.68,94.51,91.92,96.47,98.55,104.05,106.48,102.91,109.65,118.04,120.23,124.38,124.51,122.06,124.9,124.62,113.23,110.0,117.87,115.92,119.57,131.12,138.33,138.99,137.1,138.87,139.14,128.21,123.53,135.43,133.42,134.85,134.93,142.23,142.39,139.8,142.99,144.58,163.58,160.45,155.89,152.8,160.55,147.88,143.79,138.91,137.94,126.25,134.67,149.16,151.56,142.38,137.56,148.69,156.38,142.5,151.74,156.3,154.34,145.86,150.03,153.52,146.54,157.14,158.94,161.83 }, 
        .high_price = { 81.23,82.0,83.0,83.87,83.84,82.89,84.8,86.89,88.5,90.22,90.33,87.69,85.74,87.98,91.8,92.0,91.13,88.35,90.21,91.07,92.96,94.67,93.51,90.37,93.14,91.93,86.5,80.75,79.52,75.02,78.71,80.98,84.2,86.28,90.7,94.81,96.9,96.81,100.45,104.15,107.2,108.68,112.63,121.76,122.0,125.36,125.77,125.14,126.6,127.0,128.09,124.69,124.99,118.37,121.82,132.03,139.69,141.7,145.87,146.92,140.07,141.04,130.72,137.6,138.59,139.1,136.84,144.54,145.98,147.23,144.87,149.72,164.98,165.35,164.98,156.47,164.9,162.0,148.12,148.61,150.63,141.8,135.67,149.16,154.65,154.8,144.1,148.69,158.11,157.57,152.07,157.68,158.38,155.32,151.38,153.63,153.49,158.5,161.74,162.73 }, 
        .low_price = { 74.87,74.46,79.63,80.48,79.1,79.41,80.86,84.32,85.13,85.96,84.06,81.71,81.18,83.24,86.75,89.59,87.15,85.52,86.93,86.31,90.52,90.34,85.72,85.42,89.12,86.08,78.5,75.81,71.95,70.73,72.86,77.42,80.11,81.25,78.49,88.18,86.74,89.08,95.99,97.75,102.13,102.41,102.4,109.61,112.68,119.02,116.17,118.24,120.96,121.86,106.67,94.34,107.0,111.25,113.76,118.9,130.47,134.7,134.68,136.74,134.2,127.44,121.82,121.54,130.29,133.37,131.94,134.92,140.8,139.53,137.6,142.28,144.85,156.04,150.56,143.03,151.26,139.18,129.5,138.23,137.86,126.21,122.18,130.96,148.08,138.73,135.83,136.1,147.09,141.9,142.45,146.96,150.93,144.82,141.45,148.83,142.5,146.28,154.91,157.61 } 
    }; 

    ticker.stocks[17] = (stock_t){
        .name = "The Home Depot, Inc.", 
        .symbol = "HD", 
        .open_price = { 133.52,130.11,124.92,124.78,133.1,134.37,132.12,128.29,138.06,134.48,128.2,121.69,129.34,135.1,137.66,146.72,146.94,156.22,153.52,154.39,150.24,150.26,164.2,166.42,180.32,190.21,199.34,182.75,177.15,184.73,187.21,193.82,196.86,200.69,208.52,176.84,183.29,169.71,184.03,185.82,192.99,203.2,189.52,209.7,214.14,226.45,233.01,236.07,220.9,219.08,230.3,219.98,175.91,216.77,249.41,249.65,266.73,284.03,279.44,270.15,278.73,266.01,271.23,258.81,306.88,326.28,320.66,319.91,330.0,325.56,328.15,373.0,402.08,416.57,369.47,314.59,300.5,301.99,301.74,275.73,300.64,288.4,281.0,300.37,326.31,317.42,322.39,291.92,294.87,298.98,284.05,309.78,331.76,332.0,300.52,285.59,313.83,344.21,353.4,380.36 }, 
        .close_price = { 132.25,125.76,124.12,133.43,133.89,132.12,127.69,138.24,134.12,128.68,122.01,129.4,134.08,137.58,144.91,146.83,156.1,153.51,153.4,149.6,149.87,163.56,165.78,179.82,189.53,200.9,182.27,178.24,184.8,186.55,195.1,197.52,200.77,207.15,175.88,180.32,171.82,183.53,185.14,191.89,203.7,189.85,207.97,213.69,227.91,232.02,234.58,220.51,218.38,228.1,217.84,186.71,219.83,248.48,250.51,265.49,285.04,277.71,266.71,277.41,265.62,270.82,258.34,305.25,323.67,318.91,318.89,328.19,326.18,328.26,371.74,400.61,415.01,366.98,315.83,299.33,300.4,302.75,274.27,300.94,288.42,275.94,296.13,323.99,315.86,324.17,296.54,295.12,300.54,283.45,310.64,333.84,330.3,302.16,284.69,313.49,346.55,352.96,380.61,379.41 }, 
        .high_price = { 134.83,131.94,127.75,134.29,137.0,137.82,132.73,138.72,139.0,135.88,130.45,132.14,137.32,139.37,146.33,150.15,156.27,160.86,159.22,154.79,156.05,163.61,167.94,180.67,191.49,207.61,202.25,184.4,187.8,191.65,201.6,204.25,203.55,215.43,209.79,188.69,183.5,184.67,193.42,192.19,208.3,203.52,211.99,219.3,229.27,235.49,238.99,239.31,222.0,236.53,247.36,241.32,224.22,252.23,259.29,269.07,292.95,288.04,292.65,289.0,278.95,285.77,284.68,308.02,328.83,345.69,321.26,333.45,338.55,343.74,375.15,416.56,420.61,417.84,374.67,340.74,318.4,315.75,308.46,310.67,332.98,302.83,299.28,329.08,347.25,335.16,341.47,300.11,303.2,299.56,315.46,334.07,338.17,333.45,303.45,314.58,354.92,362.96,381.78,385.1 }, 
        .low_price = { 130.01,113.59,109.62,123.98,131.75,130.02,123.62,128.12,133.6,125.35,121.62,119.2,128.68,133.05,136.33,145.83,145.76,153.28,150.75,144.25,146.89,149.76,161.51,160.53,176.7,187.82,175.42,171.56,170.42,181.2,186.52,192.12,191.09,200.5,170.91,167.0,158.09,168.21,182.45,179.52,192.85,186.27,188.75,208.17,199.05,220.67,222.12,216.88,210.61,216.4,212.33,140.63,174.0,215.21,234.31,246.22,263.84,262.81,262.03,268.52,258.73,261.06,254.03,246.59,303.89,309.07,298.4,314.8,316.61,320.28,324.16,364.7,380.9,343.61,299.29,298.89,293.59,279.59,264.51,274.55,288.28,265.61,267.87,277.5,310.73,307.39,292.0,279.93,284.24,277.09,279.98,300.89,321.2,299.82,274.26,282.02,313.0,336.59,350.02,368.87 } 
    }; 

    ticker.stocks[18] = (stock_t){
        .name = "Advanced Micro Devices, Inc.", 
        .symbol = "AMD", 
        .open_price = { 2.36,2.77,2.17,2.16,2.79,3.58,4.6,5.09,6.89,7.18,6.95,7.32,8.92,11.42,10.9,15.08,14.6,13.43,11.25,12.57,13.72,13.12,12.8,11.25,10.81,10.42,13.62,12.26,9.99,10.83,13.98,14.8,18.34,25.62,30.69,18.41,22.48,18.01,24.61,23.97,26.42,28.95,28.75,31.79,30.5,30.83,29.05,34.37,39.32,46.86,46.4,47.42,44.18,51.07,53.31,52.63,78.19,91.92,83.06,75.85,92.25,92.11,86.83,85.37,80.16,81.97,81.01,94.04,105.93,111.3,102.6,119.45,160.37,145.14,116.75,122.33,110.48,85.66,102.13,75.19,95.59,82.35,64.46,61.49,78.31,66.0,78.47,78.55,96.7,91.03,117.29,115.16,114.26,107.0,102.21,98.58,119.88,144.28,169.27,197.91 }, 
        .close_price = { 2.87,2.2,2.14,2.85,3.55,4.57,5.14,6.86,7.4,6.91,7.23,8.91,11.34,10.37,14.46,14.55,13.3,11.19,12.48,13.61,13.0,12.75,10.99,10.89,10.28,13.74,12.11,10.05,10.88,13.73,14.99,18.33,25.17,30.89,18.21,21.3,18.46,24.41,23.53,25.52,27.63,27.41,30.37,30.45,31.45,28.99,33.93,39.15,45.86,47.0,45.48,45.48,52.39,53.8,52.61,77.43,90.82,81.99,75.29,92.66,91.71,85.64,84.51,78.5,81.62,80.08,93.93,106.19,110.72,102.9,120.23,158.37,143.9,114.25,123.34,109.34,85.52,101.86,76.47,94.47,84.87,63.36,60.06,77.63,64.77,75.15,78.58,98.01,89.37,118.21,113.91,114.4,105.72,102.82,98.5,121.16,147.41,167.69,192.53,181.42 }, 
        .high_price = { 3.06,2.82,2.19,2.98,3.99,4.71,5.52,7.16,8.0,7.64,7.53,9.23,12.42,11.69,15.55,15.09,14.74,13.63,14.67,15.65,13.93,14.24,14.41,12.27,11.19,13.85,13.84,12.82,11.36,13.95,17.34,20.18,27.3,34.14,31.91,22.22,23.75,25.14,25.52,28.11,29.95,29.67,34.3,34.86,35.55,32.05,34.34,41.79,47.31,52.81,59.27,50.2,58.63,56.98,59.0,78.96,92.64,94.28,88.72,92.74,97.98,99.23,94.22,86.95,89.2,82.0,94.34,106.97,122.49,111.85,128.08,164.46,160.88,152.42,132.96,125.67,111.42,104.55,109.57,94.81,104.59,85.68,70.29,79.16,79.23,77.08,88.94,102.43,97.27,130.79,132.83,122.12,119.5,111.82,111.31,125.73,151.05,184.92,193.0,227.3 }, 
        .low_price = { 2.2,1.75,1.81,2.12,2.6,3.45,4.07,4.82,6.15,5.66,6.24,6.22,8.26,9.42,10.81,12.38,12.22,9.85,10.57,12.13,11.86,12.04,10.65,10.66,9.7,10.34,10.63,9.79,9.04,10.77,13.92,14.74,18.0,25.57,16.17,17.18,16.03,16.94,22.27,21.04,25.83,26.03,27.29,30.1,27.65,28.35,27.43,34.1,37.15,46.1,41.04,36.75,41.7,49.09,48.42,51.6,76.1,73.85,74.23,73.76,89.03,85.02,79.36,73.86,77.94,72.5,78.96,84.24,101.98,99.51,99.82,118.13,130.6,99.35,104.26,100.08,84.02,83.27,75.48,71.6,83.72,62.83,54.57,58.03,61.96,60.05,75.92,76.65,83.76,81.02,107.08,108.55,99.58,94.46,93.12,98.5,116.37,133.74,161.81,177.36 } 
    }; 

    ticker.stocks[19] = (stock_t){
        .name = "Costco Wholesale Corporation", 
        .symbol = "COST", 
        .open_price = { 162.02,159.81,150.84,150.4,157.87,148.76,150.5,157.19,167.15,159.0,152.22,148.48,150.11,160.65,163.84,177.37,167.74,178.5,180.81,160.21,159.1,157.59,164.92,161.97,183.26,187.23,193.41,191.31,186.91,196.49,195.45,208.42,218.65,233.14,235.81,228.18,230.72,200.5,214.0,219.76,243.07,245.34,239.78,266.43,275.75,292.57,288.04,297.99,299.75,294.06,307.0,294.44,282.36,301.78,307.9,302.5,325.55,345.71,356.26,362.22,384.5,377.43,351.21,335.21,352.54,373.84,379.93,396.3,430.62,455.48,449.73,494.15,543.1,565.03,505.0,519.46,577.38,532.23,469.38,481.18,541.42,519.72,474.5,503.7,519.14,458.0,508.31,481.1,496.5,499.15,509.33,537.25,560.63,553.07,567.91,555.0,593.28,655.58,694.0,740.44 }, 
        .close_price = { 161.5,151.12,150.03,157.58,148.13,148.77,157.04,167.22,162.09,152.51,147.87,150.11,160.11,163.95,177.18,167.69,177.52,180.43,159.93,158.51,156.74,164.29,161.08,184.43,186.12,194.87,190.9,188.43,197.16,198.24,208.98,218.71,233.13,234.88,228.63,231.28,203.71,214.63,218.74,242.14,245.53,239.58,264.26,275.63,294.76,288.11,297.11,299.81,293.92,305.52,281.14,285.13,303.0,308.47,303.21,325.53,347.66,355.0,357.62,391.77,376.78,352.43,331.0,352.48,372.09,378.27,395.67,429.72,455.49,449.35,491.54,539.38,567.7,505.13,519.25,575.85,531.72,466.22,479.28,541.3,522.1,472.27,501.5,539.25,456.5,511.14,484.18,496.87,503.22,511.56,538.38,560.67,549.28,564.96,552.44,592.74,660.08,694.88,743.89,732.17 }, 
        .high_price = { 169.73,161.23,154.88,159.8,159.09,153.88,158.79,168.82,169.59,159.28,152.32,153.42,164.95,164.8,177.87,178.71,178.27,183.18,182.72,161.35,162.35,165.32,167.29,184.9,195.35,199.88,195.52,192.99,199.04,201.77,212.46,224.62,233.52,245.16,237.57,240.88,233.86,215.56,219.69,242.44,248.7,251.01,268.94,284.31,299.95,307.34,304.88,307.1,300.2,314.28,325.26,324.51,322.63,311.83,315.35,331.49,349.06,363.67,384.87,393.15,388.07,381.55,361.67,357.77,375.44,389.45,400.47,431.5,460.62,470.49,494.17,560.78,571.49,568.72,534.24,586.32,612.27,546.14,491.13,542.12,564.75,542.6,512.82,542.58,519.14,511.41,530.05,499.86,513.13,514.79,539.56,571.16,569.21,572.18,577.3,599.94,681.91,705.52,752.56,787.08 }, 
        .low_price = { 157.33,144.91,141.62,147.1,147.46,138.57,148.55,154.67,161.15,147.2,147.1,142.11,150.11,158.51,161.81,164.1,166.11,169.06,156.56,150.0,150.06,155.03,154.11,161.29,181.13,183.88,175.79,180.83,180.94,190.18,195.0,206.05,215.01,231.51,217.0,217.45,189.51,199.85,205.75,215.77,240.31,233.05,238.08,261.67,262.71,284.4,281.6,295.17,289.1,288.62,271.28,276.34,280.9,294.54,293.84,300.75,324.3,331.2,352.85,360.59,359.5,351.88,330.94,307.0,351.63,371.11,375.5,393.88,425.48,445.67,436.17,487.2,514.04,469.01,482.98,511.78,529.7,406.51,443.2,478.0,520.26,463.53,449.03,474.5,450.75,447.9,483.78,465.33,477.5,476.75,502.1,524.63,530.56,540.18,540.23,549.65,590.59,640.51,691.5,711.01 } 
    }; 

}

static void dates_init(void) {
    dates = (date_t){.str = {"Dec 2015","Jan 2016","Feb 2016","Mar 2016","Apr 2016","May 2016","Jun 2016","Jul 2016","Aug 2016","Sep 2016","Oct 2016","Nov 2016","Dec 2016","Jan 2017","Feb 2017","Mar 2017","Apr 2017","May 2017","Jun 2017","Jul 2017","Aug 2017","Sep 2017","Oct 2017","Nov 2017","Dec 2017","Jan 2018","Feb 2018","Mar 2018","Apr 2018","May 2018","Jun 2018","Jul 2018","Aug 2018","Sep 2018","Oct 2018","Nov 2018","Dec 2018","Jan 2019","Feb 2019","Mar 2019","Apr 2019","May 2019","Jun 2019","Jul 2019","Aug 2019","Sep 2019","Oct 2019","Nov 2019","Dec 2019","Jan 2020","Feb 2020","Mar 2020","Apr 2020","May 2020","Jun 2020","Jul 2020","Aug 2020","Sep 2020","Oct 2020","Nov 2020","Dec 2020","Jan 2021","Feb 2021","Mar 2021","Apr 2021","May 2021","Jun 2021","Jul 2021","Aug 2021","Sep 2021","Oct 2021","Nov 2021","Dec 2021","Jan 2022","Feb 2022","Mar 2022","Apr 2022","May 2022","Jun 2022","Jul 2022","Aug 2022","Sep 2022","Oct 2022","Nov 2022","Dec 2022","Jan 2023","Feb 2023","Mar 2023","Apr 2023","May 2023","Jun 2023","Jul 2023","Aug 2023","Sep 2023","Oct 2023","Nov 2023","Dec 2023","Jan 2024","Feb 2024","Mar 2024"}};
}

static void data_init(void) {
    news_init();
    stocks_init();
    dates_init();
}

