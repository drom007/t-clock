//*********************************************************************************************************
//*    ESP32 MatrixClock                                                                                  *
//*********************************************************************************************************
//
// system libraries
#include <Arduino.h>
#include <fonts.h>

#include <ESP32Time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <ESP_WiFiManager.h>
#include <SPI.h>
#include <Ticker.h>
#include <WiFi.h>      //ESP32 Core WiFi Library
#include <WebServer.h> //Local DNS Server used for redirecting all requests to the configuration portal (  https://github.com/zhouhan0126/DNSServer---esp32  )
#include <DNSServer.h> //Local WebServer used to serve the configuration portal (  https://github.com/zhouhan0126/DNSServer---esp32  )

#include "apps/sntp/sntp.h"     // espressif esp32/arduino V1.0.0
//#include "lwip/apps/sntp.h"   // espressif esp32/arduino V1.0.1-rc2 or higher

// Digital I/O used
#define SPI_MOSI      23
#define SPI_MISO      19    // not connected
#define SPI_SCK       18
#define MAX_CS        5

// Timezone -------------------------------------------
// #define TZName       "MSK-3"   // Europe Moscow  (examples see at the bottom)

// User defined text ----------------------------------
// #define UDTXT        "    Привет! "

// other defines --------------------------------------
#define anzMAX       4     // number of cascaded MAX7219
#define FORMAT24H    true  // if false - time will be displayed in 12h fromat
#define SCROLLDOWN   true  // if false - it scrolls up
#define BRIGHTNESS_DAY 10
#define BRIGHTNESS_NIGHT 0
#define GMT_OFFSET 10800   // 3 hours by 3600 seconds (GMT+3)
#define MAX_TEXT_LEN  256
//-----------------------------------------------------
//global variables
unsigned short  brightness = 0;          // values can be 0...15
String   _SSID = "";
String   _PW   = "";
String   _myIP = "0.0.0.0";
// boolean  _f_rtc = false;                 // true if time from ntp is received
uint8_t  _maxPosX = anzMAX * 8 - 1;      // calculated max X position
uint8_t  _LEDarr[anzMAX][8];             // character matrix to display (4*8)
uint8_t  _helpArrMAX[anzMAX * 8];        // helperarray for chardecoding
uint8_t  _helpArrPos[anzMAX * 8];        // helperarray pos of chardecoding
boolean  _f_tckr1s = false;              // 1s flag
boolean  _f_tckr50ms = false;            // 50ms flag
boolean  _f_tckr24h = false;             // 24h flag
int16_t  _zPosX = 0;                     // xPosition (display the time)
int16_t  _dPosX = 0;                     // xPosition (display the date)
uint16_t _chbuf[MAX_TEXT_LEN];           // characters buffer stores the position of the 'text' char in font
boolean show_colon = true;

// String months_array[12] = {"Jan.", "Feb.", "Mar.", "Apr.", "May", "June", "July", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
// String WD_arr[7] = {"Sun,", "Mon,", "Tue,", "Wed,", "Thu,", "Fri,", "Sat,"};

String months_array[12] = {"Янв", "Фев", "Мар", "Апр", "Май", "Июнь", "Июль", "Авг", "Сен", "Окт", "Ноя", "Дек"};
String WD_arr[7] = {"Воскр,", "Пон,", "Вт,", "Ср,", "Чт,", "Пят,", "Суб,"};

#include <display.h>

// NTP objects

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

ESP32Time rtc;

// RTIME class is obsolete and will be removed
// RTIME rtc;
Ticker tckr;
Ticker tckr1s;
Ticker tckr24h;

//events
// void RTIME_info(const char *info){
//     Serial.printf("rtime_info : %s\n", info);
// }

