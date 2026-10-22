// CSV in and out: tolerant reader (quotes, embedded line breaks, BOM, comma / semicolon / tab), column auto-mapping from common header names
// (including HubSpot and NetSuite exports), duplicate detection, and writers for the archive.
#pragma once
#include <cstring>
#include <sstream>
#include "account.h"

namespace ctc {

using Row = std::vector<std::string>;
inline std::vector<Row> parseCsv(std::string text, char delim = 0) {
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) text.erase(0, 3);
    if (!delim) { size_t end = text.find('\n'); const std::string first = text.substr(0, end); size_t c = std::count(first.begin(), first.end(), ','), s = std::count(first.begin(), first.end(), ';'), t = std::count(first.begin(), first.end(), '\t'); delim = (t > c && t > s) ? '\t' : (s > c ? ';' : ','); }
    std::vector<Row> rows; Row row; std::string cell; bool inQ = false, any = false;
    auto endCell = [&] { row.push_back(cell); cell.clear(); any = true; };
    auto endRow = [&] { if (any || !cell.empty()) { endCell(); bool blank = true; for (auto& c : row) if (!trim(c).empty()) blank = false; if (!blank) rows.push_back(row); } row.clear(); any = false; };
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (inQ) { if (c == '"') { if (i + 1 < text.size() && text[i + 1] == '"') { cell += '"'; ++i; } else inQ = false; } else cell += c; }
        else if (c == '"' && cell.empty()) inQ = true;
        else if (c == delim) endCell();
        else if (c == '\n') endRow();
        else if (c == '\r') { if (i + 1 < text.size() && text[i + 1] == '\n') ++i; endRow(); }
        else cell += c;
    }
    endRow(); return rows;
}
inline std::string csvEscape(const std::string& s) { if (s.find_first_of(",\"\r\n") == std::string::npos) return s; std::string o = "\""; for (char c : s) { if (c == '"') o += "\"\""; else o += c; } return o + "\""; }
inline std::string writeCsv(const std::vector<Row>& rows) { std::string o = "\xEF\xBB\xBF"; for (auto& r : rows) { for (size_t i = 0; i < r.size(); ++i) { if (i) o += ','; o += csvEscape(r[i]); } o += "\r\n"; } return o; }   // BOM so Excel reads accents correctly

