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
        .open_price = { 54.4,54.3,54.9,51.0,55.0,50.0,52.4,51.1,56.6,57.0,57.4,60.0,60.1,62.8,64.4,64.1,65.8,68.7,70.2,69.3,73.1,74.7,74.7,83.7,83.6,86.1,94.8,94.0,90.5,93.2,99.3,98.1,106.0,110.8,114.8,107.1,113.0,99.6,103.8,112.9,118.9,130.5,123.8,136.6,137.0,136.6,139.7,144.3,151.8,158.8,170.4,165.3,153.0,175.8,182.5,203.1,211.5,225.5,213.5,204.3,214.5,222.5,235.1,235.9,238.5,253.4,251.2,269.6,286.4,302.9,282.1,331.4,335.1,335.4,310.4,296.4,309.4,277.7,275.2,256.4,277.8,258.9,235.4,234.6,253.9,243.1,248.0,250.8,286.5,307.0,325.9,339.2,335.2,331.3,316.3,339.8,376.8,373.9,401.8,411.3 }, 
        .close_price = { 55.5,55.1,50.9,55.2,49.9,53.0,51.2,56.7,57.5,57.6,59.9,60.3,62.1,64.7,64.0,65.9,68.5,69.8,68.9,72.7,74.8,74.5,83.2,84.2,85.5,95.0,93.8,91.3,93.5,98.8,98.6,106.1,112.3,114.4,106.8,110.9,101.6,104.4,112.0,117.9,130.6,123.7,134.0,136.3,137.9,139.0,143.4,151.4,157.7,170.2,162.0,157.7,179.2,183.2,203.5,205.0,225.5,210.3,202.5,214.1,222.4,232.0,232.4,235.8,252.2,249.7,270.9,284.9,301.9,281.9,331.6,330.6,336.3,311.0,298.8,308.3,277.5,271.9,256.8,280.7,261.5,232.9,232.1,255.1,239.8,247.8,249.4,288.3,307.3,328.4,340.5,335.9,327.8,315.8,338.1,378.9,376.0,397.6,413.6,417.3 }, 
        .high_price = { 56.8,55.4,55.1,55.6,56.8,53.0,53.0,57.3,58.7,58.2,61.4,61.4,64.1,65.9,65.2,66.2,69.1,70.7,72.9,74.4,75.0,76.0,86.2,85.1,87.5,95.4,96.1,97.2,97.9,100.0,102.7,111.2,112.8,115.3,116.2,112.2,113.4,107.9,113.2,120.8,131.4,130.6,138.4,141.7,140.9,142.4,145.7,152.5,159.6,174.1,190.7,175.0,180.4,187.5,204.4,216.4,231.1,232.9,225.2,228.1,227.2,242.6,246.1,241.1,263.2,254.4,271.6,290.1,305.8,305.3,332.0,349.7,344.3,338.0,315.1,316.0,315.1,290.9,277.7,282.0,294.2,267.5,251.0,255.3,263.9,249.8,276.8,289.3,308.9,335.9,351.5,366.8,338.5,340.9,346.2,384.3,378.2,415.3,420.8,427.8 }, 
        .low_price = { 53.7,49.1,48.2,50.6,49.3,49.5,48.0,50.4,56.1,55.6,56.3,57.3,58.8,62.0,62.8,63.6,64.8,67.1,68.1,68.0,71.3,72.9,73.7,82.2,80.7,85.5,83.8,87.1,87.5,92.4,97.3,98.0,104.8,107.2,100.1,99.3,94.0,97.2,102.3,108.8,118.1,123.0,119.0,134.7,130.8,134.5,133.2,143.0,146.6,156.5,152.0,132.5,150.4,173.8,181.4,197.5,203.1,196.2,199.6,200.1,209.1,211.9,227.9,224.3,238.1,238.1,243.0,269.6,283.7,281.6,280.2,326.4,317.2,276.0,271.5,270.0,270.0,246.4,241.5,245.9,260.7,232.7,219.1,213.4,233.9,219.4,245.5,245.6,275.4,303.4,322.5,327.0,311.5,309.5,311.2,339.6,362.9,366.5,397.2,398.4 } 
    }; 

    ticker.stocks[1] = (stock_t){
        .name = "Apple Inc.", 
        .symbol = "AAPL", 
        .open_price = { 29.7,25.7,24.1,24.4,27.2,23.5,24.8,23.9,26.1,26.5,28.2,28.4,27.6,29.0,31.8,34.5,35.9,36.3,38.3,36.2,37.3,41.2,38.6,42.5,42.5,42.5,41.8,44.6,41.7,41.6,47.0,46.0,49.8,57.1,57.0,54.8,46.1,38.7,41.7,43.6,47.9,52.5,43.9,50.8,53.5,51.6,56.3,62.4,66.8,74.1,76.1,70.6,61.6,71.6,79.4,91.3,108.2,132.8,117.6,109.1,121.0,133.5,133.8,123.8,123.7,132.0,125.1,136.6,146.4,152.8,141.9,149.0,167.5,177.8,174.0,164.7,174.0,156.7,149.9,136.0,161.0,156.6,138.2,155.1,148.2,130.3,144.0,146.8,164.3,169.3,177.7,193.8,196.2,189.5,171.2,171.0,190.3,187.1,184.0,179.6 }, 
        .close_price = { 26.3,24.3,24.2,27.2,23.4,25.0,23.9,26.1,26.5,28.3,28.4,27.6,29.0,30.3,34.2,35.9,35.9,38.2,36.0,37.2,41.0,38.5,42.3,43.0,42.3,41.9,44.5,41.9,41.3,46.7,46.3,47.6,56.9,56.4,54.7,44.6,39.4,41.6,43.3,47.5,50.2,43.8,49.5,53.3,52.2,56.0,62.2,66.8,73.4,77.4,68.3,63.6,73.4,79.5,91.2,106.3,129.0,115.8,108.9,119.1,132.7,132.0,121.3,122.2,131.5,124.6,137.0,145.9,151.8,141.5,149.8,165.3,177.6,174.8,165.1,174.6,157.6,148.8,136.7,162.5,157.2,138.2,153.3,148.0,129.9,144.3,147.4,164.9,169.7,177.2,194.0,196.4,187.9,171.2,170.8,189.9,192.5,184.4,180.8,173.7 }, 
        .high_price = { 30.0,26.5,24.7,27.6,28.1,25.2,25.5,26.1,27.6,29.0,29.7,28.4,29.5,30.6,34.4,36.1,36.4,39.2,39.0,38.5,41.1,41.2,42.4,44.1,44.3,45.0,45.2,45.9,44.7,47.6,48.5,49.0,57.2,57.4,58.4,55.6,46.2,42.2,44.0,49.4,52.1,53.8,50.4,55.3,54.5,56.6,62.4,67.0,73.5,82.0,81.8,76.0,73.6,81.1,93.1,106.4,131.0,138.0,125.4,122.0,138.8,145.1,137.9,128.7,137.1,134.1,137.4,150.0,153.5,157.3,153.2,165.7,182.1,182.9,176.6,179.6,178.5,166.5,151.7,163.6,176.1,164.3,157.5,155.4,150.9,147.2,157.4,165.0,169.9,179.4,194.5,198.2,196.7,190.0,182.3,192.9,199.6,196.4,191.1,180.5 }, 
        .low_price = { 26.2,23.1,23.1,24.4,23.1,22.4,22.9,23.6,26.0,25.6,28.1,26.0,27.1,28.7,31.8,34.3,35.0,36.1,35.5,35.6,37.1,37.3,38.1,41.3,41.6,41.2,37.6,41.2,40.2,41.3,45.2,45.9,49.3,53.8,51.5,42.6,36.6,35.5,41.5,42.4,47.1,43.7,42.6,49.6,48.1,51.1,53.8,62.3,64.1,73.2,64.1,53.2,59.2,71.5,79.3,89.1,107.9,103.1,107.7,107.3,120.0,126.4,118.4,116.2,122.5,122.2,123.1,135.8,144.5,141.3,138.3,147.5,157.8,154.7,152.0,150.1,155.4,132.6,129.0,135.7,157.1,138.0,134.4,134.4,125.9,124.2,141.3,143.9,159.8,164.3,176.9,186.6,172.0,167.6,165.7,170.1,187.4,180.2,179.2,168.5 } 
    }; 

    ticker.stocks[2] = (stock_t){
        .name = "NVIDIA Corporation", 
        .symbol = "NVDA", 
        .open_price = { 8.0,8.1,7.3,7.9,8.9,9.0,11.6,11.7,14.3,15.3,17.1,17.9,23.0,26.1,27.6,25.9,27.2,26.2,36.2,36.3,40.5,42.5,45.2,52.3,49.8,48.9,59.6,60.5,57.2,56.1,63.5,58.5,61.5,70.0,71.0,53.1,43.2,32.7,36.1,39.1,45.8,45.8,34.0,43.1,42.3,41.1,43.8,49.9,54.1,59.7,58.9,69.2,63.9,71.1,88.3,95.2,107.3,134.8,137.6,126.6,134.9,131.0,130.5,138.8,135.7,151.2,162.7,201.2,197.0,224.9,207.5,256.5,332.2,298.1,251.0,242.9,273.8,185.4,187.2,149.0,181.8,142.1,123.5,138.1,170.0,148.5,196.9,231.9,275.1,278.4,384.9,425.2,464.6,497.6,440.3,408.8,465.2,492.4,621.0,800.0 }, 
        .close_price = { 8.2,7.3,7.8,8.9,8.9,11.7,11.8,14.3,15.3,17.1,17.8,23.0,26.7,27.3,25.4,27.2,26.1,36.1,36.1,40.6,42.4,44.7,51.7,50.2,48.4,61.5,60.5,57.9,56.2,63.0,59.2,61.2,70.2,70.3,52.7,40.9,33.4,35.9,38.6,44.9,45.2,33.9,41.1,42.2,41.9,43.5,50.3,54.2,58.8,59.1,67.5,65.9,73.1,88.8,95.0,106.1,133.7,135.3,125.3,134.0,130.6,129.9,137.1,133.5,150.1,162.4,200.0,195.0,223.9,207.2,255.7,326.8,294.1,244.9,243.9,272.9,185.5,186.7,151.6,181.6,150.9,121.4,135.0,169.2,146.1,195.4,232.2,277.8,277.5,378.3,423.0,467.3,493.5,435.0,407.8,467.7,495.2,615.3,791.1,884.5 }, 
        .high_price = { 8.5,8.4,8.0,9.1,9.4,11.7,12.1,14.3,15.9,17.3,18.2,23.8,30.0,28.0,30.2,27.5,27.4,36.8,42.1,42.5,43.6,47.8,52.0,54.7,50.1,62.3,63.0,63.6,59.8,65.1,67.3,64.2,70.4,71.3,73.2,55.5,43.7,40.2,41.3,46.2,48.4,46.2,41.3,44.7,43.4,47.1,52.2,55.4,60.5,64.9,79.1,71.2,76.1,91.8,96.4,107.9,135.8,147.3,143.5,146.9,137.3,140.0,153.7,139.2,162.1,162.8,201.6,208.8,230.4,229.9,257.1,346.5,332.9,307.1,269.2,289.5,275.6,204.0,196.2,182.4,192.7,145.5,138.5,170.0,187.9,206.3,238.9,278.3,281.1,419.4,439.9,480.9,502.7,498.0,476.1,505.5,504.3,634.9,823.9,974.0 }, 
        .low_price = { 8.0,6.6,6.2,7.8,8.7,8.6,11.1,11.5,13.9,14.3,15.9,16.6,21.2,24.8,23.9,23.8,23.9,25.6,35.5,34.6,38.2,40.7,44.2,47.8,45.1,48.6,51.0,54.2,52.6,55.5,58.8,58.3,59.7,64.7,44.0,33.3,31.1,31.9,35.6,36.2,43.3,33.8,33.2,38.8,36.8,40.8,42.5,49.6,50.1,57.8,58.9,45.2,59.6,70.2,86.6,94.1,107.2,117.0,123.0,123.9,127.6,125.9,129.0,115.7,135.1,134.6,159.0,178.7,187.6,204.7,195.6,252.3,271.5,208.9,208.9,206.5,182.9,155.7,148.6,140.6,149.6,119.5,108.1,129.6,138.8,140.3,196.1,223.0,262.2,272.4,373.6,413.5,403.1,409.8,392.3,408.7,450.1,473.2,616.5,794.3 } 
    }; 

    ticker.stocks[3] = (stock_t){
        .name = "Amazon.com, Inc.", 
        .symbol = "AMZN", 
        .open_price = { 33.7,32.8,28.9,27.8,29.5,33.2,36.0,35.9,38.0,38.5,41.8,40.0,37.6,37.9,41.5,42.7,44.4,46.4,49.9,48.6,49.8,49.2,48.2,55.3,58.6,58.6,72.2,75.7,70.9,78.2,81.9,84.1,89.2,101.3,101.1,81.2,88.5,73.3,81.9,82.8,90.0,96.7,88.0,96.1,93.6,88.5,87.3,89.4,90.2,93.8,100.5,95.3,96.6,116.8,122.4,137.9,159.0,174.5,160.4,153.1,159.4,163.5,162.1,156.4,155.9,174.2,162.2,171.7,167.7,174.8,164.5,168.1,177.2,167.6,150.0,152.7,164.1,122.4,122.3,106.3,135.0,126.0,113.6,104.0,97.0,85.5,102.5,93.9,102.3,104.9,120.7,130.8,133.6,139.5,127.3,134.0,146.0,151.5,155.9,176.8 }, 
        .close_price = { 33.8,29.4,27.6,29.7,33.0,36.1,35.8,37.9,38.5,41.9,39.5,37.5,37.5,41.2,42.3,44.3,46.2,49.7,48.4,49.4,49.0,48.1,55.3,58.8,58.5,72.5,75.6,72.4,78.3,81.5,85.0,88.9,100.6,100.2,79.9,84.5,75.1,85.9,82.0,89.0,96.3,88.8,94.7,93.3,88.8,86.8,88.8,90.0,92.4,100.4,94.2,97.5,123.7,122.1,137.9,158.2,172.5,157.4,151.8,158.4,162.8,160.3,154.6,154.7,173.4,161.2,172.0,166.4,173.5,164.3,168.6,175.4,166.7,149.6,153.6,163.0,124.3,120.2,106.2,134.9,126.8,113.0,102.4,96.5,84.0,103.1,94.2,103.3,105.4,120.6,130.4,133.7,138.0,127.1,133.1,146.1,151.9,155.2,176.8,174.5 }, 
        .high_price = { 34.8,32.9,29.1,30.2,33.5,36.2,36.6,38.3,38.7,42.0,42.4,40.0,39.1,42.2,43.0,44.5,47.5,50.1,50.8,54.2,50.3,50.0,56.1,60.7,59.7,73.6,76.4,80.9,81.9,81.8,88.2,94.0,101.3,102.5,101.7,89.2,88.9,86.8,83.7,91.2,97.8,98.2,96.8,101.8,94.9,92.7,89.9,91.2,95.1,102.8,109.3,99.8,123.8,126.3,139.8,167.2,174.8,177.6,174.8,168.3,167.5,168.2,171.7,159.1,177.7,174.3,176.2,188.7,173.6,177.5,173.9,188.1,178.0,171.4,163.8,170.8,168.4,126.2,129.0,137.6,146.6,136.5,123.0,104.6,97.2,103.5,114.0,103.5,110.9,122.9,131.5,136.6,143.6,145.9,134.5,149.3,155.6,161.7,177.2,180.1 }, 
        .low_price = { 31.8,27.4,23.7,26.9,29.3,32.8,34.1,35.8,37.5,37.8,38.7,35.5,36.8,37.4,40.2,41.7,44.2,46.4,46.3,47.5,46.8,46.6,47.5,54.3,56.2,58.5,63.3,68.3,67.6,77.3,81.8,83.9,88.8,93.2,73.8,71.0,65.3,73.0,78.3,79.3,89.9,88.6,83.6,92.5,87.2,85.5,84.3,86.1,86.8,90.8,90.6,81.3,94.5,112.8,121.9,137.7,153.6,143.6,150.9,147.5,153.6,154.3,151.8,144.1,155.8,156.4,158.6,165.3,158.8,163.7,158.8,164.2,165.2,135.4,138.3,133.6,121.6,101.3,101.4,105.8,126.7,112.1,97.7,85.9,81.7,81.4,92.3,88.1,97.7,101.2,119.9,125.9,126.4,123.0,118.3,133.7,142.8,144.1,155.6,171.5 } 
    }; 

    ticker.stocks[4] = (stock_t){
        .name = "Meta Platforms, Inc.", 
        .symbol = "META", 
        .open_price = { 104.8,101.9,112.3,107.8,113.8,117.8,118.5,114.2,123.8,126.4,128.4,131.4,118.4,116.0,132.2,136.5,141.9,151.7,151.8,151.7,169.8,172.4,171.4,182.4,176.0,177.7,188.2,179.0,157.8,172.0,193.1,193.4,173.9,173.5,163.0,151.5,143.0,129.0,165.8,162.6,167.8,194.8,175.0,195.2,194.2,184.0,179.1,192.9,202.1,206.8,203.4,194.0,161.6,201.6,224.6,228.5,252.6,294.7,265.4,264.6,279.2,274.8,259.5,260.8,298.4,326.2,330.1,346.8,358.1,379.6,341.6,326.0,330.3,338.3,314.6,209.9,224.6,201.2,196.5,160.3,157.2,163.6,137.1,94.3,119.2,122.8,148.0,174.6,208.8,238.6,265.9,286.7,317.5,299.4,302.7,301.9,325.5,351.3,393.9,492.1 }, 
        .close_price = { 104.7,112.2,106.9,114.1,117.6,118.8,114.3,123.9,126.1,128.3,131.0,118.4,115.1,130.3,135.5,142.1,150.2,151.5,151.0,169.2,172.0,170.9,180.1,177.2,176.5,186.9,178.3,159.8,172.0,191.8,194.3,172.6,175.7,164.5,151.8,140.6,131.1,166.7,161.4,166.7,193.4,177.5,193.0,194.2,185.7,178.1,191.6,201.6,205.2,201.9,192.5,166.8,204.7,225.1,227.1,253.7,293.2,261.9,263.1,277.0,273.2,258.3,257.6,294.5,325.1,328.7,347.7,356.3,379.4,339.4,323.6,324.5,336.4,313.3,211.0,222.4,200.5,193.6,161.2,159.1,162.9,135.7,93.2,118.1,120.3,149.0,174.9,211.9,240.3,264.7,287.0,318.6,295.9,300.2,301.3,327.1,354.0,390.1,490.1,497.0 }, 
        .high_price = { 107.9,112.8,117.6,117.0,120.8,121.1,119.4,128.3,126.7,132.0,133.5,131.9,122.5,133.1,137.2,142.9,151.5,153.6,156.5,175.5,173.1,174.0,180.8,184.2,182.3,190.7,195.3,186.1,177.1,192.7,203.6,218.6,188.3,173.9,165.9,154.1,147.2,171.7,172.5,174.3,198.5,196.2,198.9,208.7,198.5,193.1,198.1,203.8,208.9,224.2,218.8,197.2,209.7,240.9,245.2,255.9,304.7,303.6,285.2,297.4,291.8,286.8,276.6,299.7,331.8,333.8,358.1,377.5,382.8,384.3,345.0,353.8,352.7,343.1,328.0,231.1,236.9,224.3,202.0,183.9,183.1,171.4,142.4,118.7,124.7,153.2,197.2,212.2,241.7,268.6,289.8,326.2,324.1,312.9,330.5,342.9,361.9,406.4,494.4,523.6 }, 
        .low_price = { 101.5,89.4,96.8,104.4,106.3,115.9,108.2,113.0,122.1,125.6,126.8,113.6,114.0,115.5,130.3,136.1,138.8,144.4,144.6,147.8,165.0,161.6,168.3,174.0,169.0,175.8,167.2,149.0,150.5,170.2,186.4,166.6,170.3,158.9,139.0,126.8,123.0,128.6,159.6,159.3,167.3,177.2,160.8,191.9,176.7,175.7,173.1,188.5,193.2,201.1,181.8,137.1,150.8,198.8,207.1,226.9,247.4,244.1,254.8,257.3,264.6,244.6,254.0,253.5,296.0,298.2,323.5,334.5,347.7,338.1,308.1,323.2,299.5,289.0,190.2,185.8,169.0,176.1,154.2,154.9,155.2,134.1,92.6,88.1,112.5,122.3,147.1,171.4,207.1,229.9,258.9,284.9,274.4,286.8,279.4,301.9,313.7,340.0,393.0,476.0 } 
    }; 

    ticker.stocks[5] = (stock_t){
        .name = "Alphabet Inc.", 
        .symbol = "GOOGL", 
        .open_price = { 38.3,38.1,38.6,36.1,37.9,35.6,37.4,35.3,39.3,39.6,40.1,40.5,38.9,40.0,41.2,42.6,42.4,46.2,49.5,46.7,47.4,47.9,48.8,51.8,51.5,52.7,58.8,55.5,51.4,50.8,55.6,55.8,62.0,61.1,60.7,54.6,56.6,51.4,56.1,56.5,59.4,59.9,53.3,55.1,60.9,59.1,61.1,63.3,65.1,67.4,73.1,67.6,56.2,66.2,71.3,71.0,74.6,81.6,74.2,81.2,88.3,88.0,92.2,102.4,104.6,118.2,118.7,121.7,135.1,145.0,134.4,148.0,144.0,145.1,137.6,134.9,139.5,113.4,114.9,107.9,115.3,108.3,96.8,95.4,101.0,89.6,98.7,90.0,102.4,106.8,122.8,119.2,130.8,137.5,131.2,124.1,131.9,138.6,142.1,138.4 }, 
        .close_price = { 38.9,38.1,35.9,38.1,35.4,37.4,35.2,39.6,39.5,40.2,40.5,38.8,39.6,41.0,42.2,42.4,46.2,49.4,46.5,47.3,47.8,48.7,51.7,51.8,52.7,59.1,55.2,51.9,50.9,55.0,56.5,61.4,61.6,60.4,54.5,55.5,52.2,56.3,56.3,58.8,59.9,55.3,54.1,60.9,59.5,61.1,62.9,65.2,67.0,71.6,67.0,58.1,67.3,71.7,70.9,74.4,81.5,73.3,80.8,87.7,87.6,91.4,101.1,103.1,117.7,117.8,122.1,134.7,144.7,133.7,148.0,141.9,144.9,135.3,135.1,139.1,114.1,113.8,109.0,116.3,108.2,95.7,94.5,101.0,88.2,98.8,90.1,103.7,107.3,122.9,119.7,132.7,136.2,130.9,124.1,132.5,139.7,140.1,138.5,147.7 }, 
        .high_price = { 39.9,38.5,40.5,38.9,39.5,37.7,37.6,40.2,40.7,41.0,42.0,40.8,41.2,43.3,42.7,43.7,46.8,50.0,50.4,50.3,47.9,48.8,53.2,54.0,54.3,59.9,59.4,58.9,54.9,55.9,60.1,64.6,63.6,61.4,61.2,55.5,56.8,56.4,57.7,61.8,64.8,60.0,56.3,63.4,61.8,62.4,65.0,66.7,68.4,75.0,76.5,70.4,68.0,72.3,73.8,79.4,82.6,86.3,84.1,90.8,92.2,96.6,107.3,105.7,121.6,119.5,123.1,138.3,146.0,146.3,148.6,151.0,149.1,146.5,151.5,143.8,143.7,122.9,119.3,119.7,122.4,111.6,104.8,101.0,102.2,100.3,108.2,106.6,109.2,126.4,129.0,133.7,138.0,139.2,141.2,139.4,142.7,153.8,149.4,152.1 }, 
        .low_price = { 36.8,34.4,34.1,35.2,35.2,35.2,33.6,35.0,39.3,39.2,39.8,37.2,37.7,39.8,40.6,41.2,41.7,46.0,46.5,45.8,45.9,46.2,48.1,51.4,50.1,52.7,49.8,49.2,49.7,50.4,55.3,55.3,60.2,57.6,50.4,50.1,48.9,51.1,54.7,56.5,59.2,55.2,51.4,54.8,57.1,58.2,58.2,63.0,63.9,67.3,63.4,50.4,53.8,64.8,67.6,70.7,73.2,70.1,71.7,80.6,84.7,84.8,92.2,99.7,104.6,109.7,116.5,121.5,133.3,133.6,131.1,141.6,139.3,124.5,125.0,125.3,112.7,101.9,105.0,104.1,107.8,95.6,91.8,83.3,85.9,84.9,88.6,89.4,101.9,103.7,116.1,115.3,126.4,127.2,120.2,123.7,127.9,135.1,135.4,130.7 } 
    }; 

    ticker.stocks[6] = (stock_t){
        .name = "Berkshire Hathaway Inc.", 
        .symbol = "BRK-B", 
        .open_price = { 134.9,130.2,128.9,135.1,141.2,145.8,141.0,144.6,144.6,150.7,144.3,144.7,157.6,164.3,164.8,173.7,166.7,165.8,165.8,170.4,175.9,181.6,183.4,188.1,193.6,198.9,214.5,206.8,199.0,193.8,192.9,186.1,198.8,209.2,215.9,205.6,222.0,201.7,206.5,203.1,202.2,217.2,197.6,214.2,205.6,201.2,208.9,213.6,220.6,227.5,225.5,207.2,176.2,185.2,185.5,178.4,197.3,216.9,214.3,204.8,230.2,231.7,230.0,246.9,255.7,278.5,291.5,278.2,279.3,286.6,273.0,288.0,279.5,300.1,312.6,320.3,353.6,324.1,316.0,272.5,299.7,280.0,269.5,298.5,319.0,310.1,309.6,304.0,309.2,329.2,321.4,340.8,352.0,362.0,349.6,341.2,359.9,356.3,384.0,409.5 }, 
        .close_price = { 132.0,129.8,134.2,141.9,145.5,140.5,144.8,144.3,150.5,144.5,144.3,157.4,163.0,164.1,171.4,166.7,165.2,165.3,169.4,175.0,181.2,183.3,186.9,193.0,198.2,214.4,207.2,199.5,193.7,191.5,186.6,197.9,208.7,214.1,205.3,218.2,204.2,205.5,201.3,200.9,216.7,197.4,213.2,205.4,203.4,208.0,212.6,220.3,226.5,224.4,206.3,182.8,187.4,185.6,178.5,195.8,218.0,212.9,201.9,228.9,231.9,227.9,240.5,255.5,275.0,289.4,277.9,278.3,285.8,272.9,287.0,276.7,299.0,313.0,321.5,352.9,322.8,316.0,273.0,300.6,280.8,267.0,295.1,318.6,308.9,311.5,305.2,308.8,328.5,321.1,341.0,352.0,360.2,350.3,341.3,360.0,356.7,383.7,409.4,408.4 }, 
        .high_price = { 136.7,131.8,135.1,143.4,148.0,147.1,146.0,147.0,150.9,151.1,145.7,159.1,167.2,165.3,172.2,177.9,168.9,168.0,171.9,175.6,181.9,184.0,190.7,193.8,200.5,217.6,217.5,213.4,202.8,202.4,196.7,201.4,211.3,223.0,224.1,223.5,223.6,208.0,209.4,207.8,217.3,219.2,213.3,216.6,207.0,214.6,213.7,223.4,228.2,231.6,230.1,218.8,197.2,187.3,203.3,196.7,219.4,223.2,217.4,235.0,232.3,236.2,250.6,267.5,277.8,295.1,293.3,282.2,291.8,287.1,292.2,295.6,301.6,324.4,325.6,362.1,354.6,327.3,316.8,302.4,308.1,289.2,300.0,319.1,319.6,321.3,314.1,317.3,328.8,333.9,342.5,352.3,364.6,373.3,350.0,363.2,364.0,387.9,430.0,410.6 }, 
        .low_price = { 129.5,123.9,123.6,134.3,140.3,139.7,136.6,140.9,142.9,143.4,141.9,142.4,157.5,158.6,162.1,165.8,162.3,160.9,164.8,168.0,174.8,172.6,182.9,180.4,189.7,196.0,189.3,191.9,192.0,188.6,184.8,185.7,196.8,208.0,197.3,203.4,186.1,191.0,198.2,197.0,202.0,197.1,196.9,204.4,195.4,200.2,201.3,213.4,216.4,221.1,199.7,159.5,174.2,167.0,174.6,177.3,196.0,206.6,197.8,203.1,221.3,226.1,228.0,243.2,254.8,276.8,272.5,270.7,276.8,271.4,272.2,275.9,274.8,294.8,299.5,313.6,320.5,298.1,263.7,271.2,280.4,261.5,259.9,282.4,297.0,303.9,300.0,292.4,307.1,317.4,319.5,338.4,349.4,348.5,330.6,340.6,350.9,355.9,381.5,398.8 } 
    }; 

    ticker.stocks[7] = (stock_t){
        .name = "Eli Lilly and Company", 
        .symbol = "LLY", 
        .open_price = { 84.2,83.4,78.2,72.6,71.6,75.9,74.9,78.9,83.0,77.8,80.0,73.8,67.3,73.9,77.9,83.4,84.1,82.1,79.6,82.4,82.9,81.5,85.8,82.2,85.0,84.5,81.5,77.1,76.9,80.7,85.2,85.1,98.7,105.3,107.7,108.7,118.6,114.8,120.3,127.2,130.7,117.1,116.5,111.3,109.0,112.5,111.9,114.1,117.5,131.8,140.5,127.7,134.0,153.7,154.5,164.3,152.8,148.6,148.3,132.5,146.7,169.0,209.5,205.8,186.8,183.0,200.3,229.5,245.7,258.5,231.0,255.1,249.4,274.4,247.1,247.6,286.1,291.2,313.4,323.9,327.5,301.0,326.0,345.8,374.8,366.3,342.8,310.0,343.2,397.3,430.3,466.3,455.4,556.3,536.0,555.0,591.7,580.4,647.3,769.0 }, 
        .close_price = { 84.3,79.1,72.0,72.0,75.5,75.0,78.8,82.9,77.8,80.3,73.8,67.1,73.6,77.0,82.8,84.1,82.1,79.6,82.3,82.7,81.3,85.5,81.9,84.6,84.5,81.4,77.0,77.4,81.1,85.0,85.3,98.8,105.7,107.3,108.4,118.6,115.7,119.9,126.3,129.8,117.0,115.9,110.8,108.9,113.0,111.8,113.9,117.3,131.4,139.6,126.1,138.7,154.6,152.9,164.2,150.3,148.4,148.0,130.5,145.6,168.8,208.0,204.9,186.8,182.8,199.7,229.5,243.5,258.3,231.1,254.8,248.0,276.2,245.4,249.9,286.4,292.1,313.4,324.2,329.7,301.2,323.4,362.1,371.1,365.8,344.1,311.2,343.4,395.9,429.5,469.0,454.5,554.2,537.1,553.9,591.0,582.9,645.6,753.7,762.7 }, 
        .high_price = { 88.2,85.4,79.2,74.9,78.5,78.7,78.8,83.6,83.8,81.5,83.2,79.7,74.5,78.1,83.5,86.1,86.7,83.1,84.8,85.5,83.2,85.6,89.1,85.5,89.1,88.3,83.0,80.5,83.6,85.1,87.3,99.2,106.5,107.8,116.6,118.7,119.8,120.1,127.8,132.1,131.4,119.5,118.9,115.5,116.1,117.2,114.7,118.5,137.0,143.7,147.9,144.0,164.9,162.4,167.4,170.8,157.5,154.5,157.1,152.0,173.9,218.0,209.9,212.2,193.5,203.6,239.4,248.4,275.9,261.0,256.8,271.1,283.9,274.4,252.9,295.3,314.0,324.1,330.9,335.3,330.4,341.7,363.9,372.4,375.2,369.0,353.8,343.6,404.3,455.0,469.9,467.6,557.8,601.8,630.0,625.9,602.0,663.5,794.5,800.8 }, 
        .low_price = { 82.6,76.5,70.4,67.9,71.5,73.3,71.9,78.4,77.5,76.4,73.8,64.2,65.7,73.5,76.6,81.4,79.9,76.8,78.8,80.8,76.9,79.1,81.7,81.4,84.2,81.3,73.7,74.5,75.4,77.1,84.4,84.7,97.8,103.7,104.2,104.9,105.7,111.1,116.9,122.0,113.7,113.9,110.0,105.2,107.4,106.4,101.4,110.5,115.9,130.2,121.5,117.1,133.0,143.6,139.7,148.7,147.0,145.1,129.2,130.0,143.1,161.8,195.6,179.8,178.6,182.9,196.7,228.7,243.5,220.2,224.2,246.5,239.5,232.7,231.9,245.4,276.8,283.0,283.1,315.5,296.5,296.3,317.0,340.1,354.6,339.4,309.6,309.2,342.3,392.3,428.1,434.3,446.9,532.2,516.6,551.3,561.7,579.0,643.2,727.6 } 
    }; 

    ticker.stocks[8] = (stock_t){
        .name = "Broadcom Inc.", 
        .symbol = "AVGO", 
        .open_price = { 131.2,142.1,133.3,135.4,154.0,146.2,153.4,154.5,162.8,176.9,173.0,170.7,170.0,178.3,202.2,213.1,219.0,222.1,241.6,234.5,248.2,252.3,245.3,264.2,275.2,259.8,241.7,245.9,233.9,230.9,254.4,240.1,223.3,218.6,247.9,225.2,245.2,248.9,269.1,277.7,303.1,320.5,252.6,301.7,289.1,280.0,278.3,293.0,317.7,319.3,305.6,276.8,228.0,266.0,290.4,315.1,318.0,350.0,369.0,355.0,403.5,439.3,455.9,479.7,472.1,459.8,475.4,477.8,489.0,496.3,487.9,530.3,563.5,666.3,585.9,584.8,631.7,557.1,587.6,479.4,531.4,491.5,449.2,475.8,551.0,565.0,583.6,594.0,639.0,626.5,800.6,868.6,899.0,901.9,829.1,842.0,922.5,1092.1,1187.3,1325.9 }, 
        .close_price = { 145.1,133.7,134.0,154.5,145.8,154.4,155.4,162.0,176.4,172.5,170.3,170.5,176.8,199.5,210.9,219.0,220.8,239.5,233.1,246.7,252.1,242.5,263.9,277.9,256.9,248.0,246.5,235.6,229.4,252.1,242.6,221.8,219.0,246.7,223.5,237.4,254.3,268.2,275.4,300.7,318.4,251.6,287.9,290.0,282.6,276.1,292.9,316.2,316.0,305.2,272.6,237.1,271.6,291.3,315.6,316.8,347.1,364.3,349.6,401.6,437.9,450.5,469.9,463.7,456.2,472.3,476.8,485.4,497.2,484.9,531.7,553.7,665.4,585.9,587.4,629.7,554.4,580.1,485.8,535.5,499.1,444.0,470.1,551.0,559.1,585.0,594.3,641.5,626.5,808.0,867.4,898.7,922.9,830.6,841.4,925.7,1116.2,1180.0,1300.5,1237.2 }, 
        .high_price = { 149.7,143.3,138.7,157.4,159.6,154.9,166.0,167.6,179.4,177.7,177.0,178.0,184.0,205.8,216.0,227.8,224.1,242.9,256.8,258.5,259.4,255.3,266.7,285.7,275.7,274.3,255.7,273.9,252.9,255.3,271.8,251.8,223.9,250.1,252.1,242.6,261.6,273.8,286.6,303.3,322.5,323.2,291.8,305.8,297.8,302.3,294.1,325.7,331.2,331.6,325.7,288.5,277.0,292.0,328.1,324.3,350.6,379.0,387.8,402.2,438.5,470.0,495.1,490.9,489.6,474.6,478.6,494.0,507.9,510.7,536.1,577.2,677.8,672.2,614.6,645.3,636.7,609.0,590.9,537.8,560.6,531.3,489.7,551.7,585.7,601.7,617.0,648.5,644.2,921.8,890.0,923.2,923.7,901.9,925.9,999.9,1151.8,1284.6,1319.6,1438.2 }, 
        .low_price = { 130.9,117.2,114.2,134.3,143.2,139.2,142.3,147.2,161.3,158.8,166.8,163.0,160.6,173.3,200.2,210.6,208.4,219.9,230.6,227.5,238.7,231.5,237.5,248.9,253.4,237.0,224.9,234.8,225.4,222.0,242.3,197.5,202.8,213.9,208.2,213.7,217.6,230.3,264.9,259.2,299.8,250.1,251.2,272.4,262.5,270.0,267.2,292.1,303.1,298.8,262.7,155.7,219.7,254.8,287.4,304.2,317.3,343.5,344.4,346.7,398.1,420.5,453.6,419.3,449.0,419.1,458.4,455.7,462.7,484.5,472.8,524.9,544.0,513.4,549.0,563.6,553.4,512.4,480.7,463.9,496.5,443.6,415.1,441.4,516.0,550.0,572.1,586.1,603.2,601.3,776.4,844.3,812.0,795.1,808.9,835.6,903.1,1041.5,1179.1,1230.1 } 
    }; 

    ticker.stocks[9] = (stock_t){
        .name = "JPMorgan Chase & Co.", 
        .symbol = "JPM", 
        .open_price = { 67.3,64.0,59.2,56.8,59.0,63.7,64.8,61.7,64.2,67.6,66.3,69.5,80.7,87.3,85.5,92.8,88.0,87.4,82.5,91.6,92.5,91.2,95.8,101.1,104.9,107.6,115.8,115.5,110.0,108.4,108.3,103.7,115.8,114.3,113.4,109.6,112.4,95.9,104.0,105.1,102.2,115.7,105.8,113.2,115.3,109.0,118.4,126.2,132.3,139.8,132.7,116.6,85.1,93.5,97.8,94.9,97.0,99.6,97.1,99.4,120.3,127.5,129.4,149.5,151.9,154.9,165.9,156.3,152.0,160.2,164.0,172.0,161.0,159.9,148.7,140.0,137.4,119.9,132.9,112.7,114.5,113.3,105.6,126.9,138.2,135.2,138.2,142.1,129.9,142.3,136.5,146.2,157.4,146.1,144.8,139.2,155.8,169.1,173.6,185.7 }, 
        .close_price = { 66.0,59.5,56.3,59.2,63.2,65.3,62.1,64.0,67.5,66.6,69.3,80.2,86.3,84.6,90.6,87.8,87.0,82.2,91.4,91.8,90.9,95.5,100.6,104.5,106.9,115.7,115.5,110.0,108.8,107.0,104.2,114.9,114.6,112.8,109.0,111.2,97.6,103.5,104.4,101.2,116.1,106.0,111.8,116.0,109.9,117.7,124.9,131.8,139.4,132.4,116.1,90.0,95.8,97.3,94.1,96.6,100.2,96.3,98.0,117.9,127.1,128.7,147.2,152.2,153.8,164.2,155.5,151.8,159.9,163.7,169.9,158.8,158.4,148.6,141.8,136.3,119.4,132.2,112.6,115.4,113.7,104.5,125.9,138.2,134.1,140.0,143.4,130.3,138.2,135.7,145.4,158.0,146.3,145.0,139.1,156.1,170.1,174.4,186.1,192.7 }, 
        .high_price = { 68.0,64.1,59.7,61.0,64.7,66.2,65.9,65.0,67.8,67.9,69.8,80.5,87.4,88.2,91.3,94.0,89.1,88.1,92.7,94.5,95.2,95.9,102.4,106.7,108.5,117.3,119.3,118.8,115.2,114.7,111.9,117.6,118.3,119.2,116.8,112.9,112.9,105.2,107.3,108.4,117.2,117.0,112.4,117.2,116.8,120.4,127.4,132.4,140.1,141.1,139.3,122.9,104.4,102.9,115.8,101.3,106.4,105.2,104.4,123.5,127.3,142.8,154.9,161.7,157.2,165.7,167.4,159.2,163.8,169.3,173.0,172.3,163.4,169.8,159.0,143.9,137.4,133.1,132.9,116.5,124.2,121.6,127.4,138.2,138.7,143.5,144.3,144.0,141.8,143.4,146.0,159.4,158.0,150.2,153.1,156.1,170.7,178.3,186.4,192.7 }, 
        .low_price = { 63.5,54.7,52.5,56.7,57.1,60.6,57.0,58.8,63.4,65.1,66.1,67.6,80.7,83.0,84.2,85.2,84.4,81.6,81.7,90.3,90.2,88.1,95.0,95.9,102.2,106.8,104.0,106.7,106.1,105.0,103.1,102.2,113.0,112.5,102.7,106.0,91.1,95.9,100.1,98.1,102.1,104.8,105.3,112.2,104.3,107.3,110.5,126.0,128.6,129.7,112.7,76.9,82.8,82.4,92.0,90.8,95.0,91.4,95.1,97.9,118.1,123.8,128.5,148.0,146.7,152.1,147.6,145.7,149.5,150.5,160.1,158.3,151.8,139.6,139.8,127.3,118.9,115.0,110.9,106.1,111.0,104.4,101.3,125.9,128.4,133.6,137.4,123.1,126.2,131.8,135.4,141.4,145.5,142.6,135.2,138.5,155.8,164.3,171.4,184.3 } 
    }; 

    ticker.stocks[10] = (stock_t){
        .name = "Tesla, Inc.", 
        .symbol = "TSLA", 
        .open_price = { 15.4,15.4,12.6,12.9,16.3,16.1,14.8,13.7,15.7,13.9,14.2,13.2,12.6,14.3,16.9,16.9,19.1,21.0,22.9,24.7,21.5,23.7,22.8,22.1,20.4,20.8,23.4,23.0,17.1,19.6,19.1,24.0,19.9,19.8,20.4,22.6,24.0,20.4,20.4,20.5,18.8,15.9,12.4,15.3,16.2,14.9,16.1,21.1,22.0,28.3,44.9,47.4,33.6,50.3,57.2,72.2,96.6,167.4,146.9,131.3,199.2,239.8,271.4,230.0,229.5,234.6,209.3,228.0,233.3,244.7,259.5,381.7,386.9,382.6,311.7,289.9,360.4,286.9,251.7,227.0,301.3,272.6,254.5,234.1,197.1,118.5,173.9,206.2,199.9,163.2,202.6,276.5,266.3,257.3,244.8,204.0,233.1,250.1,188.5,200.5 }, 
        .close_price = { 16.0,12.7,12.8,15.3,16.1,14.9,14.2,15.7,14.1,13.6,13.2,12.6,14.2,16.8,16.7,18.6,20.9,22.7,24.1,21.6,23.7,22.7,22.1,20.6,20.8,23.6,22.9,17.7,19.6,19.0,22.9,19.9,20.1,17.7,22.5,23.4,22.2,20.5,21.3,18.7,15.9,12.3,14.9,16.1,15.0,16.1,21.0,22.0,27.9,43.4,44.5,34.9,52.1,55.7,72.0,95.4,166.1,143.0,129.3,189.2,235.2,264.5,225.2,222.6,236.5,208.4,226.6,229.1,245.2,258.5,371.3,381.6,352.3,312.2,290.1,359.2,290.3,252.8,224.5,297.1,275.6,265.2,227.5,194.7,123.2,173.2,205.7,207.5,164.3,203.9,261.8,267.4,258.1,250.2,200.8,240.1,248.5,187.3,201.9,173.8 }, 
        .high_price = { 16.2,15.4,13.3,16.0,18.0,16.2,16.1,15.7,15.8,14.1,14.4,13.3,14.9,17.2,19.2,18.8,21.0,22.9,25.8,24.8,24.7,26.0,24.2,22.2,23.2,24.0,24.0,23.2,20.6,20.9,24.9,24.3,25.8,21.0,23.1,24.5,25.3,23.5,21.6,20.5,19.7,17.2,15.6,17.7,16.3,16.9,22.7,24.1,29.0,43.5,64.6,53.8,58.0,56.2,72.5,119.7,166.7,167.5,155.3,202.6,239.6,300.1,293.5,240.4,260.3,235.3,232.5,233.3,246.8,266.3,371.7,414.5,390.9,402.7,315.9,371.6,384.3,318.5,264.2,298.3,314.7,313.8,257.5,237.4,198.9,180.7,217.6,207.8,202.7,204.5,277.0,299.3,266.5,279.0,268.9,252.8,265.1,251.2,205.6,204.5 }, 
        .low_price = { 14.3,12.2,9.4,12.1,15.6,13.6,12.5,13.7,13.9,12.9,12.8,11.9,12.0,14.1,16.1,16.2,19.0,19.4,22.3,20.2,20.7,22.4,21.1,19.5,20.0,20.4,19.7,16.5,16.3,18.2,18.9,19.1,19.2,16.8,16.5,21.7,19.6,18.6,19.3,17.0,15.4,12.3,11.8,14.8,14.1,14.6,15.0,20.6,21.8,28.1,40.8,23.4,29.8,45.5,56.9,72.0,91.0,110.0,126.4,130.8,180.4,239.1,206.3,179.8,219.8,182.3,190.4,206.8,216.3,236.3,254.5,326.2,295.4,264.0,233.3,252.0,273.9,206.9,208.7,216.2,271.8,262.5,198.6,166.2,108.2,101.8,169.9,163.9,152.4,158.8,199.4,254.1,212.4,234.6,194.1,197.9,228.2,180.1,175.0,160.5 } 
    }; 

    ticker.stocks[11] = (stock_t){
        .name = "UnitedHealth Group Incorporated", 
        .symbol = "UNH", 
        .open_price = { 113.5,116.9,114.8,119.5,128.7,132.6,133.6,141.2,143.4,136.8,139.4,141.5,159.1,161.1,162.8,166.7,164.6,175.0,175.8,186.3,193.4,199.8,196.6,211.6,228.9,221.0,235.3,225.7,218.5,237.0,243.7,245.0,256.1,268.0,267.2,262.9,283.0,245.0,268.5,243.6,249.7,233.1,241.5,245.9,249.2,231.7,219.2,254.0,281.8,294.0,275.1,257.3,238.7,288.4,304.0,295.8,303.6,310.2,312.9,312.6,344.8,351.5,335.0,334.4,372.2,401.0,413.7,402.0,413.6,416.5,391.6,461.8,452.9,500.0,475.0,470.9,510.7,510.8,498.3,512.3,542.3,519.3,507.1,555.0,552.4,525.1,500.0,473.6,485.2,494.6,487.8,478.1,507.5,479.0,505.5,530.0,550.4,526.8,508.8,489.4 }, 
        .close_price = { 117.6,115.2,119.1,128.9,131.7,133.7,141.2,143.2,136.1,140.0,141.3,158.3,160.0,162.1,165.4,164.0,174.9,175.2,185.4,191.8,198.9,195.9,210.2,228.2,220.5,236.8,226.2,214.0,236.4,241.5,245.3,253.2,268.5,266.0,261.4,281.4,249.1,270.2,242.2,247.3,233.1,241.8,244.0,249.0,234.0,217.3,252.7,279.9,294.0,272.5,255.0,249.4,292.5,304.9,295.0,302.8,312.5,311.8,305.1,336.3,350.7,333.6,332.2,372.1,398.8,411.9,400.4,412.2,416.3,390.7,460.5,444.2,502.1,472.6,475.9,510.0,508.5,496.8,513.6,542.3,519.3,505.0,555.2,547.8,530.2,499.2,475.9,472.6,492.1,487.2,480.6,506.4,476.6,504.2,535.6,553.0,526.5,511.7,493.6,487.0 }, 
        .high_price = { 121.1,117.9,122.3,131.1,135.1,134.8,141.3,144.5,144.2,141.8,146.4,159.8,164.0,163.8,166.8,172.1,176.1,178.9,188.7,193.0,199.5,200.8,212.8,228.8,231.8,250.8,237.8,231.3,241.7,249.2,256.7,259.0,270.2,271.2,272.8,285.5,287.9,272.4,272.5,259.2,250.2,251.2,253.5,268.7,251.6,236.6,255.7,283.0,300.0,302.5,306.7,295.8,304.0,309.7,315.8,311.0,324.6,323.8,335.6,368.0,354.1,367.5,344.6,380.5,402.2,426.0,413.7,422.5,431.4,424.4,461.4,466.0,509.2,503.8,500.9,521.9,553.3,513.5,518.7,544.3,553.1,535.0,558.1,555.7,553.0,525.6,504.4,486.3,530.5,500.9,502.9,515.9,513.7,514.2,546.8,553.9,554.7,549.0,532.8,496.0 }, 
        .low_price = { 113.1,107.5,108.8,119.4,125.3,128.5,133.0,139.3,135.6,132.4,133.0,136.2,156.2,156.1,156.5,162.7,164.2,166.6,175.2,183.9,190.6,188.2,186.0,208.9,218.2,220.0,208.5,212.5,214.6,228.2,241.3,244.1,252.2,257.5,253.3,258.3,231.8,236.1,239.1,234.5,208.1,227.2,234.8,239.5,220.8,213.1,212.1,249.1,273.9,271.2,245.3,187.7,226.0,275.6,273.7,287.1,299.2,289.6,299.6,307.4,329.4,329.0,320.4,332.7,360.5,400.5,387.2,401.8,404.3,390.5,383.1,436.0,439.2,447.3,445.7,467.7,504.5,463.3,449.7,492.2,519.2,499.0,487.7,500.8,515.7,474.8,463.9,457.6,478.4,472.5,445.7,447.2,476.3,472.1,503.1,526.8,515.9,479.0,484.4,468.2 } 
    }; 

    ticker.stocks[12] = (stock_t){
        .name = "Visa Inc.", 
        .symbol = "V", 
        .open_price = { 79.5,76.1,74.1,73.0,76.2,77.8,78.7,74.5,78.3,81.1,82.4,82.6,77.6,78.8,82.9,88.7,89.1,91.3,95.4,94.4,100.4,104.0,105.5,110.5,112.4,114.6,124.7,123.3,119.3,126.9,131.8,132.0,137.7,146.9,150.9,139.0,145.0,130.0,135.4,149.5,157.5,165.5,161.5,175.3,179.2,180.5,173.0,180.1,184.2,189.0,199.9,186.3,156.3,174.4,194.7,193.9,191.8,212.2,202.2,184.5,212.1,220.2,195.1,215.0,213.8,234.1,229.4,234.2,246.2,229.1,224.2,213.5,196.0,217.5,226.9,214.5,223.1,211.8,212.1,196.8,208.4,198.7,179.3,208.9,217.0,209.3,229.4,219.5,225.2,232.9,222.7,237.0,237.1,247.5,229.2,236.1,255.8,259.6,273.4,283.2 }, 
        .close_price = { 77.6,74.5,72.4,76.5,77.2,78.9,74.2,78.1,80.9,82.7,82.5,77.3,78.0,82.7,87.9,88.9,91.2,95.2,93.8,99.6,103.5,105.2,110.0,112.6,114.0,124.2,122.9,119.6,126.9,130.7,132.4,136.7,146.9,150.1,137.9,141.7,131.9,135.0,148.1,156.2,164.4,161.3,173.6,178.0,180.8,172.0,178.9,184.5,187.9,199.0,181.8,161.1,178.7,195.2,193.2,190.4,212.0,200.0,181.7,210.4,218.7,193.2,212.4,211.7,233.6,227.3,233.8,246.4,229.1,222.8,211.8,193.8,216.7,226.2,216.1,221.8,213.1,212.2,196.9,212.1,198.7,177.6,207.2,217.0,207.8,230.2,219.9,225.5,232.7,221.0,237.5,237.7,245.7,230.0,235.1,256.7,260.4,273.3,282.6,285.0 }, 
        .high_price = { 80.5,76.5,74.8,77.0,81.7,79.9,81.7,80.2,81.8,83.8,83.7,84.0,80.4,84.3,88.5,92.1,92.8,95.5,96.6,101.2,104.2,106.8,110.7,113.6,114.9,126.9,126.3,125.4,127.9,132.5,136.7,143.1,147.7,150.6,151.6,145.5,145.7,139.9,148.8,156.8,165.7,165.8,174.9,184.1,182.4,187.1,180.2,184.9,189.9,210.1,214.2,194.5,182.2,198.3,202.2,200.9,216.2,217.4,208.0,217.6,220.4,220.2,220.5,228.2,237.5,235.7,238.5,252.7,247.8,233.3,237.0,221.6,219.7,228.1,235.9,228.8,229.2,214.8,217.6,218.1,217.6,207.2,211.5,217.0,220.0,232.8,234.3,227.4,235.6,234.8,238.3,245.4,248.2,250.1,241.5,256.8,263.2,280.0,286.1,289.0 }, 
        .low_price = { 75.5,68.8,66.1,69.6,75.8,76.2,73.2,73.8,77.7,81.0,81.1,77.3,75.2,78.5,81.6,87.8,88.1,91.1,92.8,93.2,99.4,102.3,104.9,106.9,106.6,113.9,111.0,116.0,116.7,125.3,129.5,131.1,137.0,142.5,129.8,129.5,121.6,127.9,135.3,144.5,156.3,156.4,156.8,172.7,167.0,172.0,168.6,175.2,179.7,187.2,173.0,133.9,150.6,171.7,186.2,187.2,190.1,193.1,179.2,183.9,204.5,192.8,195.0,205.8,212.3,220.3,226.3,234.1,228.7,216.3,208.5,192.6,190.1,195.6,201.4,186.7,201.1,189.9,185.9,194.1,198.6,174.8,174.6,193.3,202.1,206.2,217.5,208.8,224.1,216.1,221.0,227.7,235.2,227.9,227.8,235.7,252.1,256.9,272.8,276.2 } 
    }; 

    ticker.stocks[13] = (stock_t){
        .name = "Exxon Mobil Corporation", 
        .symbol = "XOM", 
        .open_price = { 81.8,77.5,76.7,80.6,82.4,88.2,88.4,93.4,88.1,86.7,86.9,83.5,88.0,90.9,84.0,81.7,82.0,81.5,80.4,80.8,80.2,76.4,81.3,83.4,83.4,83.8,87.5,75.5,74.3,77.3,81.9,81.9,80.9,80.4,85.3,79.8,80.2,67.3,74.9,79.4,81.2,79.9,71.1,77.1,73.7,67.9,70.8,68.4,68.5,70.2,61.4,52.6,36.9,45.6,45.3,44.5,42.0,39.8,33.8,33.1,39.0,41.5,45.6,56.5,56.3,58.0,59.5,64.3,57.5,54.5,59.4,65.1,60.9,61.2,76.4,78.8,82.0,85.0,97.0,86.7,94.8,94.4,90.0,112.4,111.6,109.8,115.8,109.3,113.4,116.0,101.8,107.5,106.9,112.2,117.5,106.5,102.5,100.9,103.6,105.7 }, 
        .close_price = { 77.9,77.8,80.2,83.6,88.4,89.0,93.7,88.9,87.1,87.3,83.3,87.3,90.3,83.9,81.3,82.0,81.7,80.5,80.7,80.0,76.3,82.0,83.3,83.3,83.6,87.3,75.7,74.6,77.8,81.2,82.7,81.5,80.2,85.0,79.7,79.5,68.2,73.3,79.0,80.8,80.3,70.8,76.6,74.4,68.5,70.6,67.6,68.1,69.8,62.1,51.4,38.0,46.5,45.5,44.7,42.1,39.9,34.3,32.6,38.1,41.2,44.8,54.4,55.8,57.2,58.4,63.1,57.6,54.5,58.8,64.5,59.8,61.2,76.0,78.4,82.6,85.2,96.0,85.6,96.9,95.6,87.3,110.8,111.3,110.3,116.0,109.9,109.7,118.3,102.2,107.2,107.2,111.2,117.6,105.8,102.7,100.0,102.8,104.5,112.3 }, 
        .high_price = { 82.1,79.9,83.4,85.1,89.8,90.5,93.8,95.6,88.9,89.4,88.7,88.2,93.2,91.3,84.2,84.2,83.6,83.2,83.7,82.5,80.8,82.4,84.2,84.1,84.4,89.3,89.2,77.0,80.9,82.7,83.8,84.4,81.6,87.4,86.9,83.8,81.9,73.5,79.8,82.0,83.5,80.3,77.8,77.9,74.3,75.2,70.9,73.1,70.5,71.4,63.0,54.2,47.7,47.2,55.4,45.4,46.4,40.0,36.0,42.1,44.5,51.1,57.2,62.5,59.5,64.0,64.9,64.4,59.1,60.5,65.9,66.4,63.3,76.4,83.1,91.5,89.8,99.8,105.6,97.5,101.6,99.2,112.9,114.7,112.1,117.8,119.6,113.8,119.9,117.3,109.1,108.5,112.1,120.7,117.8,109.2,104.2,104.9,105.4,112.9 }, 
        .low_price = { 73.8,71.6,73.6,80.3,82.0,87.2,87.6,86.1,85.6,82.3,83.0,82.8,86.6,83.1,80.8,80.3,80.3,80.5,79.3,78.3,76.1,76.3,81.2,80.0,82.2,83.7,73.9,72.7,72.2,75.4,79.3,80.7,76.5,79.6,76.2,74.7,64.7,67.3,72.7,77.9,79.6,70.6,71.0,74.2,66.5,67.6,66.3,67.3,67.5,61.9,48.0,30.1,36.3,40.2,43.2,40.9,39.3,33.8,31.1,32.5,38.3,41.0,44.3,54.5,54.3,57.7,59.5,54.6,52.1,53.0,59.4,59.5,58.0,61.2,74.0,76.2,79.3,83.4,83.5,80.7,86.3,83.9,89.7,107.5,102.4,104.8,108.6,98.0,113.1,101.7,101.3,100.2,104.6,112.2,104.5,101.2,97.5,95.8,100.4,104.0 } 
    }; 

    ticker.stocks[14] = (stock_t){
        .name = "Johnson & Johnson", 
        .symbol = "JNJ", 
        .open_price = { 101.7,101.7,103.6,105.9,108.0,112.2,112.7,121.3,125.3,119.2,118.0,114.8,111.4,115.8,112.5,122.5,124.7,123.4,128.3,132.8,133.2,132.6,130.2,139.8,139.6,139.7,137.5,129.1,127.8,126.3,120.4,121.3,132.4,134.7,138.3,140.1,145.6,128.1,134.0,137.2,140.0,140.9,131.5,140.2,130.3,128.0,130.0,132.1,137.7,145.9,149.4,134.8,127.7,149.6,147.3,140.7,146.4,153.9,149.3,139.0,146.3,157.2,165.3,161.4,162.6,163.6,170.1,164.7,172.5,172.9,161.5,163.2,156.9,170.2,171.7,163.0,177.1,180.5,179.1,177.4,174.2,161.5,164.3,174.1,179.0,176.2,163.0,153.0,154.9,163.6,154.5,164.3,166.4,161.4,155.4,149.2,156.4,156.9,158.2,161.8 }, 
        .close_price = { 102.7,104.4,105.2,108.2,112.1,112.7,121.3,125.2,119.3,118.1,116.0,111.3,115.2,113.2,122.2,124.6,123.5,128.2,132.3,132.7,132.4,130.0,139.4,139.3,139.7,138.2,129.9,128.1,126.5,119.6,121.3,132.5,134.7,138.2,140.0,146.9,129.1,133.1,136.6,139.8,141.2,131.1,139.3,130.2,128.4,129.4,132.0,137.5,145.9,148.9,134.5,131.1,150.0,148.8,140.6,145.8,153.4,148.9,137.1,144.7,157.4,163.1,158.5,164.4,162.7,169.2,164.7,172.2,173.1,161.5,162.9,155.9,171.1,172.3,164.6,177.2,180.5,179.5,177.5,174.5,161.3,163.4,174.0,178.0,176.6,163.4,153.3,155.0,163.7,155.1,165.5,167.5,161.7,155.8,148.3,154.7,156.7,158.9,161.4,156.8 }, 
        .high_price = { 105.5,104.8,106.9,109.6,114.2,115.0,121.4,126.1,125.9,120.0,120.2,122.5,117.3,117.0,122.9,129.0,125.8,128.8,137.0,137.1,135.0,135.8,144.4,141.9,143.8,148.3,140.7,135.7,132.9,127.6,124.8,132.6,137.4,143.1,141.4,148.8,149.0,135.2,137.9,140.0,141.4,142.4,145.0,142.5,134.1,132.8,137.5,138.6,147.8,151.2,154.5,143.6,157.0,153.6,150.0,151.7,154.4,155.5,153.1,151.3,157.7,173.6,167.9,167.0,167.8,172.7,170.2,173.4,179.9,175.2,166.0,167.6,173.5,174.3,173.6,180.2,186.7,181.7,183.4,180.0,175.5,167.7,175.4,178.1,181.0,180.9,166.3,156.2,167.2,166.2,166.3,175.4,176.0,165.3,159.3,155.1,160.0,163.6,162.2,163.1 }, 
        .low_price = { 100.3,94.3,99.8,105.4,107.7,111.7,112.1,120.8,118.3,117.0,113.0,111.3,109.3,110.8,112.5,122.4,120.9,122.3,128.1,129.6,130.9,129.1,130.0,136.6,138.6,138.1,122.2,124.9,123.5,118.6,120.0,120.1,128.9,133.4,132.2,139.0,121.0,125.0,131.3,135.7,134.4,128.5,131.0,127.8,126.6,126.3,126.1,129.7,136.2,141.4,130.8,109.2,125.5,143.0,137.0,140.1,145.8,143.0,133.6,137.5,145.9,154.1,158.0,151.5,156.5,163.1,161.8,164.6,171.3,161.4,157.3,155.9,156.2,158.3,155.7,162.4,175.5,172.7,167.3,169.8,161.3,160.8,159.2,166.8,174.1,161.1,153.0,150.1,153.9,153.3,153.1,157.3,161.3,155.3,144.9,145.6,151.8,156.8,154.8,156.5 } 
    }; 

    ticker.stocks[15] = (stock_t){
        .name = "Mastercard Incorporated", 
        .symbol = "MA", 
        .open_price = { 98.2,95.4,88.5,87.8,93.7,97.2,95.9,89.2,95.2,96.7,101.4,107.0,102.3,104.4,106.6,111.4,112.7,116.5,122.8,122.2,128.7,133.8,141.9,149.9,150.4,152.0,172.5,176.4,174.6,178.3,192.0,195.7,199.3,215.7,224.8,198.9,206.0,185.8,212.0,226.9,238.3,254.9,251.8,270.0,273.9,280.0,271.5,279.0,290.6,300.5,318.8,298.9,230.9,268.7,300.8,296.0,311.0,357.7,342.2,294.2,339.7,358.0,320.9,360.7,357.0,385.5,364.5,366.0,389.3,347.4,349.8,335.2,320.8,359.8,385.8,357.9,359.2,363.0,358.2,314.1,347.8,323.8,287.9,332.2,358.0,350.0,368.6,354.0,362.6,380.5,367.2,391.3,393.8,413.8,393.6,378.7,412.9,424.1,455.0,474.9 }, 
        .close_price = { 97.4,89.0,86.9,94.5,97.0,95.9,88.1,95.2,96.6,101.8,107.0,102.2,103.2,106.3,110.5,112.5,116.3,122.9,121.4,127.8,133.3,141.2,148.8,150.5,151.4,169.0,175.8,175.2,178.3,190.1,196.5,198.0,215.6,222.6,197.7,201.1,188.6,211.1,224.8,235.4,254.2,251.5,264.5,272.3,281.4,271.6,276.8,292.2,298.6,315.9,290.2,241.6,275.0,300.9,295.7,308.5,358.2,338.2,288.6,336.5,356.9,316.3,353.9,356.0,382.1,360.6,365.1,385.9,346.2,347.7,335.5,314.9,359.3,386.4,360.8,357.4,363.4,357.9,315.5,353.8,324.4,284.3,328.2,356.4,347.7,370.6,355.3,363.4,380.0,365.0,393.3,394.3,412.6,395.9,376.4,413.8,426.5,449.2,474.8,478.9 }, 
        .high_price = { 100.8,95.8,89.2,94.9,100.0,97.9,98.0,96.5,97.2,102.3,108.9,107.1,105.7,111.1,111.0,113.5,117.4,123.0,126.2,132.2,134.5,143.6,152.0,154.6,154.6,170.8,179.2,183.7,180.0,194.7,204.0,214.3,215.9,224.4,225.4,208.9,209.9,213.0,225.6,237.1,257.4,258.9,269.9,283.3,283.0,293.7,280.4,293.0,301.5,327.1,347.2,314.6,285.0,310.0,316.1,317.2,367.2,361.6,355.0,357.0,359.4,358.1,368.8,389.5,401.5,386.9,380.9,395.3,390.0,362.6,367.4,371.1,364.6,386.5,399.9,370.8,382.0,369.2,368.3,356.8,362.0,339.5,331.8,356.4,369.3,390.0,380.5,369.1,381.9,392.2,395.2,405.2,417.8,418.6,405.3,414.2,428.4,462.0,479.1,482.0 }, 
        .low_price = { 94.5,81.0,78.5,85.9,93.0,94.1,87.6,86.7,94.4,96.5,99.8,100.4,99.5,104.1,104.0,110.1,111.0,115.6,119.9,120.7,127.6,131.7,141.3,145.3,140.6,151.1,156.8,168.6,167.9,176.9,191.9,194.8,197.9,209.7,183.8,177.4,171.9,181.0,211.2,215.9,233.1,240.0,240.2,264.9,253.9,266.6,258.5,268.4,281.5,296.0,273.5,200.0,227.1,263.0,285.1,288.6,309.3,320.8,281.2,288.1,325.5,312.4,317.6,344.7,355.2,355.4,359.5,363.4,344.7,335.6,328.9,310.1,306.0,330.6,341.3,305.6,342.9,312.8,303.6,309.5,324.3,281.7,276.9,308.6,336.4,343.9,349.6,340.2,356.0,357.9,365.9,387.1,386.4,391.5,359.8,375.0,404.3,416.5,450.1,464.6 } 
    }; 

    ticker.stocks[16] = (stock_t){
        .name = "The Procter & Gamble Company", 
        .symbol = "PG", 
        .open_price = { 74.9,78.4,81.2,80.5,82.0,80.0,80.9,84.5,85.4,87.4,89.3,86.6,82.2,83.9,87.0,91.1,89.9,87.4,88.0,87.4,91.0,92.4,91.3,86.3,90.2,91.9,86.2,78.4,79.3,72.1,73.3,77.5,80.4,82.5,83.3,88.8,94.7,91.0,96.3,98.6,104.2,106.2,103.2,109.9,118.6,119.8,124.4,124.8,121.9,124.5,124.7,113.2,107.9,117.6,116.0,119.7,130.5,137.9,139.6,138.5,139.2,139.7,129.0,124.2,135.1,134.0,135.8,135.4,141.8,142.3,139.9,143.4,144.9,161.7,160.8,154.3,153.5,161.6,148.0,144.2,138.3,137.8,127.2,134.7,149.5,150.9,142.1,138.1,148.4,156.0,143.2,151.5,155.9,154.9,144.8,150.7,153.3,146.4,156.8,158.1 }, 
        .close_price = { 79.4,81.7,80.3,82.3,80.1,81.0,84.7,85.6,87.3,89.8,86.8,82.5,84.1,87.6,91.1,89.8,87.3,88.1,87.2,90.8,92.3,91.0,86.3,90.0,91.9,86.3,78.5,79.3,72.3,73.2,78.1,80.9,82.9,83.2,88.7,94.5,91.9,96.5,98.6,104.1,106.5,102.9,109.7,118.0,120.2,124.4,124.5,122.1,124.9,124.6,113.2,110.0,117.9,115.9,119.6,131.1,138.3,139.0,137.1,138.9,139.1,128.2,123.5,135.4,133.4,134.9,134.9,142.2,142.4,139.8,143.0,144.6,163.6,160.4,155.9,152.8,160.6,147.9,143.8,138.9,137.9,126.2,134.7,149.2,151.6,142.4,137.6,148.7,156.4,142.5,151.7,156.3,154.3,145.9,150.0,153.5,146.5,157.1,158.9,161.2 }, 
        .high_price = { 81.2,82.0,83.0,83.9,83.8,82.9,84.8,86.9,88.5,90.2,90.3,87.7,85.7,88.0,91.8,92.0,91.1,88.3,90.2,91.1,93.0,94.7,93.5,90.4,93.1,91.9,86.5,80.8,79.5,75.0,78.7,81.0,84.2,86.3,90.7,94.8,96.9,96.8,100.4,104.2,107.2,108.7,112.6,121.8,122.0,125.4,125.8,125.1,126.6,127.0,128.1,124.7,125.0,118.4,121.8,132.0,139.7,141.7,145.9,146.9,140.1,141.0,130.7,137.6,138.6,139.1,136.8,144.5,146.0,147.2,144.9,149.7,165.0,165.4,165.0,156.5,164.9,162.0,148.1,148.6,150.6,141.8,135.7,149.2,154.6,154.8,144.1,148.7,158.1,157.6,152.1,157.7,158.4,155.3,151.4,153.6,153.5,158.5,161.7,162.7 }, 
        .low_price = { 74.9,74.5,79.6,80.5,79.1,79.4,80.9,84.3,85.1,86.0,84.1,81.7,81.2,83.2,86.8,89.6,87.2,85.5,86.9,86.3,90.5,90.3,85.7,85.4,89.1,86.1,78.5,75.8,71.9,70.7,72.9,77.4,80.1,81.2,78.5,88.2,86.7,89.1,96.0,97.8,102.1,102.4,102.4,109.6,112.7,119.0,116.2,118.2,121.0,121.9,106.7,94.3,107.0,111.2,113.8,118.9,130.5,134.7,134.7,136.7,134.2,127.4,121.8,121.5,130.3,133.4,131.9,134.9,140.8,139.5,137.6,142.3,144.9,156.0,150.6,143.0,151.3,139.2,129.5,138.2,137.9,126.2,122.2,131.0,148.1,138.7,135.8,136.1,147.1,141.9,142.4,147.0,150.9,144.8,141.4,148.8,142.5,146.3,154.9,157.6 } 
    }; 

    ticker.stocks[17] = (stock_t){
        .name = "The Home Depot, Inc.", 
        .symbol = "HD", 
        .open_price = { 133.5,130.1,124.9,124.8,133.1,134.4,132.1,128.3,138.1,134.5,128.2,121.7,129.3,135.1,137.7,146.7,146.9,156.2,153.5,154.4,150.2,150.3,164.2,166.4,180.3,190.2,199.3,182.8,177.1,184.7,187.2,193.8,196.9,200.7,208.5,176.8,183.3,169.7,184.0,185.8,193.0,203.2,189.5,209.7,214.1,226.4,233.0,236.1,220.9,219.1,230.3,220.0,175.9,216.8,249.4,249.6,266.7,284.0,279.4,270.1,278.7,266.0,271.2,258.8,306.9,326.3,320.7,319.9,330.0,325.6,328.1,373.0,402.1,416.6,369.5,314.6,300.5,302.0,301.7,275.7,300.6,288.4,281.0,300.4,326.3,317.4,322.4,291.9,294.9,299.0,284.0,309.8,331.8,332.0,300.5,285.6,313.8,344.2,353.4,380.4 }, 
        .close_price = { 132.2,125.8,124.1,133.4,133.9,132.1,127.7,138.2,134.1,128.7,122.0,129.4,134.1,137.6,144.9,146.8,156.1,153.5,153.4,149.6,149.9,163.6,165.8,179.8,189.5,200.9,182.3,178.2,184.8,186.6,195.1,197.5,200.8,207.1,175.9,180.3,171.8,183.5,185.1,191.9,203.7,189.9,208.0,213.7,227.9,232.0,234.6,220.5,218.4,228.1,217.8,186.7,219.8,248.5,250.5,265.5,285.0,277.7,266.7,277.4,265.6,270.8,258.3,305.2,323.7,318.9,318.9,328.2,326.2,328.3,371.7,400.6,415.0,367.0,315.8,299.3,300.4,302.8,274.3,300.9,288.4,275.9,296.1,324.0,315.9,324.2,296.5,295.1,300.5,283.5,310.6,333.8,330.3,302.2,284.7,313.5,346.5,353.0,380.6,371.9 }, 
        .high_price = { 134.8,131.9,127.8,134.3,137.0,137.8,132.7,138.7,139.0,135.9,130.4,132.1,137.3,139.4,146.3,150.1,156.3,160.9,159.2,154.8,156.1,163.6,167.9,180.7,191.5,207.6,202.2,184.4,187.8,191.6,201.6,204.2,203.6,215.4,209.8,188.7,183.5,184.7,193.4,192.2,208.3,203.5,212.0,219.3,229.3,235.5,239.0,239.3,222.0,236.5,247.4,241.3,224.2,252.2,259.3,269.1,293.0,288.0,292.6,289.0,279.0,285.8,284.7,308.0,328.8,345.7,321.3,333.5,338.5,343.7,375.1,416.6,420.6,417.8,374.7,340.7,318.4,315.8,308.5,310.7,333.0,302.8,299.3,329.1,347.2,335.2,341.5,300.1,303.2,299.6,315.5,334.1,338.2,333.5,303.5,314.6,354.9,363.0,381.8,385.1 }, 
        .low_price = { 130.0,113.6,109.6,124.0,131.8,130.0,123.6,128.1,133.6,125.3,121.6,119.2,128.7,133.1,136.3,145.8,145.8,153.3,150.8,144.2,146.9,149.8,161.5,160.5,176.7,187.8,175.4,171.6,170.4,181.2,186.5,192.1,191.1,200.5,170.9,167.0,158.1,168.2,182.4,179.5,192.9,186.3,188.8,208.2,199.1,220.7,222.1,216.9,210.6,216.4,212.3,140.6,174.0,215.2,234.3,246.2,263.8,262.8,262.0,268.5,258.7,261.1,254.0,246.6,303.9,309.1,298.4,314.8,316.6,320.3,324.2,364.7,380.9,343.6,299.3,298.9,293.6,279.6,264.5,274.5,288.3,265.6,267.9,277.5,310.7,307.4,292.0,279.9,284.2,277.1,280.0,300.9,321.2,299.8,274.3,282.0,313.0,336.6,350.0,368.9 } 
    }; 

    ticker.stocks[18] = (stock_t){
        .name = "Advanced Micro Devices, Inc.", 
        .symbol = "AMD", 
        .open_price = { 2.4,2.8,2.2,2.2,2.8,3.6,4.6,5.1,6.9,7.2,6.9,7.3,8.9,11.4,10.9,15.1,14.6,13.4,11.2,12.6,13.7,13.1,12.8,11.2,10.8,10.4,13.6,12.3,10.0,10.8,14.0,14.8,18.3,25.6,30.7,18.4,22.5,18.0,24.6,24.0,26.4,29.0,28.8,31.8,30.5,30.8,29.0,34.4,39.3,46.9,46.4,47.4,44.2,51.1,53.3,52.6,78.2,91.9,83.1,75.8,92.2,92.1,86.8,85.4,80.2,82.0,81.0,94.0,105.9,111.3,102.6,119.4,160.4,145.1,116.8,122.3,110.5,85.7,102.1,75.2,95.6,82.3,64.5,61.5,78.3,66.0,78.5,78.6,96.7,91.0,117.3,115.2,114.3,107.0,102.2,98.6,119.9,144.3,169.3,197.9 }, 
        .close_price = { 2.9,2.2,2.1,2.8,3.5,4.6,5.1,6.9,7.4,6.9,7.2,8.9,11.3,10.4,14.5,14.6,13.3,11.2,12.5,13.6,13.0,12.8,11.0,10.9,10.3,13.7,12.1,10.1,10.9,13.7,15.0,18.3,25.2,30.9,18.2,21.3,18.5,24.4,23.5,25.5,27.6,27.4,30.4,30.5,31.5,29.0,33.9,39.2,45.9,47.0,45.5,45.5,52.4,53.8,52.6,77.4,90.8,82.0,75.3,92.7,91.7,85.6,84.5,78.5,81.6,80.1,93.9,106.2,110.7,102.9,120.2,158.4,143.9,114.2,123.3,109.3,85.5,101.9,76.5,94.5,84.9,63.4,60.1,77.6,64.8,75.2,78.6,98.0,89.4,118.2,113.9,114.4,105.7,102.8,98.5,121.2,147.4,167.7,192.5,190.6 }, 
        .high_price = { 3.1,2.8,2.2,3.0,4.0,4.7,5.5,7.2,8.0,7.6,7.5,9.2,12.4,11.7,15.6,15.1,14.7,13.6,14.7,15.6,13.9,14.2,14.4,12.3,11.2,13.9,13.8,12.8,11.4,13.9,17.3,20.2,27.3,34.1,31.9,22.2,23.8,25.1,25.5,28.1,30.0,29.7,34.3,34.9,35.5,32.0,34.3,41.8,47.3,52.8,59.3,50.2,58.6,57.0,59.0,79.0,92.6,94.3,88.7,92.7,98.0,99.2,94.2,86.9,89.2,82.0,94.3,107.0,122.5,111.8,128.1,164.5,160.9,152.4,133.0,125.7,111.4,104.6,109.6,94.8,104.6,85.7,70.3,79.2,79.2,77.1,88.9,102.4,97.3,130.8,132.8,122.1,119.5,111.8,111.3,125.7,151.1,184.9,193.0,227.3 }, 
        .low_price = { 2.2,1.8,1.8,2.1,2.6,3.5,4.1,4.8,6.2,5.7,6.2,6.2,8.3,9.4,10.8,12.4,12.2,9.9,10.6,12.1,11.9,12.0,10.6,10.7,9.7,10.3,10.6,9.8,9.0,10.8,13.9,14.7,18.0,25.6,16.2,17.2,16.0,16.9,22.3,21.0,25.8,26.0,27.3,30.1,27.6,28.4,27.4,34.1,37.2,46.1,41.0,36.8,41.7,49.1,48.4,51.6,76.1,73.8,74.2,73.8,89.0,85.0,79.4,73.9,77.9,72.5,79.0,84.2,102.0,99.5,99.8,118.1,130.6,99.3,104.3,100.1,84.0,83.3,75.5,71.6,83.7,62.8,54.6,58.0,62.0,60.0,75.9,76.7,83.8,81.0,107.1,108.6,99.6,94.5,93.1,98.5,116.4,133.7,161.8,184.0 } 
    }; 

    ticker.stocks[19] = (stock_t){
        .name = "Costco Wholesale Corporation", 
        .symbol = "COST", 
        .open_price = { 162.0,159.8,150.8,150.4,157.9,148.8,150.5,157.2,167.1,159.0,152.2,148.5,150.1,160.6,163.8,177.4,167.7,178.5,180.8,160.2,159.1,157.6,164.9,162.0,183.3,187.2,193.4,191.3,186.9,196.5,195.4,208.4,218.6,233.1,235.8,228.2,230.7,200.5,214.0,219.8,243.1,245.3,239.8,266.4,275.8,292.6,288.0,298.0,299.8,294.1,307.0,294.4,282.4,301.8,307.9,302.5,325.5,345.7,356.3,362.2,384.5,377.4,351.2,335.2,352.5,373.8,379.9,396.3,430.6,455.5,449.7,494.1,543.1,565.0,505.0,519.5,577.4,532.2,469.4,481.2,541.4,519.7,474.5,503.7,519.1,458.0,508.3,481.1,496.5,499.1,509.3,537.2,560.6,553.1,567.9,555.0,593.3,655.6,694.0,740.4 }, 
        .close_price = { 161.5,151.1,150.0,157.6,148.1,148.8,157.0,167.2,162.1,152.5,147.9,150.1,160.1,163.9,177.2,167.7,177.5,180.4,159.9,158.5,156.7,164.3,161.1,184.4,186.1,194.9,190.9,188.4,197.2,198.2,209.0,218.7,233.1,234.9,228.6,231.3,203.7,214.6,218.7,242.1,245.5,239.6,264.3,275.6,294.8,288.1,297.1,299.8,293.9,305.5,281.1,285.1,303.0,308.5,303.2,325.5,347.7,355.0,357.6,391.8,376.8,352.4,331.0,352.5,372.1,378.3,395.7,429.7,455.5,449.4,491.5,539.4,567.7,505.1,519.2,575.8,531.7,466.2,479.3,541.3,522.1,472.3,501.5,539.2,456.5,511.1,484.2,496.9,503.2,511.6,538.4,560.7,549.3,565.0,552.4,592.7,660.1,694.9,743.9,731.5 }, 
        .high_price = { 169.7,161.2,154.9,159.8,159.1,153.9,158.8,168.8,169.6,159.3,152.3,153.4,164.9,164.8,177.9,178.7,178.3,183.2,182.7,161.4,162.4,165.3,167.3,184.9,195.4,199.9,195.5,193.0,199.0,201.8,212.5,224.6,233.5,245.2,237.6,240.9,233.9,215.6,219.7,242.4,248.7,251.0,268.9,284.3,300.0,307.3,304.9,307.1,300.2,314.3,325.3,324.5,322.6,311.8,315.4,331.5,349.1,363.7,384.9,393.1,388.1,381.5,361.7,357.8,375.4,389.5,400.5,431.5,460.6,470.5,494.2,560.8,571.5,568.7,534.2,586.3,612.3,546.1,491.1,542.1,564.8,542.6,512.8,542.6,519.1,511.4,530.0,499.9,513.1,514.8,539.6,571.2,569.2,572.2,577.3,599.9,681.9,705.5,752.6,787.1 }, 
        .low_price = { 157.3,144.9,141.6,147.1,147.5,138.6,148.6,154.7,161.1,147.2,147.1,142.1,150.1,158.5,161.8,164.1,166.1,169.1,156.6,150.0,150.1,155.0,154.1,161.3,181.1,183.9,175.8,180.8,180.9,190.2,195.0,206.1,215.0,231.5,217.0,217.4,189.5,199.9,205.8,215.8,240.3,233.1,238.1,261.7,262.7,284.4,281.6,295.2,289.1,288.6,271.3,276.3,280.9,294.5,293.8,300.8,324.3,331.2,352.9,360.6,359.5,351.9,330.9,307.0,351.6,371.1,375.5,393.9,425.5,445.7,436.2,487.2,514.0,469.0,483.0,511.8,529.7,406.5,443.2,478.0,520.3,463.5,449.0,474.5,450.8,447.9,483.8,465.3,477.5,476.8,502.1,524.6,530.6,540.2,540.2,549.7,590.6,640.5,691.5,711.0 } 
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
    const static int N_ROWS_REQ = 18, N_COLS_REQ = 28;
    if (x + N_COLS_REQ >= module.ncols || y + N_ROWS_REQ >= module.nrows) {
        printf("Error: graph out of bounds\n");
        return;
    }

    const static int N_TIME_DISPLAY = 10;
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
    else if (10 < diff && diff <= 20) step_size = 2;
    else if (20 < diff && diff <= 40) step_size = 4;
    else if (40 < diff && diff <= 50) step_size = 5;
    else if (50 < diff && diff <= 100) step_size = 10;
    else if (100 < diff && diff <= 200) step_size = 20;
    else if (200 < diff && diff <= 400) step_size = 40;
    else if (400 < diff && diff <= 800) step_size = 80;
    else {
        if (diff < 1) { // two or more digits after decimal point not supported
            printf("WARNING: stock too static (max_interval_price - min_interval_price = %f < 1)\n", diff);
        }
        if (diff > 80) {
            printf("WARNING: stock too volatile (max_interval_price - min_interval_price = %f > 200)\n", diff);
        }
        return; 
    }

    // draw title
    char buf[N_COLS_REQ + 1], buf1[N_COLS_REQ + 1];
    snprintf(buf, N_COLS_REQ + 1, "%s (%s)\n", ticker.stocks[stock_ind].name, ticker.stocks[stock_ind].symbol);
    gl_draw_string(gl_get_char_width() * x, module.line_height * y, buf, GL_AMBER);

    // draw axes
    float graph_min = (int)(min_interval_price / step_size) * step_size;
    float graph_max = graph_min + N_PRICE_INTERVALS * step_size;
    for (int i = 0; i < N_PRICE_INTERVALS; i++) {
        buf[0] = '\0'; buf1[0] = '\0';
        snprintf(buf, N_COLS_REQ + 1, "%.1f-", graph_max - i * step_size);
        rprintf(buf1, buf, 8);
        int x_pix = gl_get_char_width() * x, y_pix = module.line_height * (y + 1 + i);
        gl_draw_string(x_pix, y_pix, buf1, GL_WHITE);
    }
    int x_pix = gl_get_char_width() * (x + 8), y_pix = module.line_height * (y + 1);
    gl_draw_line(x_pix, y_pix, x_pix, y_pix + module.line_height * (y + 1 + N_PRICE_INTERVALS) - 1);
    y_pix = module.line_height * (y + 1 + N_PRICE_INTERVALS);
    gl_draw_line(x_pix, y_pix, x_pix + 21 * gl_get_char_width(), GL_WHITE);
    
    // draw box plot
    for (int i = 0; i <= end_time - start_time; i++) {
        float open_price = ticker.stocks[stock_ind].open_price[i + start_time];
        float close_price = ticker.stocks[stock_ind].close_price[i + start_time];
        float high_price = ticker.stocks[stock_ind].high_price[i + start_time];
        float low_price = ticker.stocks[stock_ind].low_price[i + start_time];
        float max_price = fmax(open_price, close_price), min_price = fmin(open_price, close_price);
        int bx = (x + 6 + 2 * i) * gl_get_char_width() + 4;
        int by = (y + 1) * module.line_height + (graph_max - max_price) * 20 / step_size;
        int w = 2 * (gl_get_char_width() - 4);
        int h = (max_price - min_price) * 20 / step_size;
        int ly1 = (y + 1) * module.line_height + (graph_max - high_price) * 20 / step_size;
        int ly2 = (y + 1) * module.line_height + (graph_max - low_price) * 20 / step_size;
        int lx = (x + 6 + 2 * i) * gl_get_char_width() + 13;
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
    draw_graph(12, 0, 1);
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

