// States and provinces: names, codes, rough centres, sales regions, and the tile-grid layout used by the territory map.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace ctc {

struct Place { const char* code; const char* name; double lat, lon; const char* region; int gx, gy; };   // gx, gy: tile-grid column and row
inline const std::vector<Place>& places() {
    static const std::vector<Place> p = {
        {"AK", "Alaska", 64.0, -152.0, "West", 0, 0}, {"ME", "Maine", 45.3, -69.2, "Northeast", 11, 0},
        {"VT", "Vermont", 44.0, -72.7, "Northeast", 10, 1}, {"NH", "New Hampshire", 43.7, -71.6, "Northeast", 11, 1},
        {"WA", "Washington", 47.4, -120.5, "West", 1, 2}, {"ID", "Idaho", 44.4, -114.6, "West", 2, 2}, {"MT", "Montana", 47.0, -109.6, "West", 3, 2}, {"ND", "North Dakota", 47.5, -100.5, "Midwest", 4, 2},
        {"MN", "Minnesota", 46.3, -94.3, "Midwest", 5, 2}, {"IL", "Illinois", 40.0, -89.2, "Midwest", 6, 2}, {"WI", "Wisconsin", 44.6, -89.9, "Midwest", 7, 2}, {"MI", "Michigan", 44.3, -85.4, "Midwest", 8, 2},
        {"NY", "New York", 42.9, -75.5, "Northeast", 9, 2}, {"RI", "Rhode Island", 41.7, -71.5, "Northeast", 10, 2}, {"MA", "Massachusetts", 42.3, -71.8, "Northeast", 11, 2},
        {"OR", "Oregon", 43.9, -120.6, "West", 1, 3}, {"NV", "Nevada", 39.3, -116.6, "West", 2, 3}, {"WY", "Wyoming", 43.0, -107.5, "West", 3, 3}, {"SD", "South Dakota", 44.4, -100.2, "Midwest", 4, 3},
        {"IA", "Iowa", 42.1, -93.5, "Midwest", 5, 3}, {"IN", "Indiana", 39.9, -86.3, "Midwest", 6, 3}, {"OH", "Ohio", 40.3, -82.8, "Midwest", 7, 3}, {"PA", "Pennsylvania", 40.9, -77.8, "Northeast", 8, 3},
        {"NJ", "New Jersey", 40.1, -74.7, "Northeast", 9, 3}, {"CT", "Connecticut", 41.6, -72.7, "Northeast", 10, 3},
        {"CA", "California", 37.2, -119.7, "West", 1, 4}, {"UT", "Utah", 39.3, -111.7, "West", 2, 4}, {"CO", "Colorado", 39.0, -105.5, "West", 3, 4}, {"NE", "Nebraska", 41.5, -99.8, "Midwest", 4, 4},
        {"MO", "Missouri", 38.4, -92.5, "Midwest", 5, 4}, {"KY", "Kentucky", 37.5, -85.3, "South", 6, 4}, {"WV", "West Virginia", 38.6, -80.6, "South", 7, 4}, {"VA", "Virginia", 37.5, -78.8, "South", 8, 4},
        {"MD", "Maryland", 39.0, -76.8, "South", 9, 4}, {"DE", "Delaware", 39.0, -75.5, "South", 10, 4},
        {"AZ", "Arizona", 34.3, -111.7, "West", 2, 5}, {"NM", "New Mexico", 34.4, -106.1, "West", 3, 5}, {"KS", "Kansas", 38.5, -98.4, "Midwest", 4, 5}, {"AR", "Arkansas", 34.9, -92.4, "South", 5, 5},
        {"TN", "Tennessee", 35.9, -86.4, "South", 6, 5}, {"NC", "North Carolina", 35.5, -79.4, "South", 7, 5}, {"SC", "South Carolina", 33.9, -80.9, "South", 8, 5}, {"DC", "District of Columbia", 38.9, -77.0, "South", 9, 5},
        {"OK", "Oklahoma", 35.6, -97.5, "South", 4, 6}, {"LA", "Louisiana", 31.1, -92.0, "South", 5, 6}, {"MS", "Mississippi", 32.7, -89.7, "South", 6, 6}, {"AL", "Alabama", 32.8, -86.8, "South", 7, 6}, {"GA", "Georgia", 32.7, -83.4, "South", 8, 6},
        {"HI", "Hawaii", 20.8, -156.3, "West", 0, 7}, {"TX", "Texas", 31.5, -99.3, "South", 4, 7}, {"FL", "Florida", 28.6, -82.4, "South", 9, 7},
        {"BC", "British Columbia", 53.7, -127.6, "Canada", 1, 9}, {"AB", "Alberta", 55.0, -115.0, "Canada", 2, 9}, {"SK", "Saskatchewan", 54.0, -106.0, "Canada", 3, 9}, {"MB", "Manitoba", 55.0, -97.0, "Canada", 4, 9},
        {"ON", "Ontario", 50.0, -85.0, "Canada", 5, 9}, {"QC", "Quebec", 52.0, -72.0, "Canada", 6, 9}, {"NB", "New Brunswick", 46.5, -66.5, "Canada", 7, 9}, {"NS", "Nova Scotia", 45.0, -63.0, "Canada", 8, 9},
        {"PE", "Prince Edward Island", 46.4, -63.2, "Canada", 9, 9}, {"NL", "Newfoundland and Labrador", 53.5, -60.0, "Canada", 10, 9}};
    return p;
}
inline std::string lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
inline std::string trim(const std::string& s) { size_t a = 0, b = s.size(); while (a < b && std::isspace((unsigned char)s[a])) ++a; while (b > a && std::isspace((unsigned char)s[b - 1])) --b; return s.substr(a, b - a); }
// "California", "calif.", "ca" -> "CA"; unknown text -> ""
inline std::string stateCode(const std::string& in) {
    std::string t; for (char c : in) if (std::isalnum((unsigned char)c) || c == ' ') t += (char)std::tolower((unsigned char)c); t = trim(t); if (t.empty()) return "";
    static const std::map<std::string, std::string> alias = {{"calif", "CA"}, {"cali", "CA"}, {"penn", "PA"}, {"penna", "PA"}, {"mass", "MA"}, {"wash", "WA"}, {"tex", "TX"}, {"fla", "FL"}, {"quebec", "QC"}, {"pq", "QC"}, {"newfoundland", "NL"}, {"nf", "NL"}, {"washington dc", "DC"}, {"washington d c", "DC"}, {"dc", "DC"}};
    for (auto& p : places()) { if (t == lower(p.code) || t == lower(p.name)) return p.code; }
    auto it = alias.find(t); return it != alias.end() ? it->second : "";
}
inline const Place* findPlace(const std::string& code) { for (auto& p : places()) if (code == p.code) return &p; return nullptr; }
inline std::string regionOf(const std::string& code) { const Place* p = findPlace(code); return p ? p->region : "Other"; }
inline const std::vector<std::string>& regionNames() { static const std::vector<std::string> r = {"Northeast", "South", "Midwest", "West", "Canada"}; return r; }

