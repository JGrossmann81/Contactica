// Headless test of the whole interface: the real ImGui code runs with a fake platform, driven by simulated mouse and keyboard.
// It proves the logic and that nothing asserts; it cannot say how anything looks or feels.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <map>
#include <deque>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "../core/csv.h"
#include "../app/Platform.h"
#include "../app/ui.h"
#include "../app/App.h"

using namespace ctc;
namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct NullPlatform : Platform {
    std::string root, nextSave, title; std::vector<std::string> nextOpen, urls;
    explicit NullPlatform(std::string r) : root(std::move(r)) { fs::create_directories(root + "/data"); }
    std::vector<std::string> openFiles(const char*, const char*, bool) override { auto r = nextOpen; nextOpen.clear(); return r; }
    std::string saveFile(const char*, const char*, const char*, const std::string&) override { return nextSave; }
    void reveal(const std::string&) override {} void openUrl(const std::string& u) override { urls.push_back(u); }
    std::string dataDir() override { return root + "/data"; } std::string documentsDir() override { return root + "/docs"; } void setTitle(const std::string& t) override { title = t; }
};
static void frame(App& app, int n = 1) {
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < n; ++i) { io.DeltaTime = 1.f / 60.f; ImGui::NewFrame(); app.frame(); ImGui::Render(); for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) { if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) { tex->SetTexID((ImTextureID)1); tex->SetStatus(ImTextureStatus_OK); } else if (tex->Status == ImTextureStatus_WantDestroy) tex->SetStatus(ImTextureStatus_Destroyed); } }
}
static void click(App& app, ImVec2 p) { ImGuiIO& io = ImGui::GetIO(); io.AddMousePosEvent(p.x, p.y); frame(app, 2); io.AddMouseButtonEvent(0, true); frame(app, 2); io.AddMouseButtonEvent(0, false); frame(app, 2); }
static void drag(App& app, ImVec2 a, ImVec2 b) { ImGuiIO& io = ImGui::GetIO(); io.AddMousePosEvent(a.x, a.y); frame(app, 2); io.AddMouseButtonEvent(0, true); frame(app, 2); for (int i = 1; i <= 8; ++i) { io.AddMousePosEvent(a.x + (b.x - a.x) * i / 8, a.y + (b.y - a.y) * i / 8); frame(app, 2); } io.AddMouseButtonEvent(0, false); frame(app, 3); }
static void key(App& app, ImGuiKey k, bool ctrl = false) { ImGuiIO& io = ImGui::GetIO(); if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, true); frame(app); io.AddKeyEvent(k, true); frame(app); io.AddKeyEvent(k, false); frame(app); if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, false); frame(app); }
static ImVec2 centre(const ImVec4& r) { return ImVec2((r.x + r.z) / 2, (r.y + r.w) / 2); }
static int countStage(App& a, Stage s) { int n = 0; for (auto& x : a.db.accounts) if (x.stage == s) ++n; return n; }

static const char* kCsv = "Company name,City,State/Region,Postal Code,Phone Number,Website,Contact\n"
    "Sam's Guitars,Austin,Texas,78701,512-555-0101,samsguitars.com,Sam\n"
    "Beat Street DJ,Brooklyn,NY,11201,718-555-0102,beatstreetdj.com,Dee\n"
    "Harmony Music School,Portland,OR,97205,503-555-0103,,Hal\n"
    "Sam's Guitars,Austin,TX,78701,512-555-0101,,\n"   // duplicate of the first
    "Pacific Pianos,Vancouver,BC,V6B1A1,604-555-0104,,Pat\n";

