#ifndef OPEN_SKY_H
#define OPEN_SKY_H

#include <string>
#include <vector>

struct Flight{
    std::string icao24;
    std::string callsign;
    std::string depart_airport;
    std::string arrival_airport;
    time_t est_depart_time;
    time_t est_arrival_time;
};

class MyOpenSky{
    private:
        std::string client_id;
        std::string client_secret;
        std::string saved_token;

        time_t token_time;

        bool authenticate();
    public:
        MyOpenSky();
        std::vector<Flight> get_arrivals(const std::string airport_icao);
};

#endif