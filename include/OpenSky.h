#ifndef OPEN_SKY_H
#define OPEN_SKY_H

#include <string>
#include <vector>
#include <memory>

struct Flight{
    bool is_special = false;
    std::string hex_num;
    std::string callsign;
    std::string depart_airport;
    std::string arrival_airport;
    time_t est_depart_time;
    time_t est_arrival_time;
    double latitude;
    double longitude;
    double altitude;
    bool isdescending;
    std::string regi_num;
    std::string description;
    std::string status_text;
};

class MyOpenSky{
    private:
        struct SImplementation;
        std::shared_ptr<SImplementation> DImplementation;
    public:
        MyOpenSky();
        ~MyOpenSky();
        std::vector<Flight> get_arrivals(const std::string& airport_icao, const std::string& airport_iata, double lamin, double lomin, double lamax, double lomax);
};

#endif