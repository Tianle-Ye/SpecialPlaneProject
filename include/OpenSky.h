#ifndef OPEN_SKY_H
#define OPEN_SKY_H

#include <string>

struct get_flight{
    std::string icao24;
    std::string callsign;
    std::string depart_airport;
    std::string arrival_airport;
    time_t est_depart_time;
    time_t est_arrival_time;
};

std::string get_opensky_token();

#endif