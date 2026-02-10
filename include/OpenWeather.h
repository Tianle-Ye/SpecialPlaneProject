#ifndef OPEN_WEATHER_H
#define OPEN_WEATHER_H

#include <string>

struct WindInfo{
    double speed;
    int deg;
    bool state;
};

class MyOpenWeather{
    private:
        std::string my_api;
        WindInfo current_wind_info;
    public:
        MyOpenWeather();
        void get_wind_info(double lat, double lon);
        WindInfo get_wind() const;
};

WindInfo get_ksmf_wind(const std::string open_weather_api);

#endif