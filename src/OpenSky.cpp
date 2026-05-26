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
#include <sqlite3.h>
#include <cmath>
#include <deque>

using std::cout;
using std::endl;

using json = nlohmann::json;

namespace {
    // KSMF runway data
    const double SMF_RWY_TRUE_HDG = 180.8; 
    const double SMF_EAST_RWY_LON = -121.585; // 17L/35R
    const double SMF_WEST_RWY_LON = -121.597; // 17R/35L

    // likelihood calculation：using Gausian Function
    double calculate_likelihood_east(double current_lon) {
        // 1000000 is sensitivity coefficient，with deviation of 0.012 could generate distinguishable difference
        double dist_sq_east = std::pow(current_lon - SMF_EAST_RWY_LON, 2);
        double dist_sq_west = std::pow(current_lon - SMF_WEST_RWY_LON, 2);

        double l_east = std::exp(-dist_sq_east * 1000000); 
        double l_west = std::exp(-dist_sq_west * 1000000);
        
        // normalization
        return l_east / (l_east + l_west);
    }

    double get_prior_p_east(const std::string& callsign, const std::deque<bool>& history) {
        std::string carrier = callsign.substr(0, 3);
        double p_base_east = 0.5; // default to neutral

        // airline preferences
        if (carrier == "UAL" || carrier == "AAL" || carrier == "DAL" || carrier == "SKW") 
            p_base_east = 0.85; 
        else if (carrier == "SWA" || carrier == "ASA") 
            p_base_east = 0.15;

        if (history.empty()) return p_base_east;

        // recent historic trends
        double east_observed = std::count(history.begin(), history.end(), true);
        double p_history = east_observed / history.size();

        // 7:3 hybrid model
        return (0.7 * p_base_east) + (0.3 * p_history);
    }
}

void MyOpenSky::predict_runway(Flight& f, double wind_deg_true, double wind_speed_kts, const std::deque<bool>& history) {
    // 1. determine Flow
    const double SMF_RWY_35_TRUE_HDG = 360.8; // runway 35
    
    // get the angle of wind_deg with 17/35 runway
    double diff_17 = std::abs(wind_deg_true - SMF_RWY_TRUE_HDG);
    if (diff_17 > 180.0) diff_17 = 360.0 - diff_17;
    
    double diff_35 = std::abs(wind_deg_true - SMF_RWY_35_TRUE_HDG);
    if (diff_35 > 180.0) diff_35 = 360.0 - diff_35;
    
    std::string flow = (diff_17 < diff_35) ? "17" : "35";
    
    // Tailwind Override
    // hw_17 > 0 headwind，< 0 tailwind
    double angle_rad_17 = (wind_deg_true - SMF_RWY_TRUE_HDG) * 3.14159265 / 180.0;
    double hw_17 = wind_speed_kts * std::cos(angle_rad_17);
    
    if (flow == "17" && hw_17 < -5.0) {
        flow = "35"; 
    } else if (flow == "35" && hw_17 > 5.0) { // hw_17 > 5.0 means 35 tailwind < -5.0
        flow = "17"; 
    }

    // 2. Bayes reasoning
    double P_E = get_prior_p_east(f.callsign, history); 
    double L_E = calculate_likelihood_east(f.longitude); 

    // calculate for posterior probability
    double posterior_east = (L_E * P_E) / (L_E * P_E + (1.0 - L_E) * (1.0 - P_E));

    // 3. dynamic allocation
    double final_p_east;
    if (f.altitude > 4000) {
        final_p_east = P_E; 
    } else if (f.altitude < 1000) {
        // keep 10% Bayes to prevent deviation
        final_p_east = 0.1 * P_E + 0.9 * L_E; 
    } else {
        // use Bayes in middle altitude
        final_p_east = posterior_east;
    }

    // 4. Semantic Mapping
    bool is_east = (final_p_east > 0.5);
    char side;
    if (flow == "17") side = is_east ? 'L' : 'R';
    else side = is_east ? 'R' : 'L';

    // 5. output
    if (f.status_text.find("Landed") != std::string::npos) {
        f.predicted_runway = "-";
    } else {
        // higher than 5000ft
        f.predicted_runway = (f.altitude > 5000) ? flow : (flow + side);
    }
}

