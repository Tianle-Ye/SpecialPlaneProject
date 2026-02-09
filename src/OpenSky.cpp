#include <iostream>
#include <string>
#include <curl/curl.h>
#include <../include/json.hpp>
#include <fstream>
#include <vector>
#include <ctime>

using json = nlohmann::json;

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
    std::string *buffer = (std::string*) userdata;
    buffer->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string get_opensky_token(){
    std::ifstream cred_file("secrets/credentials.json");

    json cred = json::parse(cred_file);
    std::string client_id = cred["client_id"];
    std::string client_secret = cred["client_secret"];

    CURL* curl = curl_easy_init();

    std::string saved_token;

    if(curl){
        std::string auth_endpoint = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
        std::string post_fields = "grant_type=client_credentials&client_id=" + client_id + "&client_secret=" + client_secret;
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        curl_easy_setopt(curl, CURLOPT_URL, auth_endpoint.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &saved_token);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            auto token_data = json::parse(saved_token);
            std::string token = token_data["access_token"];

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            return token;
        }
        else if(res == CURLE_OPERATION_TIMEDOUT){
            cout<<"The request timed out."<<endl;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

std::vector<get_flight> get_ksmf_arrivals(const std::string token){
    std::vector<get_flight> arrival_list;
    CURL* curl = curl_easy_init(curl);
    std::string saved_data;
    
    if(curl){
        time_t now = std::time(0);
        time_t three_hours_later = now + (3 * 3600);

        std::string url = "https://opensky-network.org/api/flights/arrival?airport=KSMF&begin=" + now + "&end=" + three_hours_later;
        
        struct curl_slist *headers = NULL;
        std::string auth_header = "Authorization: Bearer " + token; 
        headers = curl_slist_append(headers, auth_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &saved_data);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            auto data = json::parse(saved_token);
            
        }
        else if(res == CURLE_OPERATION_TIMEDOUT){
            cout<<"The request timed out."<<endl;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return arrival_list;
}