//*********************************************************************************************************
//*    ESP32 MatrixClock                                                                                  *
//*********************************************************************************************************
//
// system libraries
#include <Arduino.h>
#include <fonts.h>
// #include <ESPAsync_WiFiManager.h>               //https://github.com/khoih-prog/ESPAsync_WiFiManager
#include <SPI.h>
#include <Ticker.h>
#include <time.h>
#include "apps/sntp/sntp.h"     // espressif esp32/arduino V1.0.0
//#include "lwip/apps/sntp.h"   // espressif esp32/arduino V1.0.1-rc2 or higher

// #if defined(ESP8266)
// #include <ESP8266WiFi.h>
// #else
// #include <WiFi.h>
// #endif

//needed for library
// #include <DNSServer.h>
// #if defined(ESP8266)
// #include <ESP8266WebServer.h>
// #else
// #include <WebServer.h>
// #endif

// Digital I/O used
#define SPI_MOSI      23
#define SPI_MISO      19    // not connected
#define SPI_SCK       18
#define MAX_CS        5

// Timezone -------------------------------------------
#define TZName       "MSK-3MSD,M3.5.0,M10.5.0/3"   // Europe Moscow  (examples see at the bottom)
//#define TZName     "GMT0BST,M3.5.0/1,M10.5.0"     // London
//#define TZName     "IST-5:30"                     // New Delhi

// User defined text ----------------------------------
#define UDTXT        "Привет!"

// other defines --------------------------------------
#define BRIGHTNESS   0     // values can be 0...15
#define anzMAX       4     // number of cascaded MAX7219
#define FORMAT12H          // if not defined time will be displayed in 12h fromat
#define SCROLLDOWN         // if not defined it scrolls up
//-----------------------------------------------------
//global variables
String   _SSID = "";
String   _PW   = "";
String   _myIP = "0.0.0.0";
boolean  _f_rtc = false;                 // true if time from ntp is received
uint8_t  _maxPosX = anzMAX * 8 - 1;      // calculated maxposition
uint8_t  _LEDarr[anzMAX][8];             // character matrix to display (40*8)
uint8_t  _helpArrMAX[anzMAX * 8];        // helperarray for chardecoding
uint8_t  _helpArrPos[anzMAX * 8];        // helperarray pos of chardecoding
boolean  _f_tckr1s = false;
boolean  _f_tckr50ms = false;
boolean  _f_tckr24h = false;
int16_t  _zPosX = 0;                     //xPosition (display the time)
int16_t  _dPosX = 0;                     //xPosition (display the date)
boolean  _f_updown = false;              //scroll direction
uint16_t _chbuf[256];

