#include <iostream>
#include <string>
#include <curl/curl.h>
#include <../include/json.hpp>
#include <../include/OpenSky.h>
#include <fstream>
#include <vector>
#include <ctime>

using std::cout;
using std::endl;

using json = nlohmann::json;

namespace{
    size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
        std::string *buffer = (std::string*) userdata;
        buffer->append(ptr, size * nmemb);
        return size * nmemb;
    }
}

MyOpenSky::MyOpenSky(){
    std::ifstream cred_file("secrets/credentials.json");
    if(cred_file.is_open()){
        json cred = json::parse(cred_file);
        this->client_id = cred["client_id"];
        this->client_secret = cred["client_secret"];
        this->authenticate();
    }
}


bool MyOpenSky::authenticate(){
    CURL* curl = curl_easy_init();

    std::string response;

    if(curl){
        std::string auth_endpoint = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
        std::string post_fields = "grant_type=client_credentials&client_id=" + this->client_id + "&client_secret=" + this->client_secret;
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        curl_easy_setopt(curl, CURLOPT_URL, auth_endpoint.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            auto token_data = json::parse(response);
            this->saved_token = token_data["access_token"];
            this->token_time = std::time(0);

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return true;
        }
        else if(res == CURLE_OPERATION_TIMEDOUT){
            cout<<"The request timed out."<<endl;
            return false;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }
    return false;
}

std::vector<Flight> MyOpenSky::get_arrivals(const std::string airport_icao){

    time_t current_time = std::time(0);
    if((current_time - this->token_time) > (25 * 60) || this->saved_token.empty() == true){
        this->authenticate();//if the token is close to expire, authenticate a new one.
    }

    std::vector<Flight> arrival_list;
    CURL* curl = curl_easy_init();
    std::string saved_data;
    
    if(curl){
        time_t now = std::time(0);
        time_t three_hours_later = now + (3 * 3600);

        std::string url = "https://opensky-network.org/api/flights/arrival?airport=" + airport_icao + "&begin=" + std::to_string(now) + "&end=" + std::to_string(three_hours_later);
        
        struct curl_slist *headers = NULL;
        std::string auth_header = "Authorization: Bearer " + this->saved_token; 
        headers = curl_slist_append(headers, auth_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &saved_data);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            auto data = json::parse(saved_data);
            for(auto& item : data){
                Flight f;
                f.icao24 = item.value("icao24", "N/A");
                f.callsign = item.value("callsign", "N/A");
                f.depart_airport = item.value("estDepartureAirport", "Unknown");
                f.arrival_airport = item.value("estArrivalAirport", "Unknown");
                f.est_depart_time = item.value("firstSeen", 0);
                f.est_arrival_time = item.value("lastSeen", 0);
                arrival_list.push_back(f);
            }
        }
        else if(res == CURLE_OPERATION_TIMEDOUT){
            cout<<"The request timed out."<<endl;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return arrival_list;
}