// ---------------------------------------------------------------- mapping
enum class Field { None, Name, Type, Address, City, State, Zip, Country, Phone, Email, Website, Contact, ContactTitle, Brands, Source, Notes, Lat, Lon, Stage, NextFollowUp, Count };
inline const char* fieldLabel(Field f) { static const char* n[] = {"(skip)", "Shop name", "Type", "Street address", "City", "State / province", "ZIP / postal code", "Country", "Phone", "Email", "Website", "Contact person", "Contact title", "Brands", "Source", "Notes", "Latitude", "Longitude", "Stage", "Next follow-up"}; return n[(int)f]; }
inline std::string normHeader(const std::string& h) { std::string o; for (char c : h) if (std::isalnum((unsigned char)c)) o += (char)std::tolower((unsigned char)c); return o; }
inline Field guessField(const std::string& header) {
    static const std::vector<std::pair<Field, std::vector<std::string>>> syn = {
        {Field::Name, {"name", "shopname", "storename", "company", "companyname", "account", "accountname", "dealer", "dealername", "business", "businessname", "customer", "retailer", "store", "shop"}},
        {Field::Type, {"type", "category", "industry", "companytype", "businesstype", "segment", "channel"}},
        {Field::Address, {"address", "address1", "street", "streetaddress", "addr", "billingaddress", "billingstreet", "streetaddress1"}},
        {Field::City, {"city", "town", "billingcity"}}, {Field::State, {"state", "st", "province", "stateregion", "stateprovince", "billingstate", "region", "stateorprovince"}},
        {Field::Zip, {"zip", "zipcode", "postal", "postalcode", "postcode", "billingzip", "billingpostalcode"}}, {Field::Country, {"country", "countryregion", "billingcountry"}},
        {Field::Phone, {"phone", "phonenumber", "tel", "telephone", "mainphone", "mobile"}}, {Field::Email, {"email", "emailaddress", "contactemail", "mail"}},
        {Field::Website, {"website", "web", "url", "site", "domain", "companydomainname", "websiteurl", "homepage"}},
        {Field::Contact, {"contact", "contactname", "owner", "buyer", "contactperson", "primarycontact", "fullname", "manager"}}, {Field::ContactTitle, {"title", "jobtitle", "contacttitle", "position", "role"}},
        {Field::Brands, {"brands", "brand", "lines", "linescarried", "interestedin"}}, {Field::Source, {"source", "leadsource", "origin"}}, {Field::Notes, {"notes", "note", "comments", "comment", "description", "memo"}},
        {Field::Lat, {"lat", "latitude", "y"}}, {Field::Lon, {"lon", "lng", "long", "longitude", "x"}}, {Field::Stage, {"stage", "status", "leadstatus", "dealstage"}}, {Field::NextFollowUp, {"nextfollowup", "followup", "nextcontact", "nextstep", "followupdate", "nextactivitydate"}}};
    const std::string n = normHeader(header); if (n.empty()) return Field::None; for (auto& [f, names] : syn) for (auto& s : names) if (n == s) return f; return Field::None;
}
struct ImportPlan { std::vector<Field> columns; bool hasHeader = true; bool skipDuplicates = true; std::string defaultType = "Instrument retailer", defaultSource, defaultCountry = "US"; };
inline ImportPlan planFor(const std::vector<Row>& rows) {
    ImportPlan p; if (rows.empty()) return p; std::set<Field> used; for (auto& h : rows[0]) { Field f = guessField(h); if (f != Field::None && used.count(f)) f = Field::None; if (f != Field::None) used.insert(f); p.columns.push_back(f); }
    p.hasHeader = used.count(Field::Name) > 0 || used.size() >= 2; if (!p.hasHeader) { p.columns.assign(rows[0].size(), Field::None); } return p;
}

inline std::string digits(const std::string& s) { std::string o; for (char c : s) if (std::isdigit((unsigned char)c)) o += c; return o; }
inline std::string nameKey(const std::string& s) { std::string o; for (char c : lower(s)) if (std::isalnum((unsigned char)c)) o += c; for (const char* w : {"inc", "llc", "ltd", "co", "corp", "the"}) { const std::string x = w; if (o.size() > x.size() + 3 && o.compare(o.size() - x.size(), x.size(), x) == 0) o.erase(o.size() - x.size()); } return o; }
inline std::string domainOf(std::string u) { u = lower(trim(u)); for (const char* p : {"https://", "http://"}) if (u.rfind(p, 0) == 0) u = u.substr(strlen(p)); if (u.rfind("www.", 0) == 0) u = u.substr(4); const auto s = u.find_first_of("/?#"); if (s != std::string::npos) u = u.substr(0, s); return u; }
// two records are the same shop if the name and ZIP / city match, or the phone, or the website domain
inline bool sameShop(const Account& a, const Account& b) {
    if (nameKey(a.name).empty() || nameKey(b.name).empty()) return false;
    if (nameKey(a.name) == nameKey(b.name) && ((!a.zip.empty() && digits(a.zip).substr(0, 5) == digits(b.zip).substr(0, 5) && !digits(a.zip).empty()) || (lower(trim(a.city)) == lower(trim(b.city)) && !a.city.empty()))) return true;
    if (digits(a.phone).size() >= 10 && digits(a.phone).substr(digits(a.phone).size() - 10) == (digits(b.phone).size() >= 10 ? digits(b.phone).substr(digits(b.phone).size() - 10) : std::string("x"))) return true;
    const std::string da = domainOf(a.website), db = domainOf(b.website); return !da.empty() && da == db && da.find('.') != std::string::npos && nameKey(a.name) == nameKey(b.name);
}
inline Stage parseStage(const std::string& s) { const std::string t = lower(trim(s)); if (t.find("won") != std::string::npos || t.find("customer") != std::string::npos) return Stage::Won; if (t.find("lost") != std::string::npos || t.find("unqualified") != std::string::npos) return Stage::Lost; if (t.find("quote") != std::string::npos || t.find("proposal") != std::string::npos || t.find("negot") != std::string::npos) return Stage::Quoted; if (t.find("interest") != std::string::npos || t.find("sampl") != std::string::npos || t.find("qualified") != std::string::npos) return Stage::Interested; if (t.find("contact") != std::string::npos || t.find("attempt") != std::string::npos || t.find("working") != std::string::npos) return Stage::Contacted; return Stage::Prospect; }
inline std::vector<std::string> splitList(const std::string& s) { std::vector<std::string> o; std::string cur; for (char c : s) { if (c == ',' || c == ';' || c == '|') { if (!trim(cur).empty()) o.push_back(trim(cur)); cur.clear(); } else cur += c; } if (!trim(cur).empty()) o.push_back(trim(cur)); return o; }
inline bool parseNumber(const std::string& s, double& v) { const std::string t = trim(s); if (t.empty()) return false; char* e = nullptr; v = std::strtod(t.c_str(), &e); return e && *e == 0; }
inline std::string normDate(const std::string& s) { int a, b, c; const std::string t = trim(s); if (std::sscanf(t.c_str(), "%d-%d-%d", &a, &b, &c) == 3 && a > 1900) { char o[16]; std::snprintf(o, sizeof(o), "%04d-%02d-%02d", a, b, c); return o; } if (std::sscanf(t.c_str(), "%d/%d/%d", &a, &b, &c) == 3) { if (c < 100) c += 2000; char o[16]; std::snprintf(o, sizeof(o), "%04d-%02d-%02d", c, a, b); return o; } return ""; }   // US month/day/year

