#include "tableLookup.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

static std::string trimStr(const std::string& s) {
    const auto front = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
    const auto back = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();

    return (back <= front) ? std::string() : std::string(front, back);
}

//Splits CSV lines into fields
static std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    bool inQuotes = false;

    for(char ch : line) {
        if(ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if(ch == ',' && !inQuotes) {
            fields.push_back(trimStr(cur));
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    fields.push_back(cur);
    return fields;
}

std::vector<countyRecord> loadCountyCsv(const std::string& path) {
    std::ifstream infile(path);
    if(!infile) {
        throw std::runtime_error("Unable to open CSV file: " + path);
    }

    std::vector<countyRecord> records;
    std::string line;
    bool firstLine = true;

    while(std::getline(infile, line)) {
        if(line.empty()) continue;

        if(firstLine) {
            auto maybeHeader = splitCsvLine(line);
            if(!maybeHeader.empty() && !(std::isdigit(maybeHeader[0][0]) || maybeHeader[0][0] == '-')) {
                firstLine = false;
                continue;
            }
        }
        firstLine = false;

        auto fields = splitCsvLine(line);
        if(fields.size() < 4) {
            std::cerr << "Warning: Line with <4 columns ignored: " << line << "\n";
            continue;
        }

        countyRecord rec;
        rec.countyName = fields[5];
        rec.stateName = fields[0];

        try {
            rec.latitude = std::stod(fields[8]);
            rec.longitude = std::stod(fields[7]);
        } catch(const std::exception&) {
            std::cerr << "Warning: Numeric conversion failure for line: " << line << "\n";
            continue;
        }
        records.emplace_back(std::move(rec));
    }
    return records;
}

static bool icompare(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char ca, char cb) {return std::tolower(static_cast<unsigned char>(ca)) == std::tolower(static_cast<unsigned char>(cb)); });

}

//Search function
const countyRecord* findCounty(const std::vector<countyRecord>& data, const std::string& county, const std::string& state) {
    for(const auto& r : data) {
        if(icompare(r.countyName, county)) {
            if(state.empty() || icompare(r.stateName, state))
                return &r;
        }
    }

    return nullptr; //if not found
}
