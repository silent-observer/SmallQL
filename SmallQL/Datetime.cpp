#include "Datetime.h"
#include "Common.h"
#include <sstream>

Datetime::Datetime(const tm& tmData, bool hasDate, bool hasTime)
    : year(hasDate? tmData.tm_year + 1900 : 0)
    , month(hasDate? tmData.tm_mon : 0)
    , day(hasDate? tmData.tm_mday : 0)
    , hour(hasTime? tmData.tm_hour : 0)
    , minute(hasTime? tmData.tm_min : 0)
    , second(hasTime? tmData.tm_sec : 0) {}

tm Datetime::toTM() const {
    tm tmDate = {second, minute, hour, day, month, year - 1900};
    return tmDate;
}

ostream& operator<<(ostream& os, const Datetime& datetime) {
    char buffer[200];
    tm tmDate = datetime.toTM();
    strftime(buffer, sizeof buffer, "%F %T", &tmDate);
    os << buffer;
    return os;
}

int Datetime::compare(const Datetime& that) const {
    if (this->year != that.year) return cmp(this->year, that.year);
    if (this->month != that.month) return cmp(this->month, that.month);
    if (this->day != that.day) return cmp(this->day, that.day);
    if (this->hour != that.hour) return cmp(this->hour, that.hour);
    if (this->minute != that.minute) return cmp(this->minute, that.minute);
    if (this->second != that.second) return cmp(this->second, that.second);
    return 0;
}

string Datetime::toString() const {
    stringstream ss;
    ss << *this;
    return ss.str();
}