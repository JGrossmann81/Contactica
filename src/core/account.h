// The data: accounts (shops we are trying to win), their stage, activity log, and the database file.
#pragma once
#include <chrono>
#include <ctime>
#include <fstream>
#include <set>
#include "../../third_party/json.hpp"
#include "geo.h"

namespace ctc {

using json = nlohmann::json;
enum class Stage { Prospect, Contacted, Interested, Quoted, Won, Lost };
inline const char* stageName(Stage s) { static const char* n[] = {"Prospect", "Contacted", "Interested", "Quoted", "Won", "Lost"}; return n[(int)s]; }
inline bool isActive(Stage s) { return s != Stage::Won && s != Stage::Lost; }
inline const std::vector<std::string>& accountTypes() { static const std::vector<std::string> t = {"Instrument retailer", "DJ / electronic shop", "Pro audio / recording", "Guitar shop", "Piano / keyboard", "Band & orchestra", "Music school", "Online retailer", "Distributor", "Other"}; return t; }

inline std::string today() { const std::time_t t = std::time(nullptr); std::tm tm{}; 
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char b[16]; std::strftime(b, sizeof(b), "%Y-%m-%d", &tm); return b; }
// days between two yyyy-mm-dd dates (b - a); 0 if either is malformed
inline int daysBetween(const std::string& a, const std::string& b) {
    auto parse = [](const std::string& s, std::tm& tm) { int y, m, d; if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false; tm = {}; tm.tm_year = y - 1900; tm.tm_mon = m - 1; tm.tm_mday = d; tm.tm_hour = 12; return true; };
    std::tm x{}, y{}; if (!parse(a, x) || !parse(b, y)) return 0; return (int)std::lround(std::difftime(std::mktime(&y), std::mktime(&x)) / 86400.0);
}
inline std::string addDays(const std::string& date, int n) {
    int y, m, d; if (std::sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return date; std::tm tm{}; tm.tm_year = y - 1900; tm.tm_mon = m - 1; tm.tm_mday = d + n; tm.tm_hour = 12; std::mktime(&tm); char b[16]; std::strftime(b, sizeof(b), "%Y-%m-%d", &tm); return b;
}

struct Touch { std::string date, note; };
struct Account {
    int id = 0; std::string name, type = "Instrument retailer", address, city, state, zip, country = "US", phone, email, website, contact, contactTitle, source, notes;
    double lat = NAN, lon = NAN; std::vector<std::string> brands;            // brands we are pitching to them
    Stage stage = Stage::Prospect; std::string added, nextFollowUp, closedDate, closeNote; double firstOrder = 0; std::vector<Touch> log;
    bool hasCoords() const { return !std::isnan(lat) && !std::isnan(lon); }
    bool active() const { return isActive(stage); }
    std::string region() const { return regionOf(state); }
    void where(double& la, double& lo) const { if (hasCoords()) { la = lat; lo = lon; } else approxLatLon(state, (unsigned)id * 7919u + 13u, la, lo); }
    bool dueBy(const std::string& date) const { return active() && !nextFollowUp.empty() && nextFollowUp <= date; }
};

inline json toJson(const Account& a) {
    json j; j["id"] = a.id; j["name"] = a.name; j["type"] = a.type; j["address"] = a.address; j["city"] = a.city; j["state"] = a.state; j["zip"] = a.zip; j["country"] = a.country; j["phone"] = a.phone; j["email"] = a.email; j["website"] = a.website;
    j["contact"] = a.contact; j["contactTitle"] = a.contactTitle; j["source"] = a.source; j["notes"] = a.notes; if (a.hasCoords()) { j["lat"] = a.lat; j["lon"] = a.lon; } j["brands"] = a.brands; j["stage"] = (int)a.stage; j["added"] = a.added;
    j["next"] = a.nextFollowUp; j["closed"] = a.closedDate; j["closeNote"] = a.closeNote; j["firstOrder"] = a.firstOrder; json lg = json::array(); for (auto& t : a.log) lg.push_back({{"d", t.date}, {"n", t.note}}); j["log"] = lg; return j;
}
inline Account accountFromJson(const json& j) {
    Account a; auto s = [&](const char* k, std::string& v) { if (j.contains(k) && j[k].is_string()) v = j[k].get<std::string>(); };
    a.id = j.value("id", 0); s("name", a.name); s("type", a.type); s("address", a.address); s("city", a.city); s("state", a.state); s("zip", a.zip); s("country", a.country); s("phone", a.phone); s("email", a.email); s("website", a.website); s("contact", a.contact);
    s("contactTitle", a.contactTitle); s("source", a.source); s("notes", a.notes); s("added", a.added); s("next", a.nextFollowUp); s("closed", a.closedDate); s("closeNote", a.closeNote);
    if (j.contains("lat") && j.contains("lon") && j["lat"].is_number() && j["lon"].is_number()) { a.lat = j["lat"].get<double>(); a.lon = j["lon"].get<double>(); }
    if (j.contains("brands")) for (auto& b : j["brands"]) if (b.is_string()) a.brands.push_back(b.get<std::string>());
    a.stage = (Stage)std::clamp(j.value("stage", 0), 0, 5); a.firstOrder = j.value("firstOrder", 0.0);
    if (j.contains("log")) for (auto& t : j["log"]) a.log.push_back({t.value("d", ""), t.value("n", "")}); return a;
}

struct Database {
    std::vector<Account> accounts; int nextId = 1; std::vector<std::string> myBrands; std::string territoryNote;
    Account* find(int id) { for (auto& a : accounts) if (a.id == id) return &a; return nullptr; }
    const Account* find(int id) const { for (auto& a : accounts) if (a.id == id) return &a; return nullptr; }
    int add(Account a) { a.id = nextId++; if (a.added.empty()) a.added = today(); a.state = stateCode(a.state).empty() ? trim(a.state) : stateCode(a.state); accounts.push_back(std::move(a)); return accounts.back().id; }
    bool remove(int id) { const auto n = accounts.size(); accounts.erase(std::remove_if(accounts.begin(), accounts.end(), [&](const Account& a) { return a.id == id; }), accounts.end()); return accounts.size() != n; }
    // moves an account out of the active list: it bought something (Won) or is not worth chasing (Lost)
    void close(int id, Stage s, const std::string& note, double firstOrder = 0, const std::string& date = "") { if (Account* a = find(id)) { a->stage = s; a->closedDate = date.empty() ? today() : date; a->closeNote = note; a->firstOrder = firstOrder; a->nextFollowUp.clear(); a->log.push_back({a->closedDate, std::string(s == Stage::Won ? "WON. " : "Closed as lost. ") + note}); } }
    void reopen(int id) { if (Account* a = find(id)) { a->stage = Stage::Contacted; a->closedDate.clear(); a->log.push_back({today(), "Reopened."}); } }
    void note(int id, const std::string& text, const std::string& date = "") { if (Account* a = find(id)) a->log.push_back({date.empty() ? today() : date, text}); }
    size_t activeCount() const { size_t n = 0; for (auto& a : accounts) if (a.active()) ++n; return n; }
};

inline json toJson(const Database& d) { json j; j["version"] = 1; j["nextId"] = d.nextId; j["myBrands"] = d.myBrands; j["territoryNote"] = d.territoryNote; json as = json::array(); for (auto& a : d.accounts) as.push_back(toJson(a)); j["accounts"] = as; return j; }
inline bool fromJson(const json& j, Database& d) {
    if (!j.is_object() || !j.contains("accounts")) return false; Database q; q.nextId = j.value("nextId", 1); if (j.contains("myBrands")) for (auto& b : j["myBrands"]) if (b.is_string()) q.myBrands.push_back(b.get<std::string>());
    q.territoryNote = j.value("territoryNote", ""); for (auto& a : j["accounts"]) { Account x = accountFromJson(a); if (x.id <= 0) x.id = q.nextId++; q.nextId = std::max(q.nextId, x.id + 1); q.accounts.push_back(std::move(x)); } d = std::move(q); return true;
}
// writes to a temp file first, keeps the previous copy as .bak, so a crash mid-save cannot lose the database
inline bool saveDatabase(const Database& d, const std::string& path) {
    const std::string tmp = path + ".tmp"; { std::ofstream f(tmp, std::ios::binary); if (!f) return false; f << toJson(d).dump(1); if (!f) return false; }
    std::remove((path + ".bak").c_str()); std::rename(path.c_str(), (path + ".bak").c_str()); if (std::rename(tmp.c_str(), path.c_str()) != 0) { std::rename((path + ".bak").c_str(), path.c_str()); return false; } return true;
}
inline bool loadDatabase(Database& d, const std::string& path) { std::ifstream f(path, std::ios::binary); if (!f) return false; auto j = json::parse(f, nullptr, false); if (j.is_discarded()) return false; return fromJson(j, d); }

}  // namespace ctc
