#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "tableLookup.hpp"

static size_t writeCallback(char* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), total);
    return total;
}

inline std::vector<std::string> wrapText(const std::string& text, size_t width) {
    if(width == 0)
        throw std::invalid_argument("Error in wrapText, Width cannot be Zero");
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string token;

    while(iss >> token) {
        size_t pos = 0;
        while((pos = token.find('\n')) != std::string::npos) {
            std::string before = token.substr(0, pos);
            if(!before.empty())
                words.push_back(before);
        words.emplace_back("\n");
        token.erase(0, pos + 1);
        }

        if(!token.empty())
            words.push_back(token);
    }

    //Now to reassemble lines with the separated words
    std::vector<std::string> lines;
    std::string currentLine;

    auto flushCurrent = [&]() {
        if(!currentLine.empty()) {
            lines.push_back(currentLine);
            currentLine.clear();
        }
    };

    for(const auto& w : words) {
        if(w == "\n") {
            flushCurrent();
            continue;
        }

        //Append
        std::size_t projected = currentLine.empty() ? w.length() : currentLine.length() + 1 + w.length();
        if(projected <= width) {
            if(!currentLine.empty())
                currentLine += ' ';
            currentLine += w;
        } else {
            //If it doesn't fit in the line, start a new line
            flushCurrent();
            currentLine = w;
        }
    }
    flushCurrent();
    return lines;

}

std::string getWeather(const std::string& url) {
    CURL* handler = curl_easy_init();
    if(!handler)
        throw std::runtime_error("Failed to initialize libcurl");

    std::string response;
    curl_easy_setopt(handler, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handler, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handler, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(handler, CURLOPT_USERAGENT, "NWSApi-Project/0.2 (+https://example.com/myapp)");
    curl_easy_setopt(handler, CURLOPT_TIMEOUT, 10L);

    CURLcode rc = curl_easy_perform(handler);
    if(rc != CURLE_OK) {
        std::string err = curl_easy_strerror(rc);
        curl_easy_cleanup(handler);
        throw std::runtime_error("Error running curl perform: : " + err);
    }
    curl_easy_cleanup(handler);
    return response;
}

void printForecast(const std::string& forecastJSON, std::string& countyArg, std::string& stateArg) {
    nlohmann::json root = nlohmann::json::parse(forecastJSON);

    //Now to find the data in the JSON code pulled from NWS
    if(!root.contains("properties") || !root["properties"].contains("periods")) {
        std::cerr << "Unexpected JSON Structure - Missing properties/periods";
        return;
    }

    std::cout << "---NWS Forecast for " << countyArg << " County " + stateArg + "---\n"
              << "Day                    |  Temp  | Forecast\n"
              << "---------------------------------------------------------\n";

    std::string detailedCast;
    const auto& periods = root["properties"]["periods"];
    for(const auto& p : periods) {
        int temp = p.value("temperature", 0);
        std::string name = p.value("name", "?");
        std::string shortCast = p.value("shortForecast", "?");
        std::string unit = p.value("temperatureUnit", "?");

        if(name == "tonight" || name == "Tonight")
            detailedCast = p.value("detailedForecast", "?");

        //Making a table
        std::cout << std::left << std::setw(22) << name << " | "
                  << std::right << std::setw(4) << temp << " " << unit << " | "
                  << shortCast << "\n";


    }
    //Added detailed cast for the night of. This is what the append function is for
    std::vector<std::string> appendedCast = wrapText(detailedCast, 80);
    std::cout << "\n\nForecast for tonight: \n";
    for(const auto& line : appendedCast) {
        std::cout << " " << line << '\n';
    }
}

void printAlerts(const std::string& alertsJSON, std::string& countyArg, std::string& stateArg) {
    nlohmann::json root = nlohmann::json::parse(alertsJSON);

    if(!root.contains("features")) {
        std::cerr << "Unexpected JSON Structure, 'features' not found.";
        return;
    }

    std::cout << "\n\n----Active Alerts for " + countyArg + " County " + stateArg + "----\n";

    std::string alertCast;
    const auto& features = root["features"];
    const auto& feature = features[0]; //For my own memory, this is an array and needs to be done this way. Could use for loop, but there's only one anyway
    const auto& properties = feature["properties"];

    std::string headLine = properties.value("headline", "?");
    std::string description = properties.value("description", "?");

    std::cout << "" + headLine + "\n\n" + description + "\n";
}


int main(int argc, char **argv) {
    //Checking for Arguments
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <CountyName> <StateName(XX)>\n";
        return 1;
    }

    std::string countyArg = argv[1];
    std::string stateArg = "";

    //Check for State override Arg
    if(argc == 3) {
        stateArg = argv[2];
        std::cout << "State Override\n";
    }

    //Now to load the CSV
    std::string csvPath = "./countyTable.csv";
    std::vector<countyRecord> table;
    try {
        table = loadCountyCsv(csvPath);
    } catch(const std::exception& e) {
        std::cerr << "Failed to load CSV " << e.what() << "\n";
        return 1;
    }

    const countyRecord* rec = findCounty(table, countyArg, stateArg);
    if(!rec) {
        std::cout << "County \"" << countyArg << "\" not found in " << csvPath << "\n";
        return 0;
    }

    std::cout << "Found County: " << rec->countyName
                                  << (rec->stateName.empty() ? "" : (" (" + rec->stateName + ")"))
                                  << "\nZone: " << rec->zoneCode
                                  << "\nLatitude: " << rec->latitude
                                  << "\nLongitude: " << rec->longitude << "\n\n";
    std::string currentLong = std::to_string(rec->latitude);
    std::string currentLat = std::to_string(rec->longitude);
    std::string currentZone = rec->zoneCode;
    std::string currentState = rec->stateName;

    std::string primaryURL = "https://api.weather.gov/points/" + currentLong + "," + currentLat;
    std::string alertURL = "https://api.weather.gov/alerts/active/zone/" + currentZone;

    std::string pointResp = getWeather(primaryURL);
    nlohmann::json pointJson = nlohmann::json::parse(pointResp);

    //Get the URL from Metadata
    std::string forecastURL = pointJson["properties"]["forecast"];
    std::cout << "Fetching forecast from: " << forecastURL << "\n\n";

    //Now get forecast
    std::string forecastResp = getWeather(forecastURL);
    printForecast(forecastResp, countyArg, stateArg);

    //now get Alerts
    std::string alertResp = getWeather(alertURL);
    printAlerts(alertResp, countyArg, stateArg);

    return 0;
}
