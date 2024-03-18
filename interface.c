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

static date_t dates;

// Helper functions
static int max(int a, int b) {
    return a >= b ? a : b;
}

static int min(int a, int b) {
    return a <= b ? a : b;
}

// TODO: Test lprintf & rprintf
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

// Initialization functions

static void news_init() {
    // initialize news
    // Data obtained via Claude AI
    news.n = 10; // at most MAX_NEWS
    news.color = GL_AMBER;
    news.top = 0;

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

    // 18 data points; should be enough for demo
}

static void stocks_init() {
    // Data Source: Yahoo Finance scraped with Python script
    ticker.n = 20;
    ticker.top = 0;
    ticker.stocks[0] = (stock_t){
        .name = "Microsoft Corporation", 
        .symbol = "MSFT", 
        .open_price = { 54,54,54,50,55,50,52,51,56,57,57,59,60,62,64,64,65,68,70,69,73,74,74,83,83,86,94,93,90,93,99,98,106,110,114,107,113,99,103,112,118,130,123,136,137,136,139,144,151,158,170,165,153,175,182,203,211,225,213,204,214,222,235,235,238,253,251,269,286,302,282,331,335,335,310,296,309,277,275,256,277,258,235,234,253,243,248,250,286,306,325,339,335,331,316,339,376,373,401,411 }, 
        .close_price = { 55,55,50,55,49,53,51,56,57,57,59,60,62,64,63,65,68,69,68,72,74,74,83,84,85,95,93,91,93,98,98,106,112,114,106,110,101,104,112,117,130,123,133,136,137,139,143,151,157,170,162,157,179,183,203,205,225,210,202,214,222,231,232,235,252,249,270,284,301,281,331,330,336,310,298,308,277,271,256,280,261,232,232,255,239,247,249,288,307,328,340,335,327,315,338,378,376,397,413,425 } 
    }; 

    ticker.stocks[1] = (stock_t){
        .name = "Apple Inc.", 
        .symbol = "AAPL", 
        .open_price = { 29,25,24,24,27,23,24,23,26,26,28,28,27,28,31,34,35,36,38,36,37,41,38,42,42,42,41,44,41,41,46,45,49,57,56,54,46,38,41,43,47,52,43,50,53,51,56,62,66,74,76,70,61,71,79,91,108,132,117,109,121,133,133,123,123,132,125,136,146,152,141,148,167,177,174,164,174,156,149,136,161,156,138,155,148,130,143,146,164,169,177,193,196,189,171,171,190,187,183,179 }, 
        .close_price = { 26,24,24,27,23,24,23,26,26,28,28,27,28,30,34,35,35,38,36,37,41,38,42,42,42,41,44,41,41,46,46,47,56,56,54,44,39,41,43,47,50,43,49,53,52,55,62,66,73,77,68,63,73,79,91,106,129,115,108,119,132,131,121,122,131,124,136,145,151,141,149,165,177,174,165,174,157,148,136,162,157,138,153,148,129,144,147,164,169,177,193,196,187,171,170,189,192,184,180,173 } 
    }; 

    ticker.stocks[2] = (stock_t){
        .name = "NVIDIA Corporation", 
        .symbol = "NVDA", 
        .open_price = { 8,8,7,7,8,8,11,11,14,15,17,17,23,26,27,25,27,26,36,36,40,42,45,52,49,48,59,60,57,56,63,58,61,70,71,53,43,32,36,39,45,45,33,43,42,41,43,49,54,59,58,69,63,71,88,95,107,134,137,126,134,131,130,138,135,151,162,201,197,224,207,256,332,298,251,242,273,185,187,148,181,142,123,138,169,148,196,231,275,278,384,425,464,497,440,408,465,492,621,800 }, 
        .close_price = { 8,7,7,8,8,11,11,14,15,17,17,23,26,27,25,27,26,36,36,40,42,44,51,50,48,61,60,57,56,63,59,61,70,70,52,40,33,35,38,44,45,33,41,42,41,43,50,54,58,59,67,65,73,88,94,106,133,135,125,134,130,129,137,133,150,162,200,194,223,207,255,326,294,244,243,272,185,186,151,181,150,121,134,169,146,195,232,277,277,378,423,467,493,434,407,467,495,615,791,879 } 
    }; 

    ticker.stocks[3] = (stock_t){
        .name = "Amazon.com, Inc.", 
        .symbol = "AMZN", 
        .open_price = { 33,32,28,27,29,33,36,35,37,38,41,39,37,37,41,42,44,46,49,48,49,49,48,55,58,58,72,75,70,78,81,84,89,101,101,81,88,73,81,82,90,96,88,96,93,88,87,89,90,93,100,95,96,116,122,137,159,174,160,153,159,163,162,156,155,174,162,171,167,174,164,168,177,167,150,152,164,122,122,106,134,126,113,103,96,85,102,93,102,104,120,130,133,139,127,133,146,151,155,176 }, 
        .close_price = { 33,29,27,29,32,36,35,37,38,41,39,37,37,41,42,44,46,49,48,49,49,48,55,58,58,72,75,72,78,81,84,88,100,100,79,84,75,85,81,89,96,88,94,93,88,86,88,90,92,100,94,97,123,122,137,158,172,157,151,158,162,160,154,154,173,161,172,166,173,164,168,175,166,149,153,162,124,120,106,134,126,113,102,96,84,103,94,103,105,120,130,133,138,127,133,146,151,155,176,178 } 
    }; 

    ticker.stocks[4] = (stock_t){
        .name = "Meta Platforms, Inc.", 
        .symbol = "META", 
        .open_price = { 104,101,112,107,113,117,118,114,123,126,128,131,118,116,132,136,141,151,151,151,169,172,171,182,176,177,188,179,157,172,193,193,173,173,163,151,143,128,165,162,167,194,175,195,194,184,179,192,202,206,203,194,161,201,224,228,252,294,265,264,279,274,259,260,298,326,330,346,358,379,341,326,330,338,314,209,224,201,196,160,157,163,137,94,119,122,148,174,208,238,265,286,317,299,302,301,325,351,393,492 }, 
        .close_price = { 104,112,106,114,117,118,114,123,126,128,130,118,115,130,135,142,150,151,150,169,171,170,180,177,176,186,178,159,172,191,194,172,175,164,151,140,131,166,161,166,193,177,193,194,185,178,191,201,205,201,192,166,204,225,227,253,293,261,263,276,273,258,257,294,325,328,347,356,379,339,323,324,336,313,211,222,200,193,161,159,162,135,93,118,120,148,174,211,240,264,286,318,295,300,301,327,353,390,490,491 } 
    }; 

    ticker.stocks[5] = (stock_t){
        .name = "Alphabet Inc.", 
        .symbol = "GOOGL", 
        .open_price = { 38,38,38,36,37,35,37,35,39,39,40,40,38,40,41,42,42,46,49,46,47,47,48,51,51,52,58,55,51,50,55,55,61,61,60,54,56,51,56,56,59,59,53,55,60,59,61,63,65,67,73,67,56,66,71,70,74,81,74,81,88,88,92,102,104,118,118,121,135,145,134,148,144,145,137,134,139,113,114,107,115,108,96,95,101,89,98,89,102,106,122,119,130,137,131,124,131,138,142,138 }, 
        .close_price = { 38,38,35,38,35,37,35,39,39,40,40,38,39,41,42,42,46,49,46,47,47,48,51,51,52,59,55,51,50,55,56,61,61,60,54,55,52,56,56,58,59,55,54,60,59,61,62,65,66,71,66,58,67,71,70,74,81,73,80,87,87,91,101,103,117,117,122,134,144,133,148,141,144,135,135,139,114,113,108,116,108,95,94,100,88,98,90,103,107,122,119,132,136,130,124,132,139,140,138,143 } 
    }; 

    ticker.stocks[6] = (stock_t){
        .name = "Berkshire Hathaway Inc.", 
        .symbol = "BRK-B", 
        .open_price = { 134,130,128,135,141,145,140,144,144,150,144,144,157,164,164,173,166,165,165,170,175,181,183,188,193,198,214,206,199,193,192,186,198,209,215,205,221,201,206,203,202,217,197,214,205,201,208,213,220,227,225,207,176,185,185,178,197,216,214,204,230,231,229,246,255,278,291,278,279,286,273,288,279,300,312,320,353,324,316,272,299,279,269,298,319,310,309,304,309,329,321,340,352,362,349,341,359,356,384,409 }, 
        .close_price = { 132,129,134,141,145,140,144,144,150,144,144,157,162,164,171,166,165,165,169,174,181,183,186,193,198,214,207,199,193,191,186,197,208,214,205,218,204,205,201,200,216,197,213,205,203,208,212,220,226,224,206,182,187,185,178,195,218,212,201,228,231,227,240,255,274,289,277,278,285,272,287,276,299,313,321,352,322,315,273,300,280,267,295,318,308,311,305,308,328,321,341,351,360,350,341,360,356,383,409,406 } 
    }; 

    ticker.stocks[7] = (stock_t){
        .name = "Eli Lilly and Company", 
        .symbol = "LLY", 
        .open_price = { 84,83,78,72,71,75,74,78,83,77,80,73,67,73,77,83,84,82,79,82,82,81,85,82,84,84,81,77,76,80,85,85,98,105,107,108,118,114,120,127,130,117,116,111,109,112,111,114,117,131,140,127,134,153,154,164,152,148,148,132,146,169,209,205,186,182,200,229,245,258,231,255,249,274,247,247,286,291,313,323,327,301,325,345,374,366,342,310,343,397,430,466,455,556,536,555,591,580,647,769 }, 
        .close_price = { 84,79,72,72,75,75,78,82,77,80,73,67,73,77,82,84,82,79,82,82,81,85,81,84,84,81,77,77,81,85,85,98,105,107,108,118,115,119,126,129,117,115,110,108,112,111,113,117,131,139,126,138,154,152,164,150,148,148,130,145,168,207,204,186,182,199,229,243,258,231,254,248,276,245,249,286,292,313,324,329,301,323,362,371,365,344,311,343,395,429,468,454,554,537,553,591,582,645,753,760 } 
    }; 

    ticker.stocks[8] = (stock_t){
        .name = "Broadcom Inc.", 
        .symbol = "AVGO", 
        .open_price = { 131,142,133,135,154,146,153,154,162,176,173,170,170,178,202,213,219,222,241,234,248,252,245,264,275,259,241,245,233,230,254,240,223,218,247,225,245,248,269,277,303,320,252,301,289,280,278,292,317,319,305,276,227,266,290,315,318,350,368,355,403,439,455,479,472,459,475,477,489,496,487,530,563,666,585,584,631,557,587,479,531,491,449,475,551,565,583,594,639,626,800,868,898,901,829,842,922,1092,1187,1325 }, 
        .close_price = { 145,133,133,154,145,154,155,161,176,172,170,170,176,199,210,218,220,239,233,246,252,242,263,277,256,248,246,235,229,252,242,221,219,246,223,237,254,268,275,300,318,251,287,289,282,276,292,316,316,305,272,237,271,291,315,316,347,364,349,401,437,450,469,463,456,472,476,485,497,484,531,553,665,585,587,629,554,580,485,535,499,444,470,551,559,585,594,641,626,807,867,898,922,830,841,925,1116,1180,1300,1262 } 
    }; 

    ticker.stocks[9] = (stock_t){
        .name = "JPMorgan Chase & Co.", 
        .symbol = "JPM", 
        .open_price = { 67,63,59,56,59,63,64,61,64,67,66,69,80,87,85,92,87,87,82,91,92,91,95,101,104,107,115,115,109,108,108,103,115,114,113,109,112,95,104,105,102,115,105,113,115,108,118,126,132,139,132,116,85,93,97,94,97,99,97,99,120,127,129,149,151,154,165,156,152,160,164,172,161,159,148,140,137,119,132,112,114,113,105,126,138,135,138,142,129,142,136,146,157,146,144,139,155,169,173,185 }, 
        .close_price = { 66,59,56,59,63,65,62,63,67,66,69,80,86,84,90,87,87,82,91,91,90,95,100,104,106,115,115,109,108,107,104,114,114,112,109,111,97,103,104,101,116,105,111,116,109,117,124,131,139,132,116,90,95,97,94,96,100,96,98,117,127,128,147,152,153,164,155,151,159,163,169,158,158,148,141,136,119,132,112,115,113,104,125,138,134,139,143,130,138,135,145,157,146,145,139,156,170,174,186,187 } 
    }; 

    ticker.stocks[10] = (stock_t){
        .name = "Tesla, Inc.", 
        .symbol = "TSLA", 
        .open_price = { 15,15,12,12,16,16,14,13,15,13,14,13,12,14,16,16,19,20,22,24,21,23,22,22,20,20,23,23,17,19,19,24,19,19,20,22,24,20,20,20,18,15,12,15,16,14,16,21,21,28,44,47,33,50,57,72,96,167,146,131,199,239,271,230,229,234,209,227,233,244,259,381,386,382,311,289,360,286,251,227,301,272,254,234,197,118,173,206,199,163,202,276,266,257,244,204,233,250,188,200 }, 
        .close_price = { 16,12,12,15,16,14,14,15,14,13,13,12,14,16,16,18,20,22,24,21,23,22,22,20,20,23,22,17,19,18,22,19,20,17,22,23,22,20,21,18,15,12,14,16,15,16,20,21,27,43,44,34,52,55,71,95,166,143,129,189,235,264,225,222,236,208,226,229,245,258,371,381,352,312,290,359,290,252,224,297,275,265,227,194,123,173,205,207,164,203,261,267,258,250,200,240,248,187,201,162 } 
    }; 

    ticker.stocks[11] = (stock_t){
        .name = "UnitedHealth Group Incorporated", 
        .symbol = "UNH", 
        .open_price = { 113,116,114,119,128,132,133,141,143,136,139,141,159,161,162,166,164,175,175,186,193,199,196,211,228,221,235,225,218,237,243,245,256,268,267,262,283,245,268,243,249,233,241,245,249,231,219,253,281,293,275,257,238,288,304,295,303,310,312,312,344,351,335,334,372,401,413,402,413,416,391,461,452,500,475,470,510,510,498,512,542,519,507,555,552,525,499,473,485,494,487,478,507,479,505,529,550,526,508,489 }, 
        .close_price = { 117,115,119,128,131,133,141,143,136,140,141,158,160,162,165,164,174,175,185,191,198,195,210,228,220,236,226,214,236,241,245,253,268,266,261,281,249,270,242,247,233,241,244,249,234,217,252,279,293,272,254,249,292,304,294,302,312,311,305,336,350,333,332,372,398,411,400,412,416,390,460,444,502,472,475,509,508,496,513,542,519,505,555,547,530,499,475,472,492,487,480,506,476,504,535,552,526,511,493,489 } 
    }; 

    ticker.stocks[12] = (stock_t){
        .name = "Visa Inc.", 
        .symbol = "V", 
        .open_price = { 79,76,74,72,76,77,78,74,78,81,82,82,77,78,82,88,89,91,95,94,100,104,105,110,112,114,124,123,119,126,131,131,137,146,150,139,145,130,135,149,157,165,161,175,179,180,173,180,184,189,199,186,156,174,194,193,191,212,202,184,212,220,195,214,213,234,229,234,246,229,224,213,196,217,226,214,223,211,212,196,208,198,179,208,217,209,229,219,225,232,222,237,237,247,229,236,255,259,273,283 }, 
        .close_price = { 77,74,72,76,77,78,74,78,80,82,82,77,78,82,87,88,91,95,93,99,103,105,109,112,114,124,122,119,126,130,132,136,146,150,137,141,131,135,148,156,164,161,173,178,180,172,178,184,187,198,181,161,178,195,193,190,211,199,181,210,218,193,212,211,233,227,233,246,229,222,211,193,216,226,216,221,213,212,196,212,198,177,207,217,207,230,219,225,232,221,237,237,245,230,235,256,260,273,282,286 } 
    }; 

    ticker.stocks[13] = (stock_t){
        .name = "Exxon Mobil Corporation", 
        .symbol = "XOM", 
        .open_price = { 81,77,76,80,82,88,88,93,88,86,86,83,87,90,84,81,82,81,80,80,80,76,81,83,83,83,87,75,74,77,81,81,80,80,85,79,80,67,74,79,81,79,71,77,73,67,70,68,68,70,61,52,36,45,45,44,42,39,33,33,38,41,45,56,56,57,59,64,57,54,59,65,60,61,76,78,81,85,97,86,94,94,90,112,111,109,115,109,113,115,101,107,106,112,117,106,102,100,103,105 }, 
        .close_price = { 77,77,80,83,88,89,93,88,87,87,83,87,90,83,81,82,81,80,80,80,76,81,83,83,83,87,75,74,77,81,82,81,80,85,79,79,68,73,79,80,80,70,76,74,68,70,67,68,69,62,51,37,46,45,44,42,39,34,32,38,41,44,54,55,57,58,63,57,54,58,64,59,61,75,78,82,85,96,85,96,95,87,110,111,110,116,109,109,118,102,107,107,111,117,105,102,99,102,104,111 } 
    }; 

    ticker.stocks[14] = (stock_t){
        .name = "Johnson & Johnson", 
        .symbol = "JNJ", 
        .open_price = { 101,101,103,105,108,112,112,121,125,119,118,114,111,115,112,122,124,123,128,132,133,132,130,139,139,139,137,129,127,126,120,121,132,134,138,140,145,128,134,137,139,140,131,140,130,127,130,132,137,145,149,134,127,149,147,140,146,153,149,138,146,157,165,161,162,163,170,164,172,172,161,163,156,170,171,163,177,180,179,177,174,161,164,174,179,176,162,153,154,163,154,164,166,161,155,149,156,156,158,161 }, 
        .close_price = { 102,104,105,108,112,112,121,125,119,118,115,111,115,113,122,124,123,128,132,132,132,130,139,139,139,138,129,128,126,119,121,132,134,138,139,146,129,133,136,139,141,131,139,130,128,129,132,137,145,148,134,131,150,148,140,145,153,148,137,144,157,163,158,164,162,169,164,172,173,161,162,155,171,172,164,177,180,179,177,174,161,163,173,178,176,163,153,155,163,155,165,167,161,155,148,154,156,158,161,159 } 
    }; 

    ticker.stocks[15] = (stock_t){
        .name = "Mastercard Incorporated", 
        .symbol = "MA", 
        .open_price = { 98,95,88,87,93,97,95,89,95,96,101,107,102,104,106,111,112,116,122,122,128,133,141,149,150,152,172,176,174,178,192,195,199,215,224,198,206,185,211,226,238,254,251,269,273,279,271,279,290,300,318,298,230,268,300,295,311,357,342,294,339,358,320,360,357,385,364,366,389,347,349,335,320,359,385,357,359,363,358,314,347,323,287,332,357,349,368,354,362,380,367,391,393,413,393,378,412,424,455,474 }, 
        .close_price = { 97,89,86,94,96,95,88,95,96,101,107,102,103,106,110,112,116,122,121,127,133,141,148,150,151,169,175,175,178,190,196,198,215,222,197,201,188,211,224,235,254,251,264,272,281,271,276,292,298,315,290,241,274,300,295,308,358,338,288,336,356,316,353,356,382,360,365,385,346,347,335,314,359,386,360,357,363,357,315,353,324,284,328,356,347,370,355,363,380,365,393,394,412,395,376,413,426,449,474,479 } 
    }; 

    ticker.stocks[16] = (stock_t){
        .name = "The Procter & Gamble Company", 
        .symbol = "PG", 
        .open_price = { 74,78,81,80,82,80,80,84,85,87,89,86,82,83,87,91,89,87,88,87,91,92,91,86,90,91,86,78,79,72,73,77,80,82,83,88,94,91,96,98,104,106,103,109,118,119,124,124,121,124,124,113,107,117,116,119,130,137,139,138,139,139,129,124,135,134,135,135,141,142,139,143,144,161,160,154,153,161,148,144,138,137,127,134,149,150,142,138,148,156,143,151,155,154,144,150,153,146,156,158 }, 
        .close_price = { 79,81,80,82,80,81,84,85,87,89,86,82,84,87,91,89,87,88,87,90,92,90,86,89,91,86,78,79,72,73,78,80,82,83,88,94,91,96,98,104,106,102,109,118,120,124,124,122,124,124,113,110,117,115,119,131,138,138,137,138,139,128,123,135,133,134,134,142,142,139,142,144,163,160,155,152,160,147,143,138,137,126,134,149,151,142,137,148,156,142,151,156,154,145,150,153,146,157,158,161 } 
    }; 

    ticker.stocks[17] = (stock_t){
        .name = "The Home Depot, Inc.", 
        .symbol = "HD", 
        .open_price = { 133,130,124,124,133,134,132,128,138,134,128,121,129,135,137,146,146,156,153,154,150,150,164,166,180,190,199,182,177,184,187,193,196,200,208,176,183,169,184,185,192,203,189,209,214,226,233,236,220,219,230,219,175,216,249,249,266,284,279,270,278,266,271,258,306,326,320,319,330,325,328,373,402,416,369,314,300,301,301,275,300,288,281,300,326,317,322,291,294,298,284,309,331,332,300,285,313,344,353,380 }, 
        .close_price = { 132,125,124,133,133,132,127,138,134,128,122,129,134,137,144,146,156,153,153,149,149,163,165,179,189,200,182,178,184,186,195,197,200,207,175,180,171,183,185,191,203,189,207,213,227,232,234,220,218,228,217,186,219,248,250,265,285,277,266,277,265,270,258,305,323,318,318,328,326,328,371,400,415,366,315,299,300,302,274,300,288,275,296,323,315,324,296,295,300,283,310,333,330,302,284,313,346,352,380,375 } 
    }; 

    ticker.stocks[18] = (stock_t){
        .name = "Advanced Micro Devices, Inc.", 
        .symbol = "AMD", 
        .open_price = { 2,2,2,2,2,3,4,5,6,7,6,7,8,11,10,15,14,13,11,12,13,13,12,11,10,10,13,12,9,10,13,14,18,25,30,18,22,18,24,23,26,28,28,31,30,30,29,34,39,46,46,47,44,51,53,52,78,91,83,75,92,92,86,85,80,81,81,94,105,111,102,119,160,145,116,122,110,85,102,75,95,82,64,61,78,66,78,78,96,91,117,115,114,107,102,98,119,144,169,197 }, 
        .close_price = { 2,2,2,2,3,4,5,6,7,6,7,8,11,10,14,14,13,11,12,13,13,12,10,10,10,13,12,10,10,13,14,18,25,30,18,21,18,24,23,25,27,27,30,30,31,28,33,39,45,47,45,45,52,53,52,77,90,81,75,92,91,85,84,78,81,80,93,106,110,102,120,158,143,114,123,109,85,101,76,94,84,63,60,77,64,75,78,98,89,118,113,114,105,102,98,121,147,167,192,187 } 
    }; 

    ticker.stocks[19] = (stock_t){
        .name = "Costco Wholesale Corporation", 
        .symbol = "COST", 
        .open_price = { 162,159,150,150,157,148,150,157,167,159,152,148,150,160,163,177,167,178,180,160,159,157,164,161,183,187,193,191,186,196,195,208,218,233,235,228,230,200,214,219,243,245,239,266,275,292,288,297,299,294,307,294,282,301,307,302,325,345,356,362,384,377,351,335,352,373,379,396,430,455,449,494,543,565,505,519,577,532,469,481,541,519,474,503,519,458,508,481,496,499,509,537,560,553,567,555,593,655,694,740 }, 
        .close_price = { 161,151,150,157,148,148,157,167,162,152,147,150,160,163,177,167,177,180,159,158,156,164,161,184,186,194,190,188,197,198,208,218,233,234,228,231,203,214,218,242,245,239,264,275,294,288,297,299,293,305,281,285,303,308,303,325,347,355,357,391,376,352,331,352,372,378,395,429,455,449,491,539,567,505,519,575,531,466,479,541,522,472,501,539,456,511,484,496,503,511,538,560,549,564,552,592,660,694,743,731 } 
    }; 

}