std::string trim(const std::string& s) {
    size_t first = s.find_first_not_of(' ');
    if (std::string::npos == first) return s;
    size_t last = s.find_last_not_of(' ');
    return s.substr(first, (last - first + 1));
}

std::deque<bool> landing_history; // true for east, false for west

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
    std::unordered_map<std::string, long long> schedule_times;
    std::unordered_map<std::string, std::string> schedule_origins;

    struct GroundedAircraft{
        std::string hex;
        std::string arrival_callsign;
        bool is_special;
        std::string description;
        long long landed_ts;
        bool is_woken_up;
    };
    std::unordered_map<std::string, GroundedAircraft> ground_fleet;

    std::string current_active_flow = "17"; // default flow
    int tailwind_violation_count = 0; 

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
            schedule_times.clear();
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
                        long long est_ts = item.value("arr_estimated_ts", 0LL);
                        std::string dep_icao = item.value("dep_icao", "");
                        if(!callsign.empty()){
                            today_arr_callsigns.insert(callsign);
                            schedule_times[callsign] = est_ts;
                            if(!dep_icao.empty()){
                                schedule_origins[callsign] = dep_icao;
                            }
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
        std::string hex_upper = f.hex_num;
        std::transform(hex_upper.begin(), hex_upper.end(), hex_upper.begin(), ::toupper);
        if(detail_cache.count(hex_upper)){
            if(f.v_rate > 1.0){
                detail_cache.erase(hex_upper);
            }
            else{
                f.arrival_airport = detail_cache[hex_upper].arrival_airport;
                f.depart_airport = detail_cache[hex_upper].depart_airport;
                f.is_special = true;
            }
            return;
        }
        CURL* curl = curl_easy_init();
        std::string response;
        if(curl){
            std::string url = "https://airlabs.co/api/v9/flights?hex=" + hex_upper + "&api_key=" + airlabs_key;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            CURLcode res = curl_easy_perform(curl);

            if(res == CURLE_OK){
                auto data = json::parse(response);
                // std::cout<<"[API TRACE] Hex: "<<hex_upper<<" Response: "<<response<<std::endl;
                if(data.contains("response") && data["response"].is_array() && !data["response"].empty()){
                    auto& flight_data = data["response"][0];
                    f.arrival_airport = flight_data.value("arr_icao", "N/A");
                    f.depart_airport = flight_data.value("dep_icao", "N/A");
                }
                detail_cache[hex_upper] = f;
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

    sqlite3* db = nullptr;
    void init_database(){
        if(sqlite3_open("data/Special_flights_history.db", &db) != SQLITE_OK){
            std::cerr<<"Can't open database: "<<sqlite3_errmsg(db)<<std::endl;
            return;
        }
        const char* sql = "CREATE TABLE IF NOT EXISTS arrivals ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "hex_num TEXT NOT NULL,"
                          "callsign TEXT,"
                          "description TEXT,"
                          "log_date DATE DEFAULT (CURRENT_DATE),"
                          "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
                          "UNIQUE(hex_num, log_date));";
        char* errMsg = 0;
        if(sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK){
            std::cerr<<"SQL error: "<<errMsg<<std::endl;
            sqlite3_free(errMsg);
        }
    }

    void log_flight(const Flight& f) {
        if(!db){
            return;
        }
        char* zSQL = sqlite3_mprintf(
            "INSERT OR IGNORE INTO arrivals (hex_num, callsign, description) VALUES (%Q, %Q, %Q);",
            f.hex_num.c_str(), f.callsign.c_str(), f.description.c_str()
        );

        sqlite3_exec(db, zSQL, 0, 0, nullptr);
        sqlite3_free(zSQL);
    }

    struct CrosswindStatus {
        double velocity;
        bool is_dangerous;
        std::string warning_msg;
    };

    CrosswindStatus check_crosswind(double wind_deg_true, double wind_speed_kts) {
        const double RWY_TRUE_HDG = 180.8;
        const double M_PI_b = 3.141592653589793;
        const double XW_LIMIT = 25.0;

        double angle_rad = (wind_deg_true - RWY_TRUE_HDG) * M_PI_b / 180.0;
        double xw_comp = std::abs(wind_speed_kts * std::sin(angle_rad));

        CrosswindStatus status;
        status.velocity = xw_comp;
        status.is_dangerous = (xw_comp > XW_LIMIT);
        
        if (status.is_dangerous) {
            status.warning_msg = "!!! CAUTION: HIGH CROSSWIND (" + std::to_string((int)xw_comp) + " KTS) !!!";
        }
        return status;
    }

    const size_t MAX_HISTORY = 20;

    // status check
    void update_history_if_landed(const Flight& f) {
        if (f.status_text == "Landed / Taxiing") {
            // determine based on longitude
            bool was_east = (f.longitude > -121.591); 
            landing_history.push_back(was_east);
            if (landing_history.size() > MAX_HISTORY) landing_history.pop_front();
        }
    }

    ~SImplementation(){
        if(db){
            sqlite3_close(db);
        }
    }
};

MyOpenSky::MyOpenSky() : DImplementation(std::make_shared<SImplementation>()){
    DImplementation->init_database();
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

std::vector<Flight> MyOpenSky::get_arrivals(const std::string& airport_icao, const std::string& airport_iata, double lamin, double lomin, double lamax, double lomax, double wind_deg, double wind_speed_kts){

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

                    double lat = s[6].is_null() ? 0.0 : s[6].get<double>();
                    double lon = s[5].is_null() ? 0.0 : s[5].get<double>();
                    double alt = s[7].is_null() ? 0.0 : s[7].get<double>();
                    bool on_ground = s[8].get<bool>();

                    const double KSMF_LAT = 38.696;
                    const double KSMF_LON = -121.591;
                    bool is_on_smf_surface = (on_ground || alt < 100) && (std::abs(lat - KSMF_LAT) < 0.03 && std::abs(lon - KSMF_LON) < 0.03);

                    bool is_inbound_flight = DImplementation->today_arr_callsigns.count(callsign);
                    bool is_tracked_on_ground = DImplementation->ground_fleet.count(hex);
                    
                    if(is_inbound_flight || is_tracked_on_ground || is_on_smf_surface){
                        Flight f;
                        f.callsign = callsign;
                        f.hex_num = hex;
                        f.latitude = lat;
                        f.longitude = lon;
                        f.altitude = alt;
                        f.track = s[10].is_null() ? 0.0 : s[10].get<double>();
                        f.v_rate = s[11].is_null() ? 0.0 : s[11].get<double>();

                        const double KSMF_LAT = 38.696;
                        const double KSMF_LON = -121.591;

                        double dLat = KSMF_LAT - f.latitude;
                        double dLon = (KSMF_LON - f.longitude) * std::cos(KSMF_LAT * 3.14159 / 180.0);
                        double bearing_to_smf = std::atan2(dLon, dLat) * 180.0 / 3.14159;
                        if(bearing_to_smf < 0) bearing_to_smf += 360.0;
                        
                        double track_diff = std::abs(f.track - bearing_to_smf);
                        if(track_diff > 180.0) track_diff = 360.0 - track_diff;
                        
                        double dist_sq = (f.latitude - KSMF_LAT)*(f.latitude - KSMF_LAT) + (f.longitude - KSMF_LON)*(f.longitude - KSMF_LON);

                        if(dist_sq > 0.25 && alt > 5000 && f.v_rate >= -0.1 && track_diff > 90.0){
                            continue;
                        }

                        bool is_locally_relevant = (std::abs(f.latitude - KSMF_LAT) < 1.0 && std::abs(f.longitude - KSMF_LON) < 1.0);
                        long long current_time = std::time(0);
                        long long last_pos_update = s[3].is_null() ? 0 : s[3].get<long long>();

                        if(on_ground || (alt < 50 && is_locally_relevant)){
                            f.status_text = "Landed / Taxiing";
                            f.predicted_runway = "-";
                            f.isdescending = false;
                            
                            if(!DImplementation->ground_fleet.count(hex)){
                                bool is_sp = DImplementation->special_planes.count(hex);
                                std::string desc = is_sp ? DImplementation->special_planes[hex] : "";
                                DImplementation->ground_fleet[hex] = {
                                    hex, callsign, is_sp, desc, std::time(0), false
                                };
                            }
                            if(DImplementation->ground_fleet.count(hex) && !DImplementation->ground_fleet[hex].is_woken_up &&
                               (callsign == DImplementation->ground_fleet[hex].arrival_callsign || callsign.empty())){
                                continue;
                            }
                        }
                        if(DImplementation->ground_fleet.count(hex)){
                            auto& parked_plane = DImplementation->ground_fleet[hex];
                            
                            bool is_callsign_changed = (!callsign.empty() && callsign != parked_plane.arrival_callsign);
                            bool is_taxiing_out = (on_ground && (is_callsign_changed || std::abs(f.track) > 0.1)); 

                            if(f.v_rate > 2.0 && alt > 1500){
                                DImplementation->ground_fleet.erase(hex); 
                                std::string hex_upper = hex;
                                std::transform(hex_upper.begin(), hex_upper.end(), hex_upper.begin(), ::toupper);
                                DImplementation->detail_cache.erase(hex_upper); 
                                continue; 
                            }

                            if(is_callsign_changed || is_taxiing_out){
                                parked_plane.is_woken_up = true;
                                if(parked_plane.is_special) {
                                    if(!is_callsign_changed){
                                        parked_plane.arrival_callsign = "WOKEN_UP_SPECIAL_SAME_CS";
                                    }
                                    else{
                                        parked_plane.arrival_callsign = "WOKEN_UP_SPECIAL_NEW_CS";
                                    }
                                }
                                continue;
                            }
                        }
                        if(f.v_rate > 2.0 && track_diff > 90.0){
                            continue; 
                        }
                        if(alt < 1000){
                            if((current_time - last_pos_update) > 25 && last_pos_update != 0){
                                f.status_text = "Landed (Signal Lost)";
                                f.isdescending = false;
                            }
                            else if(is_locally_relevant){
                                f.isdescending = false;
                                f.status_text = "> Approaching";
                            }
                            else{
                                continue;
                            }
                        }
                        else if(f.v_rate < -0.5){
                            f.isdescending = true;
                            f.status_text = is_locally_relevant ? "v Descending (to SMF)" : "v Descending";
                        }
                        else{
                            f.isdescending = false;
                            f.status_text = "- En Route";
                        }
                        if(DImplementation->special_planes.count(hex)){
                            f.is_special = true;
                            f.description = DImplementation->special_planes[hex];
                            // if((f.isdescending) || (f.status_text == "> Approaching") || (f.status_text == "Landed / Taxiing")){
                                DImplementation->get_detail(f);
                                if(f.arrival_airport == "KSMF" || f.arrival_airport == "SMF"){
                                    DImplementation->log_flight(f);
                                }
                                else if(is_locally_relevant == true && f.v_rate <= 0.0){
                                    DImplementation->log_flight(f);
                                }
                                else if(!f.arrival_airport.empty()){
                                    f.is_special = false;
                                    f.status_text = "- Overflight (To " + f.arrival_airport + ")";
                                }
                            // }
                        }
                        else{
                            f.is_special = false;
                        }
                        if(DImplementation->schedule_times.count(f.callsign)){
                            f.est_arrival_time = DImplementation->schedule_times[f.callsign];
                        }
                        if(DImplementation->schedule_origins.count(f.callsign)){
                            if(f.depart_airport.empty() || f.depart_airport == "PENDING" || f.depart_airport == "N/A"){
                                f.depart_airport = DImplementation->schedule_origins[f.callsign];
                            }
                        }
                        if (f.status_text == "Landed / Taxiing" || f.status_text == "Landed (Signal Lost)") {
                            // if already landed, set runway to "-" to eliminate misconception
                            f.predicted_runway = "-"; 
                        } else {
                            predict_runway(f, wind_deg, wind_speed_kts, landing_history);
                        }
                        live_arrivals.push_back(f);
                    }
                }
            }
        }
        else if(res == CURLE_OPERATION_TIMEDOUT){
            cout<<"The request timed out."<<endl;
        }
        std::vector<Flight> buffered_specials;
        for (const auto& pair : DImplementation->ground_fleet) {
            const auto& plane = pair.second;
            
            // Special plane, push onto the screen
            if (plane.is_special && !plane.is_woken_up) {
                Flight f;
                f.hex_num = plane.hex;
                f.callsign = plane.arrival_callsign;
                f.is_special = true;
                f.description = plane.description;
                f.predicted_runway = "-";
                
                int parked_mins = (std::time(0) - plane.landed_ts) / 60;
                f.status_text = "Landed (At Gate " + std::to_string(parked_mins) + "m ago)";
                
                if (DImplementation->schedule_origins.count(f.callsign)) {
                    f.depart_airport = DImplementation->schedule_origins[f.callsign];
                } else {
                    f.depart_airport = "N/A";
                }
                
                buffered_specials.push_back(f);
            }
        }
        
        live_arrivals.insert(live_arrivals.begin(), buffered_specials.begin(), buffered_specials.end());
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return live_arrivals;
}

