#ifndef OPEN_WEATHER_H
#define OPEN_WEATHER_H

#include <string>
#inlcude <memory>

struct WindInfo{
    double speed;
    int deg;
    bool state;
};

class MyOpenWeather{
    private:
        struct SImplementation;
        std::shared_ptr<SImplementation> DImplementation;
    public:
        MyOpenWeather();
        ~MyOpenWeather();
        void get_wind_info(double lat, double lon);
        WindInfo get_wind() const;
};

WindInfo get_ksmf_wind(const std::string open_weather_api);

#endif