static void dates_init() {
    dates = (date_t){.str = {"Dec 2015","Jan 2016","Feb 2016","Mar 2016","Apr 2016","May 2016","Jun 2016","Jul 2016","Aug 2016","Sep 2016","Oct 2016","Nov 2016","Dec 2016","Jan 2017","Feb 2017","Mar 2017","Apr 2017","May 2017","Jun 2017","Jul 2017","Aug 2017","Sep 2017","Oct 2017","Nov 2017","Dec 2017","Jan 2018","Feb 2018","Mar 2018","Apr 2018","May 2018","Jun 2018","Jul 2018","Aug 2018","Sep 2018","Oct 2018","Nov 2018","Dec 2018","Jan 2019","Feb 2019","Mar 2019","Apr 2019","May 2019","Jun 2019","Jul 2019","Aug 2019","Sep 2019","Oct 2019","Nov 2019","Dec 2019","Jan 2020","Feb 2020","Mar 2020","Apr 2020","May 2020","Jun 2020","Jul 2020","Aug 2020","Sep 2020","Oct 2020","Nov 2020","Dec 2020","Jan 2021","Feb 2021","Mar 2021","Apr 2021","May 2021","Jun 2021","Jul 2021","Aug 2021","Sep 2021","Oct 2021","Nov 2021","Dec 2021","Jan 2022","Feb 2022","Mar 2022","Apr 2022","May 2022","Jun 2022","Jul 2022","Aug 2022","Sep 2022","Oct 2022","Nov 2022","Dec 2022","Jan 2023","Feb 2023","Mar 2023","Apr 2023","May 2023","Jun 2023","Jul 2023","Aug 2023","Sep 2023","Oct 2023","Nov 2023","Dec 2023","Jan 2024","Feb 2024","Mar 2024"}};
}