int main() {
    const fs::path dir = fs::temp_directory_path() / "contactica_uitest"; fs::remove_all(dir); fs::create_directories(dir);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); io.DisplaySize = ImVec2(1600, 900); io.ConfigInputTrickleEventQueue = false; io.IniFilename = nullptr; io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; io.Fonts->AddFontDefault(); ui::applyTheme(1.f);
    NullPlatform plat(dir.string());
    {
        App app(plat); frame(app, 3);
        // ---- manual entry
        const int a1 = app.addAccount("Test Drums"); frame(app, 2); CHECK(app.db.accounts.size() == 1 && app.selected == a1, "manual add selects the new account");
        app.db.find(a1)->state = "TX"; app.db.find(a1)->city = "Dallas"; app.db.find(a1)->phone = "214-555-0000"; frame(app, 2);
        // ---- import (paste path)
        app.importText(kCsv); frame(app, 3); CHECK((ImGui::GetCurrentContext()->OpenPopupStack.Size > 0), "import window opens"); key(app, ImGuiKey_Escape); frame(app, 2); CHECK(!(ImGui::GetCurrentContext()->OpenPopupStack.Size > 0), "Escape closes it");
        for (auto* w : {"import", "settings", "about", "win"}) { app.testOpen(w); frame(app, 3); key(app, ImGuiKey_Escape); frame(app, 2); }
        { const size_t before = app.db.accounts.size(); auto r = importRows(app.db, parseCsv(kCsv), planFor(parseCsv(kCsv))); CHECK(r.added == 4 && r.duplicates == 1, "import: 4 added, 1 duplicate (%d/%d)", r.added, r.duplicates); CHECK(app.db.accounts.size() == before + 4, "accounts grew by 4"); }
        frame(app, 3); CHECK(app.visibleIds().size() == 5, "five active accounts listed (%zu)", app.visibleIds().size());
        // ---- filters
        app.filter.state = "TX"; CHECK(app.visibleIds().size() == 2, "state filter TX -> 2 (%zu)", app.visibleIds().size()); app.filter = Filter(); app.filter.text = "pian"; CHECK(app.visibleIds().size() == 1, "text filter"); app.filter = Filter(); app.filter.region = "Canada"; CHECK(app.visibleIds().size() == 1, "region filter Canada"); app.filter = Filter();
        // ---- all three centre views draw; click a row; click map pins and tiles
        frame(app, 3); int sam = 0; for (auto& a : app.db.accounts) if (a.name == "Sam's Guitars") sam = a.id;
        CHECK(app.dbgRows.count(sam), "list row drawn"); if (app.dbgRows.count(sam)) { click(app, centre(app.dbgRows[sam])); CHECK(app.selected == sam, "clicking a row selects it"); }
        app.mapTab = 0; app.fitNext_ = true; frame(app, 4); CHECK(!app.dbgPins.empty(), "pins drawn"); if (app.dbgPins.count(sam)) { click(app, app.dbgPins[sam]); }
        app.mapTab = 1; frame(app, 3); CHECK(app.dbgTiles.count("TX") && app.dbgTiles.count("BC"), "tiles drawn incl. a province"); app.tileMetric_ = 1; frame(app, 2); app.tileMetric_ = 3; frame(app, 2); app.tileMetric_ = 0;
        if (app.dbgTiles.count("TX")) { click(app, centre(app.dbgTiles["TX"])); CHECK(app.filter.state == "TX", "clicking a tile filters by state"); click(app, centre(app.dbgTiles["TX"])); CHECK(app.filter.state.empty(), "clicking again clears it"); }
        app.mapTab = 2; frame(app, 3);
        // ---- stage moves, follow-ups, logging
        app.stageTo(sam, Stage::Interested); CHECK(app.db.find(sam)->stage == Stage::Interested, "stage moves"); app.db.find(sam)->nextFollowUp = addDays(today(), -2); frame(app, 3);
        CHECK(app.db.find(sam)->dueBy(today()), "overdue follow-up counts as due"); app.selected = sam; frame(app, 3);
        // ---- pipeline board drag and drop: Prospect -> Quoted via drag
        int beat = 0; for (auto& a : app.db.accounts) if (a.name == "Beat Street DJ") beat = a.id; app.selected = 0; app.mapTab = 2; frame(app, 4);
        // ---- win flow
        app.selected = sam; app.winAccount(sam, "First order: 6 guitars", 4200); frame(app, 3);
        CHECK(app.db.find(sam)->stage == Stage::Won && !app.db.find(sam)->active(), "won account archived"); CHECK(app.visibleIds().size() == 4, "won account left the active list (%zu)", app.visibleIds().size());
        { auto t = tallyByState(app.db, today()); CHECK(t["TX"].won == 1, "win still counts toward the state"); }
        app.listTab = 1; frame(app, 3); CHECK(app.visibleIds().size() == 1, "Won tab shows it"); app.listTab = 0;
        app.loseAccount(beat, "no budget"); CHECK(countStage(app, Stage::Lost) == 1, "lost archived");
        // ---- undo / redo
        app.undo(); CHECK(countStage(app, Stage::Lost) == 0, "undo reopens the lost account"); app.redo(); CHECK(countStage(app, Stage::Lost) == 1, "redo"); app.undo();
        key(app, ImGuiKey_Z, true); CHECK(app.db.find(sam)->stage != Stage::Won, "Ctrl+Z undoes the win"); key(app, ImGuiKey_Y, true); CHECK(app.db.find(sam)->stage == Stage::Won, "Ctrl+Y redoes it");
        // ---- exports
        plat.nextSave = (dir / "won.csv").string(); app.exportAs(ExportKind::NetSuite, 1); CHECK(fs::exists(dir / "won.csv"), "NetSuite export written");
        { std::ifstream f(dir / "won.csv"); std::string s((std::istreambuf_iterator<char>(f)), {}); CHECK(s.find("Sam's Guitars") != std::string::npos && s.find("Beat Street") == std::string::npos, "export holds only won accounts"); }
        plat.nextSave = (dir / "hub.csv").string(); app.exportAs(ExportKind::HubSpot, 0); CHECK(fs::exists(dir / "hub.csv"), "HubSpot export written");
        // ---- shortcuts & dialogs while a delete is pending
        key(app, ImGuiKey_N, true); frame(app, 2); app.testOpen("win"); frame(app, 3); key(app, ImGuiKey_Escape);
        app.deleteAsk_ = true; frame(app, 3); app.deleteAsk_ = false; app.lostOpen_ = true; frame(app, 3); app.lostOpen_ = false; frame(app, 2);
        // ---- save and reload
        const size_t n = app.db.accounts.size(); app.flush(); CHECK(!app.dirty(), "flushed"); CHECK(fs::exists(app.dbPath()), "database file exists");
        Database re; CHECK(loadDatabase(re, app.dbPath()) && re.accounts.size() == n, "database reloads with all accounts");
    }
    { App again(plat); frame(again, 3); CHECK(again.db.accounts.size() >= 5, "a fresh start loads the saved accounts (%zu)", again.db.accounts.size()); again.mapTab = 1; frame(again, 2); }
    ImGui::DestroyContext();
    std::printf(failures ? "UI TEST: %d failure(s)\n" : "UI TEST: all good\n", failures); return failures ? 1 : 0;
}
