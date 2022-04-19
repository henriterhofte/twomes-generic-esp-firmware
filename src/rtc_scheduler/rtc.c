#include <time.h>
#include "rtc.h"
#include "bm8563.h"
#include "i2c_hal.h"

// private global variables
bm8563_t bm8563; //(in future private)

void rtc_scheduler_init()
{
  time_t current_time;
    // initialize bm8563 functions and i2c hal
    i2c_hal_init();
    bm8563.handle = NULL;
    bm8563.read = i2c_hal_read;
    bm8563.write = i2c_hal_write;
    bm8563_init(&bm8563);

    // set time
    current_time = time(NULL);
    bm8563_write(&bm8563, localtime( &current_time ));
}

// get rtc time in seconds 
time_t rtc_get_unixtime()
{
  struct tm rtc_time;
    // read rtc time
    bm8563_read(&bm8563, &rtc_time);

    // convert time to unix time (local time)
    return mktime(&rtc_time);
}

// set rtc alarm
void rtc_set_alarm(interval_t alarm) {
 time_t time;
 struct tm rtc_alarm,*ptr_alarm;
 uint8_t tmp;
    // clear alarm flag
    bm8563_ioctl(&bm8563,BM8563_CONTROL_STATUS2_READ, &tmp);
    tmp &= ~BM8563_AF;
    bm8563_ioctl(&bm8563, BM8563_CONTROL_STATUS2_WRITE, &tmp);
    
    // get current rtc time
    time = rtc_get_unixtime();

    // add interval to time
    time += alarm;

    // convert 
    ptr_alarm = localtime(&time);

    // set rtc alarm or timer
    if(alarm >= INTERVAL_1M && alarm <= INTERVAL_2D) {
      rtc_alarm.tm_wday = ptr_alarm->tm_wday;
      rtc_alarm.tm_mday = BM8563_ALARM_NONE;
      rtc_alarm.tm_min = ptr_alarm->tm_min;
      rtc_alarm.tm_hour = ptr_alarm->tm_hour;
      bm8563_ioctl(&bm8563, BM8563_ALARM_SET, &rtc_alarm);
    }
}