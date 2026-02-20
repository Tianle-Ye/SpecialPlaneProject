#ifndef OPEN_SKY_H
#define OPEN_SKY_H

#include <string>
#include <vector>
#include <memory>

struct Flight{
    std::string icao24;
    std::string callsign;
    std::string depart_airport;
    std::string arrival_airport;
    time_t est_depart_time;
    time_t est_arrival_time;
    double lattitude;
    double longitude;
};

class MyOpenSky{
    private:
        struct SImplementation;
        std::shared_ptr<SImplementation> DImplementation;
    public:
        MyOpenSky();
        ~MyOpenSky();
        std::vector<Flight> get_arrivals(const std::string airport_icao);
};

#endif