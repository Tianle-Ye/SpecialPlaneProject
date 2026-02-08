#include <iostream>
#include <string>
#include <curl/curl.h>
#include <../include/OpenWeather.h>
#include <iomanip>

using std::cout;
using std::endl;

#define MS_TO_KNOTS 1.94384

int main(){
    std::string open_weather_api = "92bcdf3d881aaec304a95f8bffadb962";
    WindInfo ksmf_wind = get_ksmf_wind(open_weather_api);
    double knots = ksmf_wind.speed * MS_TO_KNOTS;
    if(ksmf_wind.state == true){
        cout<<"current ksmf wind speed: "<<ksmf_wind.speed<<" m/s ("<<std::fixed<<std::setprecision(2)<<knots<<" knots)"<<endl;
        cout<<"current ksmf wind direction: "<<ksmf_wind.deg<<" degrees"<<endl;
    }
    else{
        cout<<"fail to obtain wind data."<<endl;
    }
    return 0;
}