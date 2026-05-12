#include <iostream>
#include <string>
#include <curl/curl.h>
#include <../include/OpenWeather.h>
#include <../include/OpenSky.h>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cmath>

using std::cout;
using std::endl;

#define MS_TO_KNOTS 1.94384

std::string convert_time(const long long otime){
    if(otime <= 0){
        return "---";
    }
    time_t t = static_cast<time_t>(otime);
    struct tm *lt = std::localtime(&t);
    char buffer[10];
    std::strftime(buffer, sizeof(buffer), "%H:%M", lt);
    return std::string(buffer);
}

int main(){

    MyOpenWeather wind;
    MyOpenSky sky;

    while(true){
        std::cout<<"\033[2J\033[H"<<std::flush;
        wind.get_wind_info(38.696, -121.591);
        WindInfo ksmf_wind = wind.get_wind();
        double knots = ksmf_wind.speed * MS_TO_KNOTS;

        cout<<"========KSMF Wind Status and Special List========"<<endl;

        if(ksmf_wind.state == true){
            cout<<"current KSMF wind speed: "<<ksmf_wind.speed<<" m/s ("<<std::fixed<<std::setprecision(2)<<knots<<" knots)"<<endl;
            cout<<"current KSMF wind direction: "<<ksmf_wind.deg<<" degrees"<<endl;

            double rwy_true_hdg = 180.8;
            double angle_rad = (ksmf_wind.deg - rwy_true_hdg) * 3.14159265 / 180.0;
            double xw_comp = std::abs(knots * std::sin(angle_rad));

            cout << "Crosswind Component: " << std::fixed << std::setprecision(1) << xw_comp << " knots" << endl;

            //if crosswind > 20, print alerts
            if (xw_comp > 20.0) {
                cout << "\033[1;31;5m !!! HIGH CROSSWIND WARNING !!! \033[0m" << endl;
            }
        }
        else{
            cout<<"fail to retrieve KSMF wind info."<<endl;
        }

        cout<<"----------------------------------------"<<endl;

        std::vector<Flight> ksmf_arrivals = sky.get_arrivals("KSMF", "SMF", 34.906, -126.775, 42.930, -114.539, ksmf_wind.deg, knots);

        cout<<std::left<<std::setw(12)<<"CALLSIGN"
            <<std::setw(10)<<"FROM"
            <<std::setw(8)<<"RWY"     // 新增：跑道列
            <<std::setw(10)<<"ETA"
            <<"STATUS / SPECIAL DESCRIPTION"<<endl;

        if (ksmf_arrivals.empty()) {
            cout<<"\n[Status] No KSMF inbound traffic detected at this moment."<<endl;
        }
        else{
            for(const auto& f : ksmf_arrivals){
                cout<<std::left<<std::setw(12)<<f.callsign;
                std::string from_apt = (f.depart_airport.empty() || f.depart_airport == "PENDING") ? "N/A" : f.depart_airport;
                cout<<std::setw(10)<<from_apt;

                cout<<std::setw(8)<<f.predicted_runway;

                cout<<std::setw(10)<<convert_time(f.est_arrival_time);
                cout<<std::setw(25)<<f.status_text;
                if (f.is_special) {
                    cout<<" ★★★ [[ SPECIAL ]] "<<f.description;
                }
                cout<<endl;
            }
        }

        cout<<"========================================"<<endl;

        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        cout << "Last updated: " << std::put_time(std::localtime(&now_c), "%H:%M:%S") << endl;

        cout<<"wait 60s for update."<<endl;

        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
    
    return 0;
}