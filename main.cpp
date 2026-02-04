#include "json.hpp"
#include <iostream>
#include <curl/curl.h>

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){
    std::string *buffer = (std::string*) userdata;
    buffer->append(ptr, size * nmemb)
    return size * nmemb;//return the number of bytes actually taken care of
}

int main(){
    return 0;
}