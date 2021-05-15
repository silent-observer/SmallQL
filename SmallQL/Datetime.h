#pragma once

#include <cstdint>
#include <iostream>
#include <ctime>

using namespace std;

struct Datetime {
    int16_t year;
    uint8_t month, day, hour, minute, second;
    Datetime(int16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
        : year(year), month(month), day(day), hour(hour), minute(minute), second(second) {}
    Datetime(int16_t year, uint8_t month, uint8_t day)
        : year(year), month(month), day(day), hour(0), minute(0), second(0) {}
    Datetime(uint8_t hour, uint8_t minute, uint8_t second)
        : year(0), month(0), day(0), hour(hour), minute(minute), second(second) {}
    Datetime()
        : year(0), month(0), day(0), hour(0), minute(0), second(0) {}
    Datetime(const tm& tmData, bool hasDate, bool hasTime);
    tm toTM() const;
    int compare(const Datetime& that) const;
    string toString() const;
};

ostream& operator<<(ostream& os, const Datetime& datetime);