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
#define SCROLLDOWN         // if not defined it scrolls up
#define BRIGHTNESS_DAY 10
#define BRIGHTNESS_NIGHT 0
#define GMT_OFFSET 10800   // 3 hours by 3600 seconds (GMT+3)
//-----------------------------------------------------
//global variables
unsigned short  brightness = 0;          // values can be 0...15
String   _SSID = "";
String   _PW   = "";
String   _myIP = "0.0.0.0";
boolean  _f_rtc = false;                 // true if time from ntp is received
uint8_t  _maxPosX = anzMAX * 8 - 1;      // calculated maxposition
uint8_t  _LEDarr[anzMAX][8];             // character matrix to display (40*8)
uint8_t  _helpArrMAX[anzMAX * 8];        // helperarray for chardecoding
uint8_t  _helpArrPos[anzMAX * 8];        // helperarray pos of chardecoding
boolean  _f_tckr1s = false;              // 1s flag
boolean  _f_tckr50ms = false;            // 50ms flag
boolean  _f_tckr24h = false;             // 24h flag
int16_t  _zPosX = 0;                     //xPosition (display the time)
int16_t  _dPosX = 0;                     //xPosition (display the date)
boolean  _f_updown = false;              //scroll direction
uint16_t _chbuf[256];

// String months_array[12] = {"Jan.", "Feb.", "Mar.", "Apr.", "May", "June", "July", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
// String WD_arr[7] = {"Sun,", "Mon,", "Tue,", "Wed,", "Thu,", "Fri,", "Sat,"};

String months_array[12] = {"Янв", "Фев", "Мар", "Апр", "Май", "Июнь", "Июль", "Авг", "Сен", "Окт", "Ноя", "Дек"};
String WD_arr[7] = {"Вск,", "Пон,", "Вт,", "Ср,", "Чт,", "Пят,", "Суб,"};

#include <display.h>

// NTP objects

WiFiUDP ntpUDP;
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionally you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP);

ESP32Time rtc;

// RTIME class is obsolete and will be removed
// RTIME rtc;
Ticker tckr;

//events
// void RTIME_info(const char *info){
//     Serial.printf("rtime_info : %s\n", info);
// }

//*********************************************************************************************************
uint8_t char2Arr_t(unsigned short ch, int PosX, short PosY) { //characters into arr, shows only the time
    int i, j, k, l, m, o1, o2, o3, char_width=0;
    PosX++;
    k = ch - 0x30;                       //ASCII position in font
    if ((k >= 0) && (k < 11)){           //character found in font?
        char_width = font_t[k][0];               //character width
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
//*********************************************************************************************************
uint8_t char2Arr_p(uint16_t ch, int PosX) { // characters into arr, proportional font
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
            // do here what need to be done once a day
        }
        if (_f_tckr1s == true)        // flag 1sek
        {
            sek1 = (rtc.getSecond()%10);
            sek2 = (rtc.getSecond()/10);
            min1 = (rtc.getMinute()%10);
            min2 = (rtc.getMinute()/10);

            std1 = (rtc.getHour(FORMAT24H)%10);  // 24 hour format
            std2 = (rtc.getHour(FORMAT24H)/10);

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
            if (rtc.getSecond() == 45) f_scroll_x1 = true; // scroll ddmmyy
#ifdef UDTXT
            if (rtc.getSecond() == 25) f_scroll_x2 = true; // scroll userdefined text
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
                txt += WD_arr[rtc.getDayofWeek()] + " ";
                txt += String(rtc.getDay()) + " ";
                txt += months_array[rtc.getMonth()] + " ";
                txt += String(rtc.getYear()) + "   ";
                sctxtlen = scrolltext(_dPosX, txt);
            }
//          -------------------------------------
            if(f_scroll_x2){ // user defined text
#ifdef UDTXT
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
        if (rtc.getHour(FORMAT24H) >= 8 && rtc.getHour(FORMAT24H) < 21) {
            max7219_set_brightness(BRIGHTNESS_DAY);
        } else
            max7219_set_brightness(BRIGHTNESS_NIGHT);
        }
    }  //end while(true)
    //this section can not be reached
}