String M_arr[12] = {"Jan.", "Feb.", "Mar.", "Apr.", "May", "June", "July", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
String WD_arr[7] = {"Sun,", "Mon,", "Tue,", "Wed,", "Thu,", "Fri,", "Sat,"};



//*********************************************************************************************************
//*    Class RTIME                                                                                        *
//*********************************************************************************************************
//
extern __attribute__((weak)) void RTIME_info(const char*);

class RTIME{

public:
    RTIME();
    ~RTIME();
    boolean begin(String TimeZone="CET-1CEST,M3.5.0,M10.5.0/3");
    const char* gettime();
    const char* gettime_l();
    const char* gettime_s();
    const char* gettime_xs();
    uint8_t getweekday();
    uint8_t getyear();
    uint8_t getmonth();
    uint8_t getday();
    uint8_t gethour();
    uint8_t getminute();
    uint8_t getsecond();
protected:
    boolean obtain_time();
private:
    char sbuf[256];
    String RTIME_TZ="";
    struct tm timeinfo;
    time_t now;
    char strftime_buf[64];
    String w_day_l[7]={"Sonntag","Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag"};
    String w_day_s[7]={"So","Mo","Di","Mi","Do","Fr","Sa"};
    String month_l[12]={"Januar","Februar","März","April","Mai","Juni","Juli","August","September","Oktober","November","Dezember"};
    String month_s[12]={"Jan","Feb","März","Apr","Mai","Jun","Jul","Sep","Okt","Nov","Dez"};
};


RTIME::RTIME(){
    timeinfo = { 0,0,0,0,0,0,0,0,0 };
    now=0;
}
RTIME::~RTIME(){
    sntp_stop();
}
boolean RTIME::begin(String TimeZone){
    RTIME_TZ=TimeZone;
    if (RTIME_info) RTIME_info("Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    char sbuf[20]="pool.ntp.org";
    sntp_setservername(0, sbuf);
    sntp_init();
    return obtain_time();
}

boolean RTIME::obtain_time(){
    time_t now = 0;
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        sprintf(sbuf, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        if (RTIME_info) RTIME_info(sbuf);
        vTaskDelay(uint16_t(2000 / portTICK_PERIOD_MS));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    //setenv("TZ","CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1); // automatic daylight saving time
    setenv("TZ", RTIME_TZ.c_str(), 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    if (RTIME_info) RTIME_info(strftime_buf);

    //log_i( "The current date/time in Berlin is: %s", strftime_buf);
    if(retry < retry_count) return true;
    else return false;
}

const char* RTIME::gettime(){
    setenv("TZ", RTIME_TZ.c_str(), 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    //log_i( "The current date/time in Berlin is: %s", strftime_buf);
    return strftime_buf;
}

const char* RTIME::gettime_l(){  // Montag, 04. August 2017 13:12:44
    time(&now);
    localtime_r(&now, &timeinfo);
//    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
//    log_i( "The current date/time in Beriln is: %s", strftime_buf);
    sprintf(strftime_buf,"%s, %02d.%s ", w_day_l[timeinfo.tm_wday].c_str(), timeinfo.tm_mday, month_l[timeinfo.tm_mon].c_str());
    sprintf(strftime_buf,"%s%d %02d:", strftime_buf, timeinfo.tm_year+1900, timeinfo.tm_hour);
    sprintf(strftime_buf,"%s%02d:%02d ", strftime_buf, timeinfo.tm_min, timeinfo.tm_sec);
    return strftime_buf;
}

const char* RTIME::gettime_s(){  // hh:mm:ss
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(strftime_buf,"%02d:%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return strftime_buf;
}

const char* RTIME::gettime_xs(){  // hh:mm
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(strftime_buf,"%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min);
    return strftime_buf;
}

uint8_t RTIME::getweekday(){ //So=0, Mo=1 ... Sa=6
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_wday;
}

uint8_t RTIME::getyear(){ // 0...99
    time(&now);
    localtime_r(&now, &timeinfo);
    return (timeinfo.tm_year -100);
}

uint8_t RTIME::getmonth(){ // 0...11
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_mon;
}

uint8_t RTIME::getday(){ // 1...31
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_mday;
}

uint8_t RTIME::gethour(){ // 0...23  (0...12)
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_hour;
}

uint8_t RTIME::getminute(){ // 0...59
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_min;
}

uint8_t RTIME::getsecond(){ // 0...59
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_sec;
}

//*********************************************************************************************************

//objects
RTIME rtc;
Ticker tckr;

//events
void RTIME_info(const char *info){
    Serial.printf("rtime_info : %s\n", info);
}
//*********************************************************************************************************
const uint8_t InitArr[7][2] = { { 0x0C, 0x00 },    // display off
        { 0x00, 0xFF },    // no LEDtest
        { 0x09, 0x00 },    // BCD off
        { 0x0F, 0x00 },    // normal operation
        { 0x0B, 0x07 },    // start display
        { 0x0A, 0x04 },    // brightness
        { 0x0C, 0x01 }     // display on
};
//*********************************************************************************************************
void helpArr_init(void)  //helperarray init
{
    uint8_t i, j, k;
    j = 0;
    k = 0;
    for (i = 0; i < anzMAX * 8; i++) {
        _helpArrPos[i] = (1 << j);   //bitmask
        _helpArrMAX[i] = k;
        j++;
        if (j > 7) {
            j = 0;
            k++;
        }
    }
}
//*********************************************************************************************************
void max7219_init()  //all MAX7219 init
{
    uint8_t i, j;
    for (i = 0; i < 7; i++) {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = 0; j < anzMAX; j++) {
            SPI.write(InitArr[i][0]);  //register
            SPI.write(InitArr[i][1]);  //value
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void max7219_set_brightness(unsigned short br)  //brightness MAX7219
{
    uint8_t j;
    if (br < 16) {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = 0; j < anzMAX; j++) {
            SPI.write(0x0A);  //register
            SPI.write(br);    //value
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void clear_Display()   //clear all
{
    uint8_t i, j;
    for (i = 0; i < 8; i++)     //8 rows
    {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = anzMAX; j > 0; j--) {
            _LEDarr[j - 1][i] = 0;       //LEDarr clear
            SPI.write(i + 1);           //current row
            SPI.write(_LEDarr[j - 1][i]);
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void refresh_display() //take info into LEDarr
{
    uint8_t i, j;

    for (i = 0; i < 8; i++)     //8 rows
    {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = anzMAX; j > 0; j--) {
            SPI.write(i + 1);  //current row
            SPI.write(_LEDarr[j - 1][i]);
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
uint8_t char2Arr_t(unsigned short ch, int PosX, short PosY) { //characters into arr, shows only the time
    int i, j, k, l, m, o1, o2, o3, o4=0;
    PosX++;
    k = ch - 0x30;                       //ASCII position in font
    if ((k >= 0) && (k < 11)){           //character found in font?
        o4 = font_t[k][0];               //character width
        o3 = 1 << (o4-1);
        for (i = 0; i < o4; i++) {
            if (((PosX - i <= _maxPosX) && (PosX - i >= 0)) && ((PosY > -8) && (PosY < 8))){ //within matrix?
                o1 = _helpArrPos[PosX - i];
                o2 = _helpArrMAX[PosX - i];
                for (j = 0; j < 8; j++) {
                    if (((PosY >= 0) && (PosY <= j)) || ((PosY < 0) && (j < PosY + 8))){ //scroll vertical
                        l = font_t[k][j + 1];
                        m = (l & (o3 >> i));  //e.g. o4=7  0zzzzz0, o4=4  0zz0
                        if (m > 0)
                            _LEDarr[o2][j - PosY] = _LEDarr[o2][j - PosY] | (o1);  //set point
                        else
                            _LEDarr[o2][j - PosY] = _LEDarr[o2][j - PosY] & (~o1); //clear point
                    }
                }
            }
        }
    }
    return o4;
}
//*********************************************************************************************************
uint8_t char2Arr_p(uint16_t ch, int PosX) { //characters into arr, proportional font
    int i, j, l, m, o1, o2, o3, o4=0;
    if (ch <= 345){                   //character found in font?
        o4 = font_p[ch][0];              //character width
        o3 = 1 << (o4 - 1);
        for (i = 0; i < o4; i++) {
            if ((PosX - i <= _maxPosX) && (PosX - i >= 0)){ //within matrix?
                o1 = _helpArrPos[PosX - i];
                o2 = _helpArrMAX[PosX - i];
                for (j = 0; j < 8; j++) {
                    l = font_p[ch][j + 1];
                    m = (l & (o3 >> i));  //e.g. o4=7  0zzzzz0, o4=4  0zz0
                    if (m > 0)  _LEDarr[o2][j] = _LEDarr[o2][j] | (o1);  //set point
                    else        _LEDarr[o2][j] = _LEDarr[o2][j] & (~o1); //clear point
                }
            }
        }
    }
    return o4;
}
//*********************************************************************************************************
uint16_t scrolltext(int16_t posX, String txt)
{
    uint16_t i=0, j=0;
    boolean k=false;
    while((txt[i]!=0)&&(j<256)){
        if((txt[i]>=0x20)&&(txt[i]<=0x7f)){     // ASCII section
            _chbuf[j]=txt[i]-0x20; k=true; i++; j++;
        }
        if(txt[i]==0xC2){   // basic latin section (0x80...0x9f are controls, not used)
            if((txt[i+1]>=0xA0)&&(txt[i+1]<=0xBF)){_chbuf[j]=txt[i+1]-0x40; k=true; i+=2; j++;}
        }
        if(txt[i]==0xC3){   // latin1 supplement section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0xBF)){_chbuf[j]=txt[i+1]+0x00; k=true; i+=2; j++;}
        }
        if(txt[i]==0xCE){   // greek section
            if((txt[i+1]>=0x91)&&(txt[i+1]<=0xBF)){_chbuf[j]=txt[i+1]+0x2F; k=true; i+=2; j++;}
        }
        if(txt[i]==0xCF){   // greek section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0x89)){_chbuf[j]=txt[i+1]+0x6F; k=true; i+=2; j++;}
        }
        if(txt[i]==0xD0){   // cyrillic section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0xBF)){_chbuf[j]=txt[i+1]+0x79; k=true; i+=2; j++;}
        }
        if(txt[i]==0xD1){   // cyrillic section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0x9F)){_chbuf[j]=txt[i+1]+0xB9; k=true; i+=2; j++;}
        }
        if(k==false){
            _chbuf[j]=0x00; // space 1px
            i++; j++;
        }
        k=false;
    }
//  _chbuf stores the position of th char in font and in j is the length of the real string

    int16_t p=0;
    for(int k=0; k<j; k++){
        p+=char2Arr_p(_chbuf[k], posX - p);
        p+=char2Arr_p(0, posX - p); // 1px space
        if(_chbuf[k]==0) p+=2;      // +2px space
    }
    return p;
}
//*********************************************************************************************************
void timer50ms()
{
    static unsigned int cnt50ms = 0;
    static unsigned int cnt1s = 0;
    static unsigned int cnt1h = 0;
    _f_tckr50ms = true;
    cnt50ms++;
    if (cnt50ms == 20) {
        _f_tckr1s = true; // 1 sec
        cnt1s++;
        cnt50ms = 0;
    }
    if (cnt1s == 3600) { // 1h
        cnt1h++;
        cnt1s = 0;
    }
    if (cnt1h == 24) { // 1d
        _f_tckr24h = true;
        cnt1h = 0;
    }
}
//*********************************************************************************************************
void setup()
{
    Serial.begin(115200); // For debug
    pinMode(MAX_CS, OUTPUT);
    digitalWrite(MAX_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    // WiFi.disconnect();
    // WiFiManager wifiManager;
    // wifiManager.autoConnect("ClockSetup");

    // while (WiFi.status() != WL_CONNECTED) {
    // delay(500);
    // Serial.print(".");
    // }
    // Serial.println("");

    // Serial.println("WiFi connected");
    // Serial.print("IP address: ");
    // Serial.println(WiFi.localIP());

    helpArr_init();
    max7219_init();
    clear_Display();
    max7219_set_brightness(BRIGHTNESS);
    _f_rtc= rtc.begin(TZName);
    if(!_f_rtc) Serial.println("no timepacket received from ntp");
    tckr.attach(0.05, timer50ms);    // every 50 msec
}
//*********************************************************************************************************
void loop()
{
    uint8_t sek1 = 0, sek2 = 0, min1 = 0, min2 = 0, std1 = 0, std2 = 0;
    uint8_t sek11 = 0, sek12 = 0, sek21 = 0, sek22 = 0;
    uint8_t min11 = 0, min12 = 0, min21 = 0, min22 = 0;
    uint8_t std11 = 0, std12 = 0, std21 = 0, std22 = 0;
    signed int x = 0; //x1,x2;
    signed int y = 0, y1 = 0, y2 = 0;
    unsigned int sc1 = 0, sc2 = 0, sc3 = 0, sc4 = 0, sc5 = 0, sc6 = 0;
    static int16_t sctxtlen=0;
    boolean f_scrollend_y = false;
    boolean f_scroll_x1 = false;
    boolean f_scroll_x2 = false;

#ifdef SCROLLDOWN
    _f_updown = true;
#else
    _f_updown = false;
#endif //SCROLLDOWN

    _zPosX = _maxPosX;
    _dPosX = -8;
    //  x=0; x1=0; x2=0;

    refresh_display();
    if (_f_updown == false) {
        y2 = -9;
        y1 = 8;
    }
    if (_f_updown == true) { //scroll  up to down
        y2 = 8;
        y1 = -8;
    }
    while (true) {
        if(_f_tckr24h == true) { //syncronisize RTC every day
            _f_tckr24h = false;
            _f_rtc= rtc.begin(TZName);
            if(_f_rtc==false) Serial.println("no timepacket received");
        }
        if (_f_tckr1s == true)        // flag 1sek
        {
            sek1 = (rtc.getsecond()%10);
            sek2 = (rtc.getsecond()/10);
            min1 = (rtc.getminute()%10);
            min2 = (rtc.getminute()/10);
#ifdef FORMAT24H
            std1 = (rtc.gethour()%10);  // 24 hour format
            std2 = (rtc.gethour()/10);
#else
            uint8_t h=rtc.gethour();    // convert to 12 hour format
            if(h>12) h-=12;
            std1 = (h%10);
            std2 = (h/10);
#endif //FORMAT24H

            y = y2;                 //scroll updown
            sc1 = 1;
            sek1++;
            if (sek1 == 10) {
                sc2 = 1;
                sek2++;
                sek1 = 0;
            }
            if (sek2 == 6) {
                min1++;
                sek2 = 0;
                sc3 = 1;
            }
            if (min1 == 10) {
                min2++;
                min1 = 0;
                sc4 = 1;
            }
            if (min2 == 6) {
                std1++;
                min2 = 0;
                sc5 = 1;
            }
            if (std1 == 10) {
                std2++;
                std1 = 0;
                sc6 = 1;
            }
            if ((std2 == 2) && (std1 == 4)) {
                std1 = 0;
                std2 = 0;
                sc6 = 1;
            }

            sek11 = sek12;
            sek12 = sek1;
            sek21 = sek22;
            sek22 = sek2;
            min11 = min12;
            min12 = min1;
            min21 = min22;
            min22 = min2;
            std11 = std12;
            std12 = std1;
            std21 = std22;
            std22 = std2;
            _f_tckr1s = false;
            if (rtc.getsecond() == 45) f_scroll_x1 = true; // scroll ddmmyy
#ifdef UDTXT
            if (rtc.getsecond() == 25) f_scroll_x2 = true; // scroll userdefined text
#endif //UDTXT
        } // end 1s
// ----------------------------------------------
        if (_f_tckr50ms == true) {
            _f_tckr50ms = false;
//          -------------------------------------
            if (f_scroll_x1 == true) {
                _zPosX++;
                _dPosX++;
                if (_dPosX == sctxtlen)  _zPosX = 0;
                if (_zPosX == _maxPosX){f_scroll_x1 = false; _dPosX = -8;}
            }
//          -------------------------------------
            if (f_scroll_x2 == true) {
                _zPosX++;
                _dPosX++;
                if (_dPosX == sctxtlen)  _zPosX = 0;
                if (_zPosX == _maxPosX){f_scroll_x2 = false; _dPosX = -8;}
            }
//          -------------------------------------
            if (sc1 == 1) {
                if (_f_updown == 1) y--;
                else                y++;
                char2Arr_t(48 + sek12, _zPosX - 42, y);
                char2Arr_t(48 + sek11, _zPosX - 42, y + y1);
                if (y == 0) {sc1 = 0; f_scrollend_y = true;}
            }
            else char2Arr_t(48 + sek1, _zPosX - 42, 0);
//          -------------------------------------
            if (sc2 == 1) {
                char2Arr_t(48 + sek22, _zPosX - 36, y);
                char2Arr_t(48 + sek21, _zPosX - 36, y + y1);
                if (y == 0) sc2 = 0;
            }
            else char2Arr_t(48 + sek2, _zPosX - 36, 0);
            char2Arr_t(':', _zPosX - 32, 0);
//          -------------------------------------
            if (sc3 == 1) {
                char2Arr_t(48 + min12, _zPosX - 25, y);
                char2Arr_t(48 + min11, _zPosX - 25, y + y1);
                if (y == 0) sc3 = 0;
            }
            else char2Arr_t(48 + min1, _zPosX - 25, 0);
//          -------------------------------------
            if (sc4 == 1) {
                char2Arr_t(48 + min22, _zPosX - 19, y);
                char2Arr_t(48 + min21, _zPosX - 19, y + y1);
                if (y == 0) sc4 = 0;
            }
            else char2Arr_t(48 + min2, _zPosX - 19, 0);
            char2Arr_t(':', _zPosX - 15 + x, 0);
//          -------------------------------------
            if (sc5 == 1) {
                char2Arr_t(48 + std12, _zPosX - 8, y);
                char2Arr_t(48 + std11, _zPosX - 8, y + y1);
                if (y == 0) sc5 = 0;
            }
            else char2Arr_t(48 + std1, _zPosX - 8, 0);
//          -------------------------------------
            if (sc6 == 1) {
                char2Arr_t(48 + std22, _zPosX - 2, y);
                char2Arr_t(48 + std21, _zPosX - 2, y + y1);
                if (y == 0) sc6 = 0;
            }
            else char2Arr_t(48 + std2, _zPosX - 2, 0);
//          -------------------------------------
            if(f_scroll_x1){ // day month year
                String txt= "   ";
                txt += WD_arr[rtc.getweekday()] + " ";
                txt += String(rtc.getday()) + ". ";
                txt += M_arr[rtc.getmonth()] + " ";
                txt += "20" + String(rtc.getyear()) + "   ";
                sctxtlen=scrolltext(_dPosX, txt);
            }
//          -------------------------------------
            if(f_scroll_x2){ // user defined text
#ifdef UDTXT
                Serial.println("scrolling user defined text");
                sctxtlen=scrolltext(_dPosX, UDTXT );
#endif //UDTXT
            }
//          -------------------------------------
            refresh_display(); //all 50msec
            if (f_scrollend_y == true) f_scrollend_y = false;
        } //end 50ms
// -----------------------------------------------
        if (y == 0) {
            // do something else
        }
    }  //end while(true)
    //this section can not be reached
}