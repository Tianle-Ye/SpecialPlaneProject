#include <iostream>
#include <string>
#include <curl/curl.h>
#include "../include/json.hpp"
#include "../include/OpenWeather.h"

using std::cout;
using std::endl;
using json = nlohmann::json;


size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
    std::string *buffer = (std::string*) userdata;
    buffer->append(ptr, size * nmemb);
    return size * nmemb;//return the number of bytes actually taken care of
}


WindInfo get_ksmf_wind(const std::string open_weather_api){
    WindInfo ksmf_info = {0.0, 0, false};
    CURL* curl = curl_easy_init();
    std::string api_call = "https://api.openweathermap.org/data/2.5/weather?lat=38.696&lon=-121.591&units=metric&appid="
                            + open_weather_api;
    std::string saved_data;

    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, api_call.c_str());//convert c++ string back to c style string since libcurl is based on c
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &saved_data);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            auto data = json::parse(saved_data);
            ksmf_info.speed = data["wind"]["speed"];
            ksmf_info.deg = data["wind"]["deg"];
            ksmf_info.state = true;
        }
        else if(res == CURLE_OPERATION_TIMEDOUT){
            cout<<"The request timed out."<<endl;
        }

        curl_easy_cleanup(curl);
    }
    
    return ksmf_info;
}