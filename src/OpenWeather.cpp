#include <iostream>
#include <string>
#include <curl/curl.h>
#include "../include/json.hpp"
#include "../include/OpenWeather.h"
#include "fstream"

using std::cout;
using std::endl;
using json = nlohmann::json;

namespace{
    size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
        std::string *buffer = (std::string*) userdata;
        buffer->append(ptr, size * nmemb);
        return size * nmemb;//return the number of bytes actually taken care of
    }
}

MyOpenWeather::MyOpenWeather(){
    std::ifstream weather_api_file("secrets/OpenWeatherAPI.json");
    if(weather_api_file.is_open()){
        json api = json::parse(weather_api_file);
        this->my_api = api["api_key"];
    }
}

void MyOpenWeather::get_wind_info(double lat, double lon){
    this->current_wind_info = {0.0, 0, false};
    CURL* curl = curl_easy_init();
    std::string saved_data;

    if(curl){
        std::string url = "https://api.openweathermap.org/data/2.5/weather?lat=" + std::to_string(lat) + "&lon=" + std::to_string(lon) + "&units=metric&appid="
                            + this->my_api;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());//convert c++ string back to c style string since libcurl is based on c
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &saved_data);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            auto data = json::parse(saved_data);
            this->current_wind_info.speed = data["wind"]["speed"];
            this->current_wind_info.deg = data["wind"]["deg"];
            this->current_wind_info.state = true;
        }
        else if(res == CURLE_OPERATION_TIMEDOUT){
            cout<<"The request timed out."<<endl;
        }

        curl_easy_cleanup(curl);
    }
    
}

WindInfo MyOpenWeather::get_wind() const{
    return this->current_wind_info;
}