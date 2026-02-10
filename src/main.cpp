#include <iostream>
#include <string>
#include <curl/curl.h>
#include <../include/OpenWeather.h>
#include <../include/OpenSky.h>
#include <iomanip>

using std::cout;
using std::endl;

#define MS_TO_KNOTS 1.94384

int main(){


    MyOpenSky sky;
    MyOpenWeather wind;

    wind.get_wind_info(38.696, -121.591);
    WindInfo ksmf_wind = wind.get_wind();
    double knots = ksmf_wind.speed * MS_TO_KNOTS;
    if(ksmf_wind.state == true){
        cout<<"current ksmf wind speed: "<<ksmf_wind.speed<<" m/s ("<<std::fixed<<std::setprecision(2)<<knots<<" knots)"<<endl;
        cout<<"current ksmf wind direction: "<<ksmf_wind.deg<<" degrees"<<endl;
    }
    else{
        cout<<"fail to get wind info."<<endl;
    }




    std::vector<Flight> ksmf_arrivals = sky.get_arrivals("KSMF");

    for(auto& f : ksmf_arrivals){
        if(f.icao24 == "8C3200"){
            cout<<"Find Air Transport B767! "
                <<"| icao24: "<<f.icao24
                <<" | callsign: "<<f.callsign
                <<" | from "<<f.depart_airport
                <<" to "<<f.arrival_airport
                <<" | estimate arrival time: "<<f.est_arrival_time
                <<" | estimate runway selection: "<<endl;
        }
    }

    return 0;
}