// translate character and put it to array by coordinates (shows only the time)
uint8_t char2Arr_t(unsigned short ch, int PosX, short PosY) {
    int i, j, k, l, m, o1, o2, o3, char_width=0;
    PosX++;
    k = ch - 0x30;                       //ASCII position in font
    if ((k >= 0) && (k < 11)){           //character found in font?
        char_width = font_t[k][0];       //character width
        o3 = 1 << (char_width-1);
        for (i = 0; i < char_width; i++) {
            if (((PosX - i <= _maxPosX) && (PosX - i >= 0)) && ((PosY > -8) && (PosY < 8))){ //within matrix?
                o1 = _helpArrPos[PosX - i];
                o2 = _helpArrMAX[PosX - i];
                for (j = 0; j < 8; j++) {
                    if (((PosY >= 0) && (PosY <= j)) || ((PosY < 0) && (j < PosY + 8))){ //scroll vertical
                        l = font_t[k][j + 1];
                        m = (l & (o3 >> i));  //e.g. char_width=7  0zzzzz0, char_width=4  0zz0
                        if (m > 0)
                            _LEDarr[o2][j - PosY] = _LEDarr[o2][j - PosY] | (o1);  //set point
                        else
                            _LEDarr[o2][j - PosY] = _LEDarr[o2][j - PosY] & (~o1); //clear point
                    }
                }
            }
        }
    }
    return char_width;
}
// translate characters into array (proportional font)
uint8_t char2Arr_p(uint16_t ch, int PosX) {
    int i, j, l, m, o1, o2, o3, char_width=0;
    if (ch <= 345){                   //character found in font?
        char_width = font_p[ch][0];              //character width
        o3 = 1 << (char_width - 1);
        for (i = 0; i < char_width; i++) {
            if ((PosX - i <= _maxPosX) && (PosX - i >= 0)) { //within matrix?
                o1 = _helpArrPos[PosX - i];
                o2 = _helpArrMAX[PosX - i];
                for (j = 0; j < 8; j++) {
                    l = font_p[ch][j + 1];
                    m = (l & (o3 >> i));  //e.g. char_width=7  0zzzzz0, char_width=4  0zz0
                    if (m > 0)  _LEDarr[o2][j] = _LEDarr[o2][j] | (o1);  //set point
                    else        _LEDarr[o2][j] = _LEDarr[o2][j] & (~o1); //clear point
                }
            }
        }
    }
    return char_width;
}
// Scrolling text
uint16_t scrolltext(int16_t posX, String txt)
{
    uint16_t i=0, j=0;
    boolean k=false;
    while((txt[i]!=0) && (j<MAX_TEXT_LEN)) {
        if((txt[i]>=0x20)&&(txt[i]<=0x7f)){     // ASCII section
            _chbuf[j]=txt[i]-0x20; k=true; i++; j++;
        }
        if(txt[i]==0xC2){   // basic latin section (0x80...0x9f are controls, not used)
            if((txt[i+1]>=0xA0)&&(txt[i+1]<=0xBF)){
                _chbuf[j]=txt[i+1]-0x40; k=true; i+=2; j++;
            }
        }
        if(txt[i]==0xC3){   // latin1 supplement section
            if((txt[i+1]>=0x80)&&(txt[i+1]<=0xBF)){
                _chbuf[j]=txt[i+1]+0x00; k=true; i+=2; j++;}
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
//  _chbuf stores the position of the char in font
// 'j' - is the length of the real string

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
    _f_tckr50ms = true;
}

void timer1s() {
    _f_tckr1s = true;
    // show and hide colon in time
    if (show_colon) show_colon=false;
    else show_colon = true;
    Serial.print("show_colon=");
    Serial.println(show_colon);
}

void timer24h() {
    // do somethiing once a day (sync NTP for instance)
}


//*********************************************************************************************************
void setup()
{
    Serial.begin(115200); // For debug
    pinMode(MAX_CS, OUTPUT);
    digitalWrite(MAX_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    WiFi.mode(WIFI_STA);
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    ESP_WiFiManager wm;
    bool res;
    res = wm.autoConnect("ClockSetup","password"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }

    helpArr_init();
    max7219_init();
    clear_Display();
    max7219_set_brightness(brightness);

    timeClient.begin();
    timeClient.setTimeOffset(GMT_OFFSET);
    if (!timeClient.update()) Serial.println("timeClient.update() failed to get time from NTP.");

    if(timeClient.isTimeSet()) {
        rtc.setTime(timeClient.getEpochTime());
    } else {
        Serial.println("timeClient.isTimeSet() = false, no timepacket received from NTP");
    }
    // debug
    // Serial.println("Got time from NTP. Setting RTC:");
    // Serial.println(timeClient.getFormattedTime());
    // Serial.println(rtc.getTimeDate());
    tckr.attach(0.05, timer50ms);    // every 50 msec
    tckr1s.attach(1, timer1s);    //every 1 second
    tckr24h.attach(24*60*60, timer24h);
}

void loop()
{
    uint8_t sec1 = 0, sec2 = 0, min1 = 0, min2 = 0, hrs1 = 0, hrs2 = 0;
    uint8_t sec11 = 0, sec12 = 0, sec21 = 0, sec22 = 0;
    uint8_t min11 = 0, min12 = 0, min21 = 0, min22 = 0;
    uint8_t hrs11 = 0, hrs12 = 0, hrs21 = 0, hrs22 = 0;
    signed int x = 0; //x1,x2;
    signed int y = 0, y1 = 0, y2 = 0;
    unsigned int scrollSec1 = 0, scrollSec2 = 0, scrollMin1 = 0, scrollMin2 = 0, scrollHrs1 = 0, scrollHrs2 = 0;
    static int16_t sctxtlen=0;
    boolean f_scrollend_y = false;
    boolean f_scroll_x1 = false;
    boolean f_scroll_x2 = false;

    _zPosX = _maxPosX;
    _dPosX = -8;
    //  x=0; x1=0; x2=0;

    refresh_display();

    if (SCROLLDOWN) { //scroll  up to down
        y2 = 8;
        y1 = -8;
    } else {
        y2 = -9;
        y1 = 8;
    }

    while (true) {
        if(_f_tckr24h == true) { //syncronisize RTC every day
            _f_tckr24h = false;
            // do here what need to be done once a day
        }

        if (_f_tckr1s == true)        // flag 1sek
        {
            sec1 = (rtc.getSecond()%10);
            sec2 = (rtc.getSecond()/10);
            min1 = (rtc.getMinute()%10);
            min2 = (rtc.getMinute()/10);
            hrs1 = (rtc.getHour(FORMAT24H)%10);  // 24 hour format
            hrs2 = (rtc.getHour(FORMAT24H)/10);

            y = y2;                 //scroll updown
            scrollSec1 = 1;
            sec1++;
            if (sec1 == 10) {
                scrollSec2 = 1;
                sec2++;
                sec1 = 0;
            }
            if (sec2 == 6) {
                min1++;
                sec2 = 0;
                scrollMin1 = 1;
            }
            if (min1 == 10) {
                min2++;
                min1 = 0;
                scrollMin2 = 1;
            }
            if (min2 == 6) {
                hrs1++;
                min2 = 0;
                scrollHrs1 = 1;
            }
            if (hrs1 == 10) {
                hrs2++;
                hrs1 = 0;
                scrollHrs2 = 1;
            }
            if ((hrs2 == 2) && (hrs1 == 4)) {
                hrs1 = 0;
                hrs2 = 0;
                scrollHrs2 = 1;
            }

            sec11 = sec12;
            sec12 = sec1;
            sec21 = sec22;
            sec22 = sec2;
            min11 = min12;
            min12 = min1;
            min21 = min22;
            min22 = min2;
            hrs11 = hrs12;
            hrs12 = hrs1;
            hrs21 = hrs22;
            hrs22 = hrs2;
            _f_tckr1s = false;

            if (rtc.getSecond() == 45) f_scroll_x1 = true; // scroll ddmmyy
#ifdef UDTXT
            if (rtc.getSecond() == 25) f_scroll_x2 = true; // scroll userdefined text
#endif //UDTXT
        } // end 1s
// ----------------------------------------------
        if (_f_tckr50ms == true) {
            _f_tckr50ms = false;
//          -------------------------------------
            if (f_scroll_x1) {
                _zPosX++;
                _dPosX++;
                if (_dPosX == sctxtlen)  _zPosX = 0;
                if (_zPosX == _maxPosX) {
                    f_scroll_x1 = false;
                    _dPosX = -8;
                }
            }
//          -------------------------------------
            if (f_scroll_x2 == true) {
                _zPosX++;
                _dPosX++;
                if (_dPosX == sctxtlen)  _zPosX = 0;
                if (_zPosX == _maxPosX) {
                    f_scroll_x2 = false;
                    _dPosX = -8;
                }
            }
//          -------------------------------------
            if (scrollSec1 || scrollSec2 || scrollMin1 || scrollMin2 || scrollHrs1 || scrollHrs2) {
                if (SCROLLDOWN == 1) y--;
                else                 y++;
            }

//             if (scrollSec1 == 1) {
//                 char2Arr_t(48 + sec12, _zPosX - 42, y);
//                 char2Arr_t(48 + sec11, _zPosX - 42, y + y1);
//                 if (y == 0) {
//                     scrollSec1 = 0;
//                     f_scrollend_y = true;
//                 }
//             } else char2Arr_t(48 + sec1, _zPosX - 42, 0);
// //          -------------------------------------
//             if (scrollSec2 == 1) {
//                 char2Arr_t(48 + sec22, _zPosX - 36, y);
//                 char2Arr_t(48 + sec21, _zPosX - 36, y + y1);
//                 if (y == 0) scrollSec2 = 0;
//             }
//             else char2Arr_t(48 + sec2, _zPosX - 36, 0);

//             char2Arr_t(':', _zPosX - 32, 0);
//          -------------------------------------
            if (scrollMin1 == 1) {
                char2Arr_t(48 + min12, _zPosX - 25, y);
                char2Arr_t(48 + min11, _zPosX - 25, y + y1);
                if (y == 0) scrollMin1 = 0;
            }
            else char2Arr_t(48 + min1, _zPosX - 25, 0);
//          -------------------------------------
            if (scrollMin2 == 1) {
                char2Arr_t(48 + min22, _zPosX - 19, y);
                char2Arr_t(48 + min21, _zPosX - 19, y + y1);
                if (y == 0) scrollMin2 = 0;
            }
            else char2Arr_t(48 + min2, _zPosX - 19, 0);

            if (show_colon) {
                char2Arr_t(':', _zPosX - 15 + x, 0);
                Serial.println("show ':'");
            } else {
                char2Arr_t('.', _zPosX - 15 + x, 0);
                Serial.println("show '.'");
            }

//          -------------------------------------
            if (scrollHrs1 == 1) {
                char2Arr_t(48 + hrs12, _zPosX - 8, y);
                char2Arr_t(48 + hrs11, _zPosX - 8, y + y1);
                if (y == 0) scrollHrs1 = 0;
            }
            else char2Arr_t(48 + hrs1, _zPosX - 8, 0);
//          -------------------------------------
            if (scrollHrs2 == 1) {
                char2Arr_t(48 + hrs22, _zPosX - 2, y);
                char2Arr_t(48 + hrs21, _zPosX - 2, y + y1);
                if (y == 0) scrollHrs2 = 0;
            }
            else char2Arr_t(48 + hrs2, _zPosX - 2, 0);
//          -------------------------------------
            if(f_scroll_x1){ // day month year
                String txt= "   ";
                txt += WD_arr[rtc.getDayofWeek()] + " ";
                txt += String(rtc.getDay()) + " ";
                txt += months_array[rtc.getMonth()] + "   ";
                sctxtlen = scrolltext(_dPosX, txt);
            }
//          -------------------------------------
            if(f_scroll_x2){ // user defined text
#ifdef UDTXT
                sctxtlen=scrolltext(_dPosX, UDTXT );
#endif //UDTXT
            }
//          -------------------------------------
            refresh_display(); // each 50ms
            if (f_scrollend_y == true) f_scrollend_y = false;
        } //end 50ms
// -----------------------------------------------
        if (y == 0) {
            // do something else
            if (rtc.getHour(FORMAT24H) >= 9 && rtc.getHour(FORMAT24H) < 21) {
                    if (brightness != BRIGHTNESS_DAY) {
                        brightness = BRIGHTNESS_DAY;
                        max7219_set_brightness(brightness);
                    }
            } else {
                if (brightness != BRIGHTNESS_NIGHT) {
                        brightness = BRIGHTNESS_NIGHT;
                        max7219_set_brightness(brightness);
                }
            }
        }
    }  //end while(true)
    //this section can not be reached
}