static void data_init() {
    news_init();
    stocks_init();
    dates_init();
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
    const static int N_ROWS_REQ = 13, N_COLS_REQ = 12;
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
        rprintf(buf1, buf, 4);
        x_pix = gl_get_char_width() * (x + 5);
        y_pix = module.line_height * (y + 1 + i); 
        gl_draw_string(x_pix, y_pix, buf1, (pct_change >= 0 ? GL_GREEN : GL_RED));
    }
}

static void draw_graph(int x, int y, int stock_ind) {
    // `stock_ind` = index of stock in ticker.stocks[]
    const static int N_ROWS_REQ = 14, N_COLS_REQ = 28;
    if (x + N_COLS_REQ >= module.ncols || y + N_ROWS_REQ >= module.nrows) {
        printf("Error: graph out of bounds\n");
        return;
    }

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
        // TODO: Add resize mechanism for volatile stocks like NVDA
        printf("WARNING: stock too volatile (max_interval_price - min_interval_price) = %d > 200\n", diff);
        printf("         displaying only portions of the stock graph\n");
        step_size = 20;
    }

    // draw title
    char buf[N_COLS_REQ + 1], buf1[N_COLS_REQ + 1];
    snprintf(buf, N_COLS_REQ + 1, "%s (%s)\n", ticker.stocks[stock_ind].name, ticker.stocks[stock_ind].symbol);
    gl_draw_string(gl_get_char_width() * x, module.line_height * y, buf, GL_AMBER);

    // draw axes
    // TODO: Implement gl_draw_vertical_line()
    int graph_min = min_interval_price / step_size * step_size;
    int graph_max = graph_min + N_PRICE_INTERVALS * step_size;
    for (int i = 0; i < N_PRICE_INTERVALS; i++) {
        buf[0] = '\0'; buf1[0] = '\0';
        snprintf(buf, N_COLS_REQ + 1, "%d-|", graph_max - i * step_size);
        rprintf(buf1, buf, 6);
        int x_pix = gl_get_char_width() * x, y_pix = module.line_height * (y + 1 + i);
        gl_draw_string(x_pix, y_pix, buf1, GL_WHITE);
    }
    char buf2[] = "     +--------------------"; 
    int x_pix = gl_get_char_width() * x, y_pix = module.line_height * (y + 1 + N_PRICE_INTERVALS);
    gl_draw_string(x_pix, y_pix, buf2, GL_WHITE);
    
    // draw box plot
    for (int i = 0; i <= end_time - start_time; i++) {
        int open_price = ticker.stocks[stock_ind].open_price[i + start_time];
        int close_price = ticker.stocks[stock_ind].close_price[i + start_time];
        int max_price = max(open_price, close_price), min_price = min(open_price, close_price);
        int bx = (x + 6 + 2 * i) * gl_get_char_width() + 4;
        int by = (y + 1) * module.line_height + (graph_max - max_price) * 20 / step_size;
        int w = 2 * (gl_get_char_width() - 4);
        int h = (max_price - min_price) * 20 / step_size;
        color_t color = (open_price >= close_price ? GL_RED : GL_GREEN);
        gl_draw_rect(bx, by, w, h, color);
    }
}