std::vector<Flight> MyOpenSky::get_departures(double wind_deg, double wind_speed_kts){
    std::vector<Flight> live_departures;

    for(const auto& pair : DImplementation->ground_fleet){
        const auto& plane = pair.second;
        
        if(plane.is_special && (plane.arrival_callsign == "WOKEN_UP_SPECIAL_SAME_CS" || plane.arrival_callsign == "WOKEN_UP_SPECIAL_NEW_CS")){
            Flight f;
            f.callsign = plane.arrival_callsign == "WOKEN_UP_SPECIAL_SAME_CS" ? "PENDING" : plane.arrival_callsign; 
            f.status_text = (plane.arrival_callsign == "WOKEN_UP_SPECIAL_SAME_CS") ? "🚨 TAXI OUT (Same CS)" : "🚨 TAXI OUT";
            f.is_special = true;
            f.description = plane.description;
            
            predict_runway(f, wind_deg, wind_speed_kts, landing_history);
            live_departures.push_back(f);
        }
    }
    return live_departures;
}

void MyOpenSky::print_ground_fleet_summary() const{
    int total_parked_count = 0;
    int woken_up_departure_count = 0;

    for(const auto& pair : DImplementation->ground_fleet){
        const auto& plane = pair.second;
        
        if(plane.is_special){
            int parked_mins = (std::time(0) - plane.landed_ts) / 60;
            
            if(plane.arrival_callsign == "WOKEN_UP_SPECIAL_SAME_CS"){
                std::cout<<"\033[1;31m🚨 [SPECIAL OUT] " << plane.description<<" [CS_Unchanged] Taxiing!\033[0m"<<std::endl;
            }
            else if (plane.arrival_callsign == "WOKEN_UP_SPECIAL_NEW_CS") {
                std::cout<<"\033[1;31m🚨 [SPECIAL OUT] "<<plane.description<<" Taxiing!\033[0m"<<std::endl;
            }
            else {
                std::cout<<"\033[1;33m🌟 [SPECIAL PARKED] "<<plane.description<<" (Landed "<<parked_mins<<" min ago)\033[0m"<<std::endl;
            }
        } else{
            if(plane.is_woken_up){
                woken_up_departure_count++; 
            }
            else{
                total_parked_count++; 
            }
        }
    }

    if(woken_up_departure_count > 0){
        std::cout<<"\033[1;32m[Dep_updates] Currently "<<woken_up_departure_count<<" plane(s) ready for departure. \033[0m"<<std::endl;
    }
    std::cout<<"[Static Monitoring] Has "<<total_parked_count<<" normal planes."<<std::endl;
}