// a rough outline of the lower 48 (lon, lat) used as a faint backdrop on the pin map; a real boundary file can replace it
inline const std::vector<std::pair<double, double>>& conusOutline() {
    static const std::vector<std::pair<double, double>> o = {
        {-124.7, 48.4}, {-123.0, 48.3}, {-122.8, 49.0}, {-117.0, 49.0}, {-110.0, 49.0}, {-104.0, 49.0}, {-97.2, 49.0}, {-95.2, 49.4}, {-94.6, 48.7}, {-92.0, 48.3}, {-89.6, 48.0}, {-88.4, 48.3}, {-84.9, 46.9}, {-84.5, 46.5},
        {-83.6, 46.1}, {-83.9, 45.9}, {-84.7, 45.8}, {-86.2, 44.7}, {-86.4, 43.6}, {-86.5, 42.2}, {-84.8, 41.7}, {-83.4, 41.7}, {-82.4, 41.5}, {-81.0, 42.0}, {-79.0, 42.8}, {-79.0, 43.3}, {-76.2, 43.6}, {-75.0, 44.9}, {-71.5, 45.0},
        {-70.0, 46.7}, {-69.2, 47.4}, {-67.8, 47.1}, {-67.8, 45.7}, {-67.0, 44.9}, {-68.8, 44.3}, {-70.2, 43.6}, {-70.7, 42.7}, {-70.0, 41.8}, {-71.5, 41.4}, {-73.7, 41.0}, {-74.0, 40.5}, {-74.1, 39.7}, {-75.1, 38.5}, {-75.9, 37.0},
        {-76.0, 36.9}, {-75.5, 35.3}, {-76.5, 34.7}, {-77.9, 33.9}, {-79.2, 33.2}, {-80.9, 32.0}, {-81.4, 30.7}, {-80.6, 28.5}, {-80.0, 26.8}, {-80.4, 25.2}, {-81.2, 25.2}, {-82.0, 26.5}, {-82.8, 28.0}, {-82.7, 29.2}, {-84.0, 30.1},
        {-85.4, 29.7}, {-86.5, 30.4}, {-88.0, 30.3}, {-89.4, 30.2}, {-89.2, 29.2}, {-90.5, 29.1}, {-92.0, 29.6}, {-93.8, 29.7}, {-94.7, 29.3}, {-96.4, 28.3}, {-97.2, 26.9}, {-97.2, 25.9}, {-99.1, 26.4}, {-99.5, 27.6},
        {-100.5, 28.7}, {-101.4, 29.8}, {-102.4, 29.8}, {-103.1, 28.97}, {-104.5, 29.6}, {-104.9, 30.6}, {-106.5, 31.8}, {-108.2, 31.8}, {-108.2, 31.3}, {-111.1, 31.3}, {-114.8, 32.5}, {-117.1, 32.5}, {-117.3, 33.2}, {-118.5, 34.0},
        {-120.5, 34.5}, {-121.9, 36.6}, {-122.5, 37.8}, {-123.8, 39.4}, {-124.4, 40.4}, {-124.2, 42.0}, {-124.5, 43.0}, {-124.0, 46.2}, {-124.7, 48.4}};
    return o;
}

// where an account without coordinates is drawn: its state's centre, nudged by a stable offset so a state's pins do not sit exactly on top of each other
inline void approxLatLon(const std::string& state, unsigned seed, double& lat, double& lon) {
    const Place* p = findPlace(state); if (!p) { lat = 39.0; lon = -98.0; return; }
    const double a = (seed * 2654435761u % 1000) / 1000.0 * 6.2831853, r = ((seed * 40503u) % 1000) / 1000.0 * 1.4; lat = p->lat + std::sin(a) * r * 0.7; lon = p->lon + std::cos(a) * r;
}
// Web-Mercator style plane used by the pin map (x in 0..1 across the world)
inline void project(double lat, double lon, double& x, double& y) { lat = std::clamp(lat, -85.0, 85.0); x = (lon + 180.0) / 360.0; const double s = std::sin(lat * 0.0174532925); y = 0.5 - std::log((1 + s) / (1 - s)) / (4 * 3.14159265358979); }

}  // namespace ctc
