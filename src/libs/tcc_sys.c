#include "../include/time.h"
#include "../include/sys/time.h"
#include "../include/sys/mman.h"
#include "../include/stdlib.h"
#include "../include/errno.h"
#include "../include/io.h"
#include <stdint.h>

static uint8_t rtc_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static int rtc_bcd(uint8_t value)
{
    return (value & 0x0Fu) + ((value >> 4) * 10);
}

static int rtc_is_leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int rtc_read_tm(struct tm *result, int *full_year)
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t century;
    uint8_t status;
    uint8_t second_after;
    int attempts = 0;
    int pm;

    do {
        for (unsigned int wait = 0; wait < 100000u; ++wait) {
            if (!(rtc_read(0x0A) & 0x80u))
                break;
        }
        second = rtc_read(0x00);
        minute = rtc_read(0x02);
        hour = rtc_read(0x04);
        day = rtc_read(0x07);
        month = rtc_read(0x08);
        year = rtc_read(0x09);
        century = rtc_read(0x32);
        status = rtc_read(0x0B);
        second_after = rtc_read(0x00);
        ++attempts;
    } while (second != second_after && attempts < 4);

    pm = (hour & 0x80u) != 0;
    hour &= 0x7Fu;

    if (!(status & 0x04u)) {
        second = (uint8_t)rtc_bcd(second);
        minute = (uint8_t)rtc_bcd(minute);
        hour = (uint8_t)rtc_bcd(hour);
        day = (uint8_t)rtc_bcd(day);
        month = (uint8_t)rtc_bcd(month);
        year = (uint8_t)rtc_bcd(year);
        century = (uint8_t)rtc_bcd(century);
    }

    if (!(status & 0x02u)) {
        hour %= 12u;
        if (pm)
            hour += 12u;
    }

    if (century >= 19 && century <= 21)
        *full_year = (int)century * 100 + year;
    else
        *full_year = year >= 70 ? 1900 + year : 2000 + year;

    result->tm_sec = second;
    result->tm_min = minute;
    result->tm_hour = hour;
    result->tm_mday = day;
    result->tm_mon = month - 1;
    result->tm_year = *full_year - 1900;
    result->tm_isdst = 0;
    return 0;
}

time_t time(time_t *result)
{
    struct tm current;
    int year;
    int month;
    long days = 0;
    static const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    rtc_read_tm(&current, &year);
    if (year < 1970 || current.tm_mon < 0 || current.tm_mon > 11 ||
        current.tm_mday < 1 || current.tm_mday > 31) {
        year = 1970;
        current.tm_mon = 0;
        current.tm_mday = 1;
        current.tm_hour = 0;
        current.tm_min = 0;
        current.tm_sec = 0;
    }

    for (int y = 1970; y < year; ++y)
        days += rtc_is_leap(y) ? 366 : 365;

    for (month = 0; month < current.tm_mon; ++month) {
        days += month_days[month];
        if (month == 1 && rtc_is_leap(year))
            ++days;
    }

    days += current.tm_mday - 1;
    {
        time_t value = (time_t)days * 86400L +
                       (time_t)current.tm_hour * 3600L +
                       (time_t)current.tm_min * 60L +
                       (time_t)current.tm_sec;
        if (result)
            *result = value;
        return value;
    }
}

clock_t clock(void)
{
    return 0;
}

int gettimeofday(struct timeval *tv, void *tz)
{
    time_t current;
    (void)tz;
    current = time(NULL);
    if (tv) {
        tv->tv_sec = current;
        tv->tv_usec = 0;
    }
    return 0;
}

int mprotect(void *address, unsigned long length, int protection)
{
    (void)address;
    (void)length;
    (void)protection;
    errno = ENOSYS;
    return -1;
}

struct tm *localtime(const time_t *timer)
{
    static struct tm result;
    static const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    long value;
    long days;
    long remainder;
    int year = 1970;
    int month = 0;

    if (!timer)
        return NULL;

    value = (long)*timer;
    if (value < 0)
        value = 0;

    days = value / 86400L;
    remainder = value % 86400L;

    while (days >= (rtc_is_leap(year) ? 366 : 365)) {
        days -= rtc_is_leap(year) ? 366 : 365;
        ++year;
    }

    while (month < 11) {
        int length = month_days[month] + (month == 1 && rtc_is_leap(year));
        if (days < length)
            break;
        days -= length;
        ++month;
    }

    result.tm_sec = (int)(remainder % 60L);
    result.tm_min = (int)((remainder / 60L) % 60L);
    result.tm_hour = (int)(remainder / 3600L);
    result.tm_mday = (int)days + 1;
    result.tm_mon = month;
    result.tm_year = year - 1900;
    result.tm_wday = (int)((value / 86400L + 4L) % 7L);
    result.tm_yday = 0;
    for (int m = 0; m < month; ++m)
        result.tm_yday += month_days[m] + (m == 1 && rtc_is_leap(year));
    result.tm_yday += (int)days;
    result.tm_isdst = 0;
    return &result;
}

struct tm *gmtime(const time_t *timer)
{
    return localtime(timer);
}
