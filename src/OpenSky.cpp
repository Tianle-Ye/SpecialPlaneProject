#include <iostream>
#include <string>
#include <curl/curl.h>
#include <../include/json.hpp>
#include <../include/OpenSky.h>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <ctime>
#include <memory>

using std::cout;
using std::endl;

using json = nlohmann::json;

std::string trim(const std::string& s) {
    size_t first = s.find_first_not_of(' ');
    if (std::string::npos == first) return s;
    size_t last = s.find_last_not_of(' ');
    return s.substr(first, (last - first + 1));
}

struct MyOpenSky::SImplementation{
    
    std::chrono::system_clock::time_point last_airlabs_update;
    std::string airlabs_key = "b9e38b7f-850d-4be7-ae27-47090f0be0ed";
    std::string client_id;
    std::string client_secret;
    std::string saved_token;
    time_t token_time;

    std::unordered_set<std::string> today_arr_callsigns;
    std::unordered_map<std::string, std::string> special_planes;
    std::unordered_map<std::string, Flight> detail_cache;

    static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
        std::string *buffer = static_cast<std::string*>(userdata);
        buffer->append(ptr, size * nmemb);
        return size * nmemb;
    }

    void load_specials(const std::string& airport_icao){
        std::ifstream file("data/special_planes.json");
        if(!file.is_open()){ 
            cout<<"ERROR: Cannot find special_planes.json at data"<<endl;
        }
        try{
            json special_data = json::parse(file);
            special_planes.clear();
            if(special_data.contains(airport_icao)){
                for(auto& item : special_data[airport_icao]){
                    std::string hex = item.value("hex_num", "");
                    std::transform(hex.begin(), hex.end(), hex.begin(), [](unsigned char c){
                        return std::tolower(c);
                    });
                    std::string desc = item.value("description", "");
                    if(!hex.empty()){
                        special_planes[hex] = desc;
                    }
                }
                cout<<"Successfully loaded special planes for "<<airport_icao<<endl;
            }
        }
        catch(json::parse_error& e){
            cout<<"Parse error: "<<e.what()<<endl;
        }
    }

    void update_daily_schdule(const std::string& airport_iata){
        auto now = std::chrono::system_clock::now();
        auto today = std::chrono::floor<std::chrono::days>(now);
        
        static std::chrono::system_clock::time_point last_day = std::chrono::system_clock::time_point::min();

        if(today != last_day){
            last_day = today;
            get_daily_schedule(airport_iata);
            cout<<"Daily schedule updated for: "<<airport_iata<<endl;
        }
    }

    void get_daily_schedule(const std::string& airport_iata){

        CURL* curl = curl_easy_init();
        std::string response;
        auto now = std::chrono::system_clock::now();

        if(curl){
            std::string url = "https://airlabs.co/api/v9/schedules?arr_iata=" + airport_iata + "&api_key=" + airlabs_key;

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            CURLcode res = curl_easy_perform(curl);

            if(res == CURLE_OK){
                auto data = json::parse(response);
                if(data.contains("response") && data["response"].is_array()){
                    today_arr_callsigns.clear();
                    for(auto &item : data["response"]){
                        std::string callsign = trim(item.value("flight_icao", ""));
                        if(!callsign.empty()){
                            // cout<<"Added to schedule:"<<callsign<<endl;
                            today_arr_callsigns.insert(callsign);
                        }
                    }
                    last_airlabs_update = now;
                    cout<<"Successfully loaded "<<today_arr_callsigns.size()<<" scheduled arrivals."<< endl;
                }
                else{
                    cout<<"Airlabs ERROR: "<<data.dump()<<endl;
                }
            }
            curl_easy_cleanup(curl);
        }
    }

    void get_detail(Flight& f){
        if (detail_cache.count(f.hex_num)) {
            f.depart_airport = detail_cache[f.hex_num].depart_airport;
            f.est_arrival_time = detail_cache[f.hex_num].est_arrival_time;
            return;
        }
        CURL* curl = curl_easy_init();
        std::string response;
        if(curl){
            std::string url = "https://airlabs.co/api/v9/flights?hex=" + f.hex_num + "&api_key=" + airlabs_key;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            CURLcode res = curl_easy_perform(curl);

            if(res == CURLE_OK){
                auto data = json::parse(response);
                if(data.is_array() && !data.empty()){
                    f.depart_airport = data[0].value("dep_icao", "N/A");
                    f.est_arrival_time = data[0].value("arr_time_ts", 0LL);
                }
                detail_cache[f.hex_num] = f;
            }
            else if(res == CURLE_OPERATION_TIMEDOUT){
                cout<<"The request timed out."<<endl;
            }
            curl_easy_cleanup(curl);
        }
    }

    bool authenticate(){
        CURL* curl = curl_easy_init();

        std::string response;

        if(curl){
            std::string auth_endpoint = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
            std::string post_fields = "grant_type=client_credentials&client_id=" + client_id + "&client_secret=" + client_secret;

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
                saved_token = token_data["access_token"];
                token_time = std::time(0);

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
};

MyOpenSky::MyOpenSky() : DImplementation(std::make_shared<SImplementation>()){
    DImplementation->load_specials("KSMF");
    std::ifstream cred_file("secrets/credentials.json");
    if(cred_file.is_open()){
        json cred = json::parse(cred_file);
        DImplementation->client_id = cred["clientId"];
        DImplementation->client_secret = cred["clientSecret"];
        DImplementation->authenticate();
    }
}

MyOpenSky::~MyOpenSky() = default;

std::vector<Flight> MyOpenSky::get_arrivals(const std::string& airport_icao, const std::string& airport_iata, double lamin, double lomin, double lamax, double lomax){

    DImplementation->update_daily_schdule(airport_iata);

    time_t current_time = std::time(0);
    if((current_time - DImplementation->token_time) > (25 * 60) || DImplementation->saved_token.empty() == true){
        DImplementation->authenticate();//if the token is close to expire, authenticate a new one.
    }

    std::vector<Flight> live_arrivals;
    CURL* curl = curl_easy_init();
    std::string response;
    
    if(curl){

        std::string url = "https://opensky-network.org/api/states/all?lamin=" + std::to_string(lamin) + "&lomin=" + std::to_string(lomin) + "&lamax=" + std::to_string(lamax) + "&lomax=" + std::to_string(lomax);
        
        struct curl_slist *headers = NULL;
        std::string auth_header = "Authorization: Bearer " + DImplementation->saved_token; 
        headers = curl_slist_append(headers, auth_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SImplementation::write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            auto data = json::parse(response);
            //cout<<"data: "<<data<<endl;
            if(data.contains("states") && data["states"].is_array()){
                for(auto& s : data["states"]){
                    std::string hex = s[0].get<std::string>();
                    std::string callsign = s[1].is_null() ? "" : s[1].get<std::string>();
                    callsign = trim(callsign);
                    if(DImplementation->today_arr_callsigns.count(callsign)){
                        Flight f;
                        f.callsign = callsign;
                        f.hex_num = hex;
                        f.latitude = s[6].is_null() ? 0.0 : s[6].get<double>();
                        f.longitude = s[5].is_null() ? 0.0 : s[5].get<double>();
                        f.altitude = s[7].is_null() ? 0.0 : s[7].get<double>();
                        double alt = f.altitude;
                        bool on_ground = s[8].get<bool>();
                        double v_rate = s[11].is_null() ? 0.0 : s[11].get<double>();
                        const double KSMF_LAT = 38.696;
                        const double KSMF_LON = -121.591;
                        bool is_locally_relevant = (std::abs(f.latitude - KSMF_LAT) < 1.0 && std::abs(f.longitude - KSMF_LON < 1.0));
                        if(on_ground || alt < 50){
                            if(is_locally_relevant){
                                f.isdescending = false;
                                f.status_text = "Landed / Taxiing";
                            }
                            else{
                                continue;
                            }
                        }
                        else if(alt < 1000){
                            if(is_locally_relevant){
                                f.isdescending = false;
                                f.status_text = "> Approaching";
                            }
                            else{
                                continue;
                            }
                        }
                        else if(v_rate < -0.5){
                            f.isdescending = true;
                            f.status_text = is_locally_relevant ? "v Descending (to SMF)" : "v Descending (Transit)";
                        }
                        else{
                            f.isdescending = false;
                            f.status_text = "- En Route";
                        }
                        if(DImplementation->special_planes.count(hex)){
                            f.is_special = true;
                            f.description = DImplementation->special_planes[hex];
                            DImplementation->get_detail(f);
                            if(f.arrival_airport != "KSMF" && f.arrival_airport != "SMF"){
                                f.is_special = false;
                                f.status_text = "- Overflight (To " + f.arrival_airport + ")";
                            }
                            else{
                                if(f.isdescending && f.altitude < 5000){
                                    DImplementation->get_detail(f);
                                }
                                else{
                                    f.depart_airport = "PENDING";
                                    f.est_arrival_time = 0;
                                }
                            }
                        }
                        else{
                            f.is_special = false;
                        }
                        live_arrivals.push_back(f);
                    }
                }
            }
        }
        else if(res == CURLE_OPERATION_TIMEDOUT){
            cout<<"The request timed out."<<endl;
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return live_arrivals;
}