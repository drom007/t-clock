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