static void draw_all() {
    gl_clear(module.bg_color);
    draw_date(0, 0);
    draw_graph(12, 0, 2);
    draw_ticker(0, 3);
    draw_news(0, 16);
}

static void display() {
    gl_swap_buffer();
}

// Interrupt handlers
static void hstimer0_handler(uintptr_t pc, void *aux_data) {
    module.tick_10s = (module.tick_10s + 1) % 3;
    if (module.tick_10s == 0) {
        module.time++; // increases every 30 seconds, or 3 10-s ticks
        if (module.time == 100) {
            hstimer_interrupt_clear(HSTIMER0);
            hstimer_disable(HSTIMER0);
            interrupts_register_handler(INTERRUPT_SOURCE_HSTIMER0, NULL, NULL);
            interrupts_disable_source(INTERRUPT_SOURCE_HSTIMER0);
            memory_report();
            return;
        }
        ticker.top = 0;
        news.top = 0;
        return;
    }
    ticker.top += N_TICKER_DISPLAY;
    if (ticker.top >= ticker.n) {
        ticker.top = 0;
    }
    news.top += N_NEWS_DISPLAY;
    if (news.top >= news.n) {
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

    // TODO: Test Floating Point
    float f = 0.324;
    printf("%f\n", f);
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
    draw_all();
    display();

    // interrupt settings
    hstimer_init(HSTIMER0, 1000000);
    hstimer_enable(HSTIMER0);
    interrupts_enable_source(INTERRUPT_SOURCE_HSTIMER0);
    interrupts_register_handler(INTERRUPT_SOURCE_HSTIMER0, hstimer0_handler, NULL);
    // for 30s: track # calls mod 3 with global variable tick_10s

    interrupts_global_enable(); // everything fully initialized, now turn on interrupts
}

void main(void) {
    interface_init(30, 80);
    while (1) {
    }
}