struct ImportResult { int added = 0, duplicates = 0, skipped = 0, noLocation = 0; std::vector<std::string> notes; };
inline ImportResult importRows(Database& db, const std::vector<Row>& rows, const ImportPlan& plan) {
    ImportResult r; for (size_t i = plan.hasHeader ? 1 : 0; i < rows.size(); ++i) {
        Account a; a.type = plan.defaultType; a.source = plan.defaultSource; a.country = plan.defaultCountry; a.added = today();
        for (size_t c = 0; c < rows[i].size() && c < plan.columns.size(); ++c) { const std::string v = trim(rows[i][c]); if (v.empty()) continue;
            switch (plan.columns[c]) {
                case Field::Name: a.name = v; break; case Field::Type: a.type = v; break; case Field::Address: a.address = v; break; case Field::City: a.city = v; break;
                case Field::State: a.state = stateCode(v).empty() ? v : stateCode(v); break; case Field::Zip: a.zip = v; break; case Field::Country: a.country = v; break; case Field::Phone: a.phone = v; break; case Field::Email: a.email = v; break;
                case Field::Website: a.website = v; break; case Field::Contact: a.contact = v; break; case Field::ContactTitle: a.contactTitle = v; break; case Field::Brands: a.brands = splitList(v); break;
                case Field::Source: a.source = v; break; case Field::Notes: a.notes = v; break; case Field::Lat: { double d; if (parseNumber(v, d) && std::fabs(d) <= 90) a.lat = d; break; } case Field::Lon: { double d; if (parseNumber(v, d) && std::fabs(d) <= 180) a.lon = d; break; }
                case Field::Stage: a.stage = parseStage(v); break; case Field::NextFollowUp: a.nextFollowUp = normDate(v); break; default: break; } }
        if (a.hasCoords() == false) { a.lat = NAN; a.lon = NAN; } if (std::isnan(a.lat) != std::isnan(a.lon)) { a.lat = NAN; a.lon = NAN; }
        if (a.name.empty()) { ++r.skipped; continue; }
        if (plan.skipDuplicates) { bool dup = false; for (auto& e : db.accounts) if (sameShop(e, a)) { dup = true; break; } if (dup) { ++r.duplicates; continue; } }
        if (!a.hasCoords() && stateCode(a.state).empty()) ++r.noLocation;
        db.add(a); ++r.added;
    }
    return r;
}

