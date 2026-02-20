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


    MyOpenSky sky;
    std::vector<Flight> ksmf_arrivals = sky.get_arrivals("KSMF", "SMF", 38.2, -122.1, 39.2, -121.1);

    return 0;
}