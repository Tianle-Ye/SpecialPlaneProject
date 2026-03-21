#include <iostream>
#include <string>
#include <curl/curl.h>
#include <../include/OpenWeather.h>
#include <../include/OpenSky.h>
#include <iomanip>

using std::cout;
using std::endl;

#define MS_TO_KNOTS 1.94384

std::string convert_time(const std::string& otime){
    if(otime == "0" || otime.empty()){
        return "TIME N/A";
    }
    time_t ootime = std::stoll(otime);
    struct tm *lt = std::localtime(&ootime);
    char buffer[10];
    std::strftime(buffer, sizeof(buffer), "%H:%M", lt);
    return std::string(buffer);
}

int main(){

    MyOpenWeather wind;

    wind.get_wind_info(38.696, -121.591);
    WindInfo ksmf_wind = wind.get_wind();

    double knots = ksmf_wind.speed * MS_TO_KNOTS;

    cout<<"========KSMF Wind Status and Special List========"<<endl;

    if(ksmf_wind.state == true){
        cout<<"current KSMF wind speed: "<<ksmf_wind.speed<<" m/s ("<<std::fixed<<std::setprecision(2)<<knots<<" knots)"<<endl;
        cout<<"current KSMF wind direction: "<<ksmf_wind.deg<<" degrees"<<endl;
    }
    else{
        cout<<"fail to retrieve KSMF wind info."<<endl;
    }

    cout<<"----------------------------------------"<<endl;

    MyOpenSky sky;
    std::vector<Flight> ksmf_arrivals = sky.get_arrivals("KSMF", "SMF", 33.906, -126.775, 42.930, -114.539);

    if(ksmf_arrivals.empty()){
        cout<<"No special plane are arriving currently."
    }
    else{
        cout<<std::left<<std::setw(10)<<"FROM"
            <<std::setw(12)<<"ETA"
            <<"SPECIAL DESCRIPTION"<<endl;

        for(const auto& f : ksmf_arrivals){
            if(f.is_special){
                cout<<std::left<<std::setw(10)<<(f.depart_airport.empty() ? "N/A" : f.depart_airport)
                    <<std::setw(10)<<convert_time(f.est_arrival_time);

                cout<<"**SPECIAL**"<<f.description;
            }
            cout<<endl;
        }
    }
    cout<<"========================================"<<endl;

    return 0;
}