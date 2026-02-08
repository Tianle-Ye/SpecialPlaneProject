#ifndef OPEN_WEATHER_H
#define OPEN_WEATHER_H

#include <string>

struct WindInfo{
    double speed;
    int deg;
    bool state;
};

WindInfo get_ksmf_wind(const std::string open_weather_api);

#endif