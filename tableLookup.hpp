#ifndef TABLELOOKUP_HPP
#define TABLELOOKUP_HPP

#include <string>
#include <vector>

//Data that I need to pull from each row
struct countyRecord {
    std::string countyName;
    std::string stateName;
    double latitude;
    double longitude;
};

std::vector<countyRecord> loadCountyCsv(const std::string& path);

const countyRecord* findCounty(const std::vector<countyRecord>& data, const std::string& county, const std::string& state = "");

#endif