// ---------------------------------------------------------------- export
enum class ExportKind { Everything, HubSpot, NetSuite };
inline Row exportHeader(ExportKind k) {
    switch (k) {
        case ExportKind::HubSpot: return {"Company name", "Company domain name", "Phone Number", "Street Address", "City", "State/Region", "Postal Code", "Country/Region", "Industry", "Description", "First contact name", "First contact email", "Lifecycle Stage"};
        case ExportKind::NetSuite: return {"Company Name", "Category", "Phone", "Email", "Web Address", "Address 1", "City", "State", "Zip", "Country", "Sales Rep Notes", "Contact", "Contact Title"};
        default: return {"Name", "Type", "Stage", "Address", "City", "State", "ZIP", "Country", "Phone", "Email", "Website", "Contact", "Contact title", "Brands", "Source", "Latitude", "Longitude", "Added", "Next follow-up", "Closed", "First order", "Notes", "Last activity"};
    }
}
inline Row exportRow(const Account& a, ExportKind k) {
    const std::string last = a.log.empty() ? "" : a.log.back().date + ": " + a.log.back().note; std::string brands; for (size_t i = 0; i < a.brands.size(); ++i) brands += (i ? "; " : "") + a.brands[i];
    char la[32] = "", lo[32] = "", fo[32] = ""; if (a.hasCoords()) { std::snprintf(la, sizeof(la), "%.6f", a.lat); std::snprintf(lo, sizeof(lo), "%.6f", a.lon); } if (a.firstOrder > 0) std::snprintf(fo, sizeof(fo), "%.2f", a.firstOrder);
    const std::string note = (a.closeNote.empty() ? a.notes : a.notes + (a.notes.empty() ? "" : " | ") + a.closeNote) + (brands.empty() ? "" : (a.notes.empty() && a.closeNote.empty() ? "" : " | ") + std::string("Brands: ") + brands);
    switch (k) {
        case ExportKind::HubSpot: return {a.name, domainOf(a.website), a.phone, a.address, a.city, a.state, a.zip, a.country, a.type, note, a.contact, a.email, a.stage == Stage::Won ? "customer" : "lead"};
        case ExportKind::NetSuite: return {a.name, a.type, a.phone, a.email, a.website, a.address, a.city, a.state, a.zip, a.country, note, a.contact, a.contactTitle};
        default: return {a.name, a.type, stageName(a.stage), a.address, a.city, a.state, a.zip, a.country, a.phone, a.email, a.website, a.contact, a.contactTitle, brands, a.source, la, lo, a.added, a.nextFollowUp, a.closedDate, fo, a.notes, last};
    }
}
template <class Pred> inline std::string exportCsv(const Database& db, ExportKind k, Pred keep) { std::vector<Row> rows{exportHeader(k)}; for (auto& a : db.accounts) if (keep(a)) rows.push_back(exportRow(a, k)); return writeCsv(rows); }

// ---------------------------------------------------------------- territory numbers (what the tile map shows)
struct Tally { int active = 0, won = 0, lost = 0, due = 0; double wonValue = 0; int total() const { return active + won + lost; } };
inline std::map<std::string, Tally> tallyByState(const Database& db, const std::string& todayDate) {
    std::map<std::string, Tally> m; for (auto& a : db.accounts) { std::string s = stateCode(a.state); if (s.empty()) s = "?"; Tally& t = m[s]; if (a.stage == Stage::Won) { ++t.won; t.wonValue += a.firstOrder; } else if (a.stage == Stage::Lost) ++t.lost; else { ++t.active; if (a.dueBy(todayDate)) ++t.due; } } return m;
}
inline std::map<std::string, Tally> tallyByRegion(const Database& db, const std::string& todayDate) {
    std::map<std::string, Tally> m; for (auto& [s, t] : tallyByState(db, todayDate)) { Tally& r = m[s == "?" ? "Unplaced" : regionOf(s)]; r.active += t.active; r.won += t.won; r.lost += t.lost; r.due += t.due; r.wonValue += t.wonValue; } return m;
}

}  // namespace ctc
