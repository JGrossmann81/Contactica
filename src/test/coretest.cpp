#include <cstdio>
#include <filesystem>
#include "../core/csv.h"
using namespace ctc;
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main() {
    // ---- states
    CHECK(stateCode("California") == "CA" && stateCode(" ca ") == "CA" && stateCode("Calif.") == "CA" && stateCode("Washington, D.C.") == "DC" && stateCode("Ontario") == "ON" && stateCode("Narnia").empty(), "state names normalise");
    CHECK(regionOf("TX") == "South" && regionOf("NY") == "Northeast" && regionOf("ON") == "Canada" && regionOf("ZZ") == "Other", "regions");
    { std::set<std::pair<int, int>> seen; bool dup = false; for (auto& p : places()) if (!seen.insert({p.gx, p.gy}).second) dup = true; CHECK(!dup, "no two states share a map tile"); CHECK(places().size() == 51 + 10, "51 US places plus 10 provinces (%zu)", places().size()); }
    // ---- CSV reading
    { auto r = parseCsv("\xEF\xBB\xBFName,Notes,City\r\n\"Smith, Music\",\"line1\nline2 \"\"q\"\"\",Austin\r\n\r\nB,,\r\n"); CHECK(r.size() == 3, "3 rows incl. header (%zu)", r.size());
      if (r.size() == 3) { CHECK(r[0][0] == "Name", "BOM removed (%s)", r[0][0].c_str()); CHECK(r[1][0] == "Smith, Music" && r[1][1] == "line1\nline2 \"q\"" && r[1][2] == "Austin", "quotes, comma and newline inside a cell"); CHECK(r[2].size() == 3, "trailing empty cells kept (%zu)", r[2].size()); } }
    { auto r = parseCsv("a;b;c\n1;2;3"); CHECK(r.size() == 2 && r[1][2] == "3", "semicolon delimiter detected"); auto t = parseCsv("a\tb\n1\t2"); CHECK(t.size() == 2 && t[1][1] == "2", "tab delimiter detected"); }
    { std::vector<Row> rows{{"x", "a,\"b\""}, {"é", "multi\nline"}}; auto back = parseCsv(writeCsv(rows)); CHECK(back == rows, "write then read gives the same table"); }
    // ---- column guessing from real-world headers
    CHECK(guessField("Company name") == Field::Name && guessField("Company domain name") == Field::Website && guessField("State/Region") == Field::State && guessField("Postal Code") == Field::Zip && guessField("Phone Number") == Field::Phone && guessField("Billing City") == Field::City && guessField("Latitude") == Field::Lat && guessField("Lng") == Field::Lon && guessField("Gibberish") == Field::None, "headers from HubSpot / NetSuite style files are recognised");
    // ---- import
    Database db; const std::string csv = "Company Name,Address,City,State,Zip,Phone,Website,Contact,Brands,Latitude,Longitude,Status,Next Follow-up\n"
        "Sam's Music,1 Main St,Austin,Texas,78701,(512) 555-0100,https://www.sams.com/,Sam,\"Alpha; Beta\",30.27,-97.74,Contacted,10/25/2026\n"
        "Beat Shop,2 Elm,Portland,OR,97201,503-555-0111,,,,,,,\n"
        ",nameless,,,,,,,,,,,\n"
        "SAMS MUSIC INC,other address,Austin,TX,78701,,,,,,,,\n"
        "Sam's Second,9 Side,Dallas,TX,75201,512 555 0100,,,,,,,\n";
    { auto rows = parseCsv(csv); ImportPlan plan = planFor(rows); CHECK(plan.hasHeader, "header detected"); ImportResult res = importRows(db, rows, plan);
      CHECK(res.added == 2 && res.duplicates == 2 && res.skipped == 1, "import: 2 added, 2 duplicates (same shop by name+zip, same phone), 1 nameless skipped (%d/%d/%d)", res.added, res.duplicates, res.skipped);
      const Account* a = db.find(1); CHECK(a && a->state == "TX" && a->stage == Stage::Contacted && a->nextFollowUp == "2026-10-25" && a->brands.size() == 2 && a->hasCoords() && std::fabs(a->lon + 97.74) < 1e-9, "fields land in the right places");
      const Account* b = db.find(2); CHECK(b && !b->hasCoords() && b->state == "OR", "no coordinates when none given"); }
    { auto rows = parseCsv(csv); ImportPlan plan = planFor(rows); plan.skipDuplicates = false; ImportResult res = importRows(db, rows, plan); CHECK(res.added == 4 && db.accounts.size() == 6, "importing without duplicate check adds everything"); db.remove(3); db.remove(4); db.remove(5); db.remove(6); }
    { Database d2; auto rows = parseCsv("Anything\nfoo\n"); ImportPlan p = planFor(rows); CHECK(!p.hasHeader || p.columns[0] == Field::None, "unknown headers do not map"); p.hasHeader = true; p.columns = {Field::Name}; importRows(d2, rows, p); CHECK(d2.accounts.size() == 1 && d2.accounts[0].name == "foo", "manual mapping works"); }
    // ---- manual entry, follow-ups, winning
    { Account m; m.name = "Riff Raff Guitars"; m.city = "Denver"; m.state = "colorado"; m.nextFollowUp = "2026-10-20"; const int id = db.add(m); CHECK(db.find(id)->state == "CO", "state normalised on entry"); CHECK(db.find(id)->dueBy("2026-10-21") && !db.find(id)->dueBy("2026-10-19"), "follow-up due logic");
      CHECK(addDays("2026-10-30", 3) == "2026-11-02" && daysBetween("2026-10-21", "2026-10-31") == 10, "date helpers");
      auto before = tallyByState(db, "2026-10-21"); CHECK(before["CO"].active == 1 && before["CO"].due == 1 && before["TX"].active == 1, "tally counts active prospects");
      db.close(id, Stage::Won, "First order: 12 units", 1450.0, "2026-10-21"); const Account* w = db.find(id); CHECK(w->stage == Stage::Won && !w->active() && w->nextFollowUp.empty() && w->closedDate == "2026-10-21" && w->log.back().note.rfind("WON", 0) == 0, "winning closes the account");
      auto after = tallyByState(db, "2026-10-21"); CHECK(after["CO"].active == 0 && after["CO"].won == 1 && after["CO"].wonValue == 1450.0, "won accounts leave the active count but stay in the territory total"); CHECK(db.activeCount() == 2, "active list shrinks (%zu)", db.activeCount());
      auto reg = tallyByRegion(db, "2026-10-21"); CHECK(reg["West"].won == 1 && reg["West"].active == 1 && reg["South"].active == 1, "region totals");
      db.reopen(id); CHECK(db.find(id)->active(), "a won account can be reopened by mistake-fix"); db.close(id, Stage::Won, "again", 10, "2026-10-22"); }
    // ---- export
    { std::string all = exportCsv(db, ExportKind::Everything, [](const Account&) { return true; }); auto rows = parseCsv(all); CHECK(rows.size() == db.accounts.size() + 1, "everything export has a row per account"); CHECK(rows[0].size() == rows[1].size(), "header and rows align");
      std::string won = exportCsv(db, ExportKind::NetSuite, [](const Account& a) { return a.stage == Stage::Won; }); auto wr = parseCsv(won); CHECK(wr.size() == 2 && wr[0][0] == "Company Name" && wr[1][0] == "Riff Raff Guitars" && wr[1][7] == "CO", "NetSuite-style won export");
      std::string hs = exportCsv(db, ExportKind::HubSpot, [](const Account& a) { return a.stage == Stage::Won; }); auto hr = parseCsv(hs); CHECK(hr.size() == 2 && hr[0][0] == "Company name" && hr[1][12] == "customer", "HubSpot-style won export");
      // an exported file imports back with the columns recognised
      Database d3; auto back = parseCsv(all); ImportPlan p = planFor(back); ImportResult r = importRows(d3, back, p); CHECK(r.added == (int)db.accounts.size(), "our own export re-imports completely (%d of %zu)", r.added, db.accounts.size()); }
    // ---- save / load
    { const std::string path = (std::filesystem::temp_directory_path() / "ctc_test.json").string(); std::filesystem::remove(path); std::filesystem::remove(path + ".bak"); db.myBrands = {"Alpha", "Beta"}; db.find(1)->log.push_back({"2026-10-21", "Called, left voicemail"});
      CHECK(saveDatabase(db, path), "save"); db.find(1)->notes = "changed"; CHECK(saveDatabase(db, path) && std::filesystem::exists(path + ".bak"), "second save keeps a .bak");
      Database back; CHECK(loadDatabase(back, path), "load"); CHECK(back.accounts.size() == db.accounts.size() && back.myBrands == db.myBrands && back.find(1)->notes == "changed" && back.find(1)->log.size() == 1 && back.find(1)->hasCoords() && !back.find(2)->hasCoords() && back.nextId == db.nextId, "everything survives a save and load");
      std::ofstream(path, std::ios::binary) << "{ not json"; Database bad; CHECK(!loadDatabase(bad, path), "a damaged file is refused, not half-loaded"); }
    // ---- map helpers
    { double la, lo; Account a; a.id = 5; a.state = "TX"; a.where(la, lo); CHECK(la > 28 && la < 34 && lo > -102 && lo < -96, "pin without coordinates is drawn near its state's centre (%.1f, %.1f)", la, lo); Account b; b.id = 6; b.lat = 10; b.lon = 20; b.where(la, lo); CHECK(la == 10 && lo == 20, "real coordinates win");
      double x, y; project(0, 0, x, y); CHECK(std::fabs(x - 0.5) < 1e-9 && std::fabs(y - 0.5) < 1e-9, "projection origin"); }
    std::printf(failures ? "CORE TEST: %d failure(s)\n" : "CORE TEST: all good\n", failures); return failures ? 1 : 0;
}
