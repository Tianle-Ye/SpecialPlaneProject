#ifndef OPEN_SKY_H
#define OPEN_SKY_H

#include <string>
#include <vector>
#include <memory>
#include <deque>

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
    double v_rate;
    bool isdescending;
    std::string regi_num;
    std::string description;
    std::string status_text;
    double track = 0.0;
    std::string predicted_runway = "N/A";
};

class MyOpenSky{
    private:
        struct SImplementation;
        std::shared_ptr<SImplementation> DImplementation;
    public:
        MyOpenSky();
        ~MyOpenSky();
        void predict_runway(Flight& f, double wind_deg_true, double wind_speed_kts, const std::deque<bool>& history);
        std::vector<Flight> get_arrivals(const std::string& airport_icao, const std::string& airport_iata, double lamin, double lomin, double lamax, double lomax, double wind_deg, double wind_speed_kts);
};

#endif