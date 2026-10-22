// Contactica: start-up, saving, undo, filters, menus, dialogs.
#include "App.h"
#include <algorithm>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace ctc {

App::App(Platform& p) : plat(p) {
    std::error_code ec; std::filesystem::create_directories(plat.dataDir(), ec);
    if (!loadDatabase(db, dbPath())) { Database b; if (std::filesystem::exists(dbPath() + ".bak", ec) && loadDatabase(b, dbPath() + ".bak")) { db = b; setStatus("The main file could not be read, so the last backup was loaded."); } }
    if (db.myBrands.empty()) db.myBrands = {"My brand 1", "My brand 2"};
    lastSave_ = 0;
}
App::~App() { flush(); }
void App::flush() { if (dirty_) { if (saveDatabase(db, dbPath())) dirty_ = false; else setStatus("WARNING: could not save to " + dbPath()); } }
bool App::wantsClose() { flush(); return !dirty_; }
void App::snapshot() { undo_.push_back(db); if (undo_.size() > 60) undo_.pop_front(); redo_.clear(); }
void App::undo() { if (undo_.empty()) return; redo_.push_back(db); db = undo_.back(); undo_.pop_back(); touch(); if (!db.find(selected)) selected = 0; setStatus("Undone."); }
void App::redo() { if (redo_.empty()) return; undo_.push_back(db); db = redo_.back(); redo_.pop_back(); touch(); if (!db.find(selected)) selected = 0; setStatus("Redone."); }

int App::addAccount(const std::string& name) { snapshot(); Account a; a.name = name; a.state = filter.state; const int id = db.add(a); selected = id; touch(); return id; }
void App::stageTo(int id, Stage s) { Account* a = db.find(id); if (!a || a->stage == s) return; if (s == Stage::Won || s == Stage::Lost) return; snapshot(); const bool was = a->active(); a->stage = s; a->closedDate.clear(); a->log.push_back({today(), std::string("Moved to ") + stageName(s) + (was ? "." : " (reopened).")}); touch(); }
void App::winAccount(int id, const std::string& note, double value) { snapshot(); db.close(id, Stage::Won, note, value); touch(); setStatus("Won! It is now in the archive (Won tab) and counts toward your territory."); if (selected == id) selected = 0; }
void App::loseAccount(int id, const std::string& note) { snapshot(); db.close(id, Stage::Lost, note); touch(); setStatus("Closed as lost. Find it in the Lost tab if you want to reopen it."); if (selected == id) selected = 0; }

bool App::matches(const Account& a) const {
    const Filter& f = filter;
    if (!f.state.empty() && stateCode(a.state) != f.state) return false; if (!f.region.empty() && a.region() != f.region) return false; if (f.stage >= 0 && (int)a.stage != f.stage) return false;
    if (!f.type.empty() && a.type != f.type) return false; if (!f.brand.empty() && std::find(a.brands.begin(), a.brands.end(), f.brand) == a.brands.end()) return false; if (f.dueOnly && !a.dueBy(today())) return false;
    if (!f.text.empty()) { const std::string q = lower(f.text); for (const std::string* s : {&a.name, &a.city, &a.contact, &a.state, &a.phone, &a.email, &a.notes, &a.zip, &a.website}) if (lower(*s).find(q) != std::string::npos) return true; return false; }
    return true;
}
std::vector<int> App::visibleIds() {
    std::vector<int> ids; for (auto& a : db.accounts) { const bool archived = !a.active(); const bool want = listTab == 0 ? !archived : listTab == 1 ? a.stage == Stage::Won : a.stage == Stage::Lost; if (want && matches(a)) ids.push_back(a.id); }
    const std::string t = today();
    auto key = [&](const Account& a, int col) -> std::string { switch (col) { case 0: return lower(a.name); case 1: return lower(a.city); case 2: return a.state; case 3: return a.type; case 4: return std::to_string((int)a.stage); case 5: return listTab != 0 ? a.closedDate : (a.nextFollowUp.empty() ? "9999" : a.nextFollowUp); case 6: return a.closedDate; default: return ""; } };
    std::stable_sort(ids.begin(), ids.end(), [&](int x, int y) { const std::string a = key(*db.find(x), sortCol_), b = key(*db.find(y), sortCol_); return sortAsc_ ? a < b : a > b; }); return ids;
}

void App::applyDocumentTitle() { plat.setTitle(std::string("Contactica") + (dirty_ ? " *" : "")); }

void App::importText(const std::string& csv) { importRows_ = parseCsv(csv); plan_ = planFor(importRows_); importName_ = "pasted text"; importOpen_ = true; }
void App::importFiles(const std::vector<std::string>& files) {
    for (auto& f : files) { std::ifstream in(f, std::ios::binary); if (!in) { setStatus("Could not open " + f); continue; } std::stringstream ss; ss << in.rdbuf(); importRows_ = parseCsv(ss.str()); plan_ = planFor(importRows_); const auto sl = f.find_last_of("/\\"); importName_ = sl == std::string::npos ? f : f.substr(sl + 1); plan_.defaultSource = importName_; importOpen_ = true; break; }
}

void App::exportAs(ExportKind k, int which) {
    const char* nm[] = {"active accounts", "won accounts", "lost accounts", "all accounts"}; const std::string suffix = k == ExportKind::HubSpot ? " (HubSpot)" : k == ExportKind::NetSuite ? " (NetSuite)" : "";
    const std::string path = plat.saveFile("Export", "CSV files|*.csv", "csv", std::string("Contactica ") + nm[which] + suffix + " " + today() + ".csv"); if (path.empty()) return;
    auto keep = [which](const Account& a) { return which == 0 ? a.active() : which == 1 ? a.stage == Stage::Won : which == 2 ? a.stage == Stage::Lost : true; };
    const std::string text = exportCsv(db, k, keep); int n = 0; for (auto& a : db.accounts) if (keep(a)) ++n; std::ofstream out(path, std::ios::binary); if (!out) { setStatus("Could not write " + path); return; } out << text; setStatus("Exported " + std::to_string(n) + " " + nm[which] + " to " + path);
}

// ---------------------------------------------------------------- one frame
void App::frame() {
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { if (io.KeyShift) redo(); else undo(); }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) { addAccount("New shop"); }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) focusSearch_ = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I, false)) { auto f = plat.openFiles("Import accounts", "CSV files|*.csv;*.txt;*.tsv|All files|*.*", false); if (!f.empty()) importFiles(f); }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) { selected = 0; }
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) focusSearch_ = true;
    menuBar();
    const ImGuiViewport* vp = ImGui::GetMainViewport(); const float sh = ImGui::GetFrameHeight() + 8.f, gap = 4.f; const ImVec2 wp = vp->WorkPos, ws = vp->WorkSize; const float areaH = ws.y - sh;
    const float lw = std::clamp(ws.x * 0.34f, 380.f, 640.f), dw = std::clamp(ws.x * 0.25f, 330.f, 460.f);
    const ImGuiWindowFlags fl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowPos(wp); ImGui::SetNextWindowSize(ImVec2(lw, areaH)); if (ImGui::Begin("##left", nullptr, fl)) leftPanel(); ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(wp.x + lw + gap, wp.y)); ImGui::SetNextWindowSize(ImVec2(ws.x - lw - dw - 2 * gap, areaH)); if (ImGui::Begin("##centre", nullptr, fl)) centrePanel(); ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(wp.x + ws.x - dw, wp.y)); ImGui::SetNextWindowSize(ImVec2(dw, areaH)); if (ImGui::Begin("##detail", nullptr, fl)) detailPanel(); ImGui::End();
    statusBar(); modals();
    if (dirty_ && ImGui::GetTime() - dirtySince_ > 1.5) flush();           // saves a moment after you stop typing
    applyDocumentTitle();
}

void App::menuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New account", "Ctrl+N")) addAccount("New shop");
        if (ImGui::MenuItem("Import CSV...", "Ctrl+I")) { auto f = plat.openFiles("Import accounts", "CSV files|*.csv;*.txt;*.tsv|All files|*.*", false); if (!f.empty()) importFiles(f); }
        if (ImGui::MenuItem("Import from clipboard (copied from Excel)")) { const char* c = ImGui::GetClipboardText(); if (c && *c) importText(c); else setStatus("The clipboard is empty."); }
        ImGui::Separator();
        if (ImGui::BeginMenu("Export")) { if (ImGui::MenuItem("Active accounts (CSV)")) exportAs(ExportKind::Everything, 0); if (ImGui::MenuItem("Everything (CSV)")) exportAs(ExportKind::Everything, 3); ImGui::Separator();
            if (ImGui::MenuItem("Won accounts for NetSuite")) exportAs(ExportKind::NetSuite, 1); if (ImGui::MenuItem("Won accounts for HubSpot")) exportAs(ExportKind::HubSpot, 1); if (ImGui::MenuItem("Won accounts (CSV, all columns)")) exportAs(ExportKind::Everything, 1); if (ImGui::MenuItem("Lost accounts (CSV)")) exportAs(ExportKind::Everything, 2); ImGui::EndMenu(); }
        ImGui::Separator(); if (ImGui::MenuItem("Settings...")) settingsOpen_ = true; if (ImGui::MenuItem("Show my data folder")) plat.reveal(plat.dataDir()); if (ImGui::MenuItem("Quit")) quitRequested = true; ImGui::EndMenu(); }
    if (ImGui::BeginMenu("Edit")) { if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undo_.empty())) undo(); if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !redo_.empty())) redo(); ImGui::Separator(); if (ImGui::MenuItem("Find", "Ctrl+F")) focusSearch_ = true; if (ImGui::MenuItem("Clear all filters", nullptr, false, filter.any())) filter = Filter(); ImGui::EndMenu(); }
    if (ImGui::BeginMenu("View")) { ImGui::MenuItem("Show won accounts on the pin map", nullptr, &showWon_); if (ImGui::MenuItem("Zoom the pin map to fit my accounts")) { fitNext_ = true; mapTab = 0; } ImGui::EndMenu(); }
    if (ImGui::BeginMenu("Help")) { if (ImGui::MenuItem("About Contactica")) aboutOpen_ = true; ImGui::EndMenu(); }
    ImGui::EndMainMenuBar();
}

void App::statusBar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport(); const float h = ImGui::GetFrameHeight() + 8.f; ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - h)); ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
    if (ImGui::Begin("##status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        if (ImGui::GetTime() < statusUntil_) ImGui::TextUnformatted(status_.c_str()); else { int won = 0; double val = 0; for (auto& a : db.accounts) if (a.stage == Stage::Won) { ++won; val += a.firstOrder; } ImGui::TextDisabled("%zu active  |  %d won (first orders $%.0f)  |  %s", db.activeCount(), won, val, dirty_ ? "saving..." : "saved"); }
    }
    ImGui::End(); ImGui::PopStyleVar();
}

// ---------------------------------------------------------------- dialogs
void App::modals() {
    auto once = [](bool& f, const char* n) { if (f) { ImGui::OpenPopup(n); f = false; } };
    once(importOpen_, "Import accounts"); once(settingsOpen_, "Settings"); once(aboutOpen_, "About Contactica"); once(winOpen_, "Mark as won"); once(lostOpen_, "Close as lost"); once(deleteAsk_, "Delete account?"); once(resultOpen_, "Import finished");
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) ImGui::ClosePopupToLevel(0, true);   // Esc closes any dialog
    const ImGuiViewport* vp = ImGui::GetMainViewport(); const ImVec2 c = vp->GetCenter();
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); ImGui::SetNextWindowSize(ImVec2(std::min(900.f, vp->WorkSize.x * 0.8f), std::min(640.f, vp->WorkSize.y * 0.85f)), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Import accounts", nullptr)) { importModal(); ImGui::EndPopup(); }
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); if (ImGui::BeginPopupModal("Import finished", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Added %d accounts.", lastResult_.added); if (lastResult_.duplicates) ImGui::Text("Skipped %d that you already have (duplicates).", lastResult_.duplicates); if (lastResult_.skipped) ImGui::Text("Skipped %d rows with no shop name.", lastResult_.skipped);
        if (lastResult_.noLocation) ImGui::TextWrapped("%d have no coordinates and no recognisable state, so they will not appear on the maps until you add one.", lastResult_.noLocation); ImGui::TextDisabled("Changed your mind? Edit > Undo (Ctrl+Z) removes the whole import."); if (ImGui::Button("OK")) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing); if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { settingsModal(); ImGui::EndPopup(); }
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); if (ImGui::BeginPopupModal("About Contactica", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { aboutModal(); ImGui::EndPopup(); }
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Appearing); if (ImGui::BeginPopupModal("Mark as won", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { winModal(); ImGui::EndPopup(); }
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Close as lost", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { const Account* a = db.find(selected); ImGui::TextWrapped("%s", a ? ("Close " + a->name + " as lost?").c_str() : "Nothing selected."); ImGui::InputTextWithHint("##why", "Why? (optional, e.g. 'carries a competitor')", &lostNote_);
        if (ImGui::Button("Close as lost") && a) { loseAccount(selected, lostNote_); lostNote_.clear(); ImGui::CloseCurrentPopup(); } ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
    ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); if (ImGui::BeginPopupModal("Delete account?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { const Account* a = db.find(selected); ImGui::TextWrapped("%s", a ? ("Delete " + a->name + " completely? (Undo can bring it back.)").c_str() : "Nothing selected.");
        if (ImGui::Button("Delete") && a) { snapshot(); db.remove(selected); selected = 0; touch(); ImGui::CloseCurrentPopup(); } ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
}

void App::winModal() {
    Account* a = db.find(selected); if (!a) { ImGui::TextUnformatted("Nothing selected."); if (ImGui::Button("Close")) ImGui::CloseCurrentPopup(); return; }
    ImGui::TextWrapped("%s bought from you. Nice.", a->name.c_str()); ImGui::TextDisabled("It leaves the active list, moves to the Won archive and counts toward your territory. Export it for NetSuite or HubSpot from the File menu.");
    double v = winValue_; ImGui::SetNextItemWidth(160); if (ImGui::InputDouble("First order ($)", &v, 0, 0, "%.2f")) winValue_ = std::max(0.0, v); ImGui::InputTextWithHint("##wn", "What did they buy? (optional)", &winNote_);
    if (ImGui::Button("Mark as won")) { winAccount(selected, winNote_, winValue_); winNote_.clear(); winValue_ = 0; ImGui::CloseCurrentPopup(); } ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
}

void App::settingsModal() {
    ImGui::SeparatorText("My brands"); ImGui::TextWrapped("The lines you are trying to place. They appear as tick boxes on each account and as a filter.");
    int rm = -1; for (size_t i = 0; i < db.myBrands.size(); ++i) { ImGui::PushID((int)i); ImGui::SetNextItemWidth(260); if (ed(ImGui::InputText("##b", &db.myBrands[i]))) {} ImGui::SameLine(); if (ImGui::SmallButton("Remove")) rm = (int)i; ImGui::PopID(); }
    if (rm >= 0) { snapshot(); db.myBrands.erase(db.myBrands.begin() + rm); touch(); }
    ImGui::SetNextItemWidth(260); ImGui::InputTextWithHint("##nb", "Add a brand", &newBrand_); ImGui::SameLine(); if (ImGui::Button("Add") && !trim(newBrand_).empty()) { snapshot(); db.myBrands.push_back(trim(newBrand_)); newBrand_.clear(); touch(); }
    ImGui::SeparatorText("Your data"); ImGui::TextWrapped("Everything is kept in one file on this computer and saved automatically (with a .bak copy of the previous save):"); ImGui::TextDisabled("%s", dbPath().c_str());
    if (ImGui::Button("Show in folder")) plat.reveal(dbPath()); ImGui::SameLine(); if (ImGui::Button("Save a backup copy...")) { flush(); const std::string p = plat.saveFile("Save a backup", "Contactica backup|*.json", "json", "Contactica backup " + today() + ".json"); if (!p.empty()) { saveDatabase(db, p); setStatus("Backup saved to " + p); } }
    ImGui::Spacing(); if (ImGui::Button("Done")) ImGui::CloseCurrentPopup();
}
void App::aboutModal() {
    ImGui::TextUnformatted("Contactica"); ImGui::TextDisabled("Find and win new music retailers"); ImGui::Separator(); ImGui::TextWrapped("A small, focused tool for hunting new accounts: shops, DJ stores and schools by region, from first contact to first order. Won accounts leave the active list and move to the archive for your main CRM."); ImGui::Spacing(); if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
}

void App::importModal() {
    ImGui::Text("Importing from: %s", importName_.c_str()); ImGui::SameLine(); if (ImGui::SmallButton("Choose another file...")) { auto f = plat.openFiles("Import accounts", "CSV files|*.csv;*.txt;*.tsv|All files|*.*", false); if (!f.empty()) importFiles(f); }
    if (importRows_.empty()) { ImGui::TextWrapped("Nothing to import yet. Choose a CSV file, or copy rows from Excel and use File > Import from clipboard."); if (ImGui::Button("Close")) ImGui::CloseCurrentPopup(); return; }
    const size_t cols = importRows_[0].size(); for (auto& r : importRows_) if (r.size() > cols) { plan_.columns.resize(std::max(plan_.columns.size(), r.size()), Field::None); } plan_.columns.resize(std::max(plan_.columns.size(), cols), Field::None);
    ImGui::TextWrapped("%zu rows. Check what each column is. Contactica guessed from the headers; change any that are wrong, or pick (skip).", importRows_.size() - (plan_.hasHeader ? 1 : 0)); ImGui::Checkbox("First row is a header", &plan_.hasHeader); ImGui::SameLine(); ImGui::Checkbox("Skip shops I already have", &plan_.skipDuplicates);
    ImGui::SetNextItemWidth(220); if (ImGui::BeginCombo("Type for rows without one", plan_.defaultType.c_str())) { for (auto& t : accountTypes()) if (ImGui::Selectable(t.c_str(), t == plan_.defaultType)) plan_.defaultType = t; ImGui::EndCombo(); } ImGui::SameLine(); ImGui::SetNextItemWidth(200); ImGui::InputTextWithHint("##src", "Source label (e.g. NAMM 2026)", &plan_.defaultSource);
    const float tableH = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.2f;
    if (ImGui::BeginTable("##imp", (int)plan_.columns.size(), ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, ImVec2(0, tableH))) {
        for (size_t c = 0; c < plan_.columns.size(); ++c) ImGui::TableSetupColumn(("##c" + std::to_string(c)).c_str(), ImGuiTableColumnFlags_WidthFixed, 150.f); ImGui::TableNextRow();
        for (size_t c = 0; c < plan_.columns.size(); ++c) { ImGui::TableSetColumnIndex((int)c); ImGui::PushID((int)c); ImGui::SetNextItemWidth(140); if (ImGui::BeginCombo("##f", fieldLabel(plan_.columns[c]))) { for (int f = 0; f < (int)Field::Count; ++f) if (ImGui::Selectable(fieldLabel((Field)f), plan_.columns[c] == (Field)f)) plan_.columns[c] = (Field)f; ImGui::EndCombo(); } ImGui::PopID(); }
        for (size_t r = 0; r < std::min<size_t>(importRows_.size(), 9); ++r) { ImGui::TableNextRow(); for (size_t c = 0; c < plan_.columns.size(); ++c) { ImGui::TableSetColumnIndex((int)c); if (c < importRows_[r].size()) { if (r == 0 && plan_.hasHeader) ImGui::TextDisabled("%s", importRows_[r][c].c_str()); else ImGui::TextUnformatted(importRows_[r][c].c_str()); } } }
        ImGui::EndTable(); }
    bool hasName = false; for (auto f : plan_.columns) if (f == Field::Name) hasName = true; if (!hasName) ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "One column must be 'Shop name'."); 
    ImGui::BeginDisabled(!hasName); if (ImGui::Button("Import")) { snapshot(); lastResult_ = importRows(db, importRows_, plan_); touch(); importRows_.clear(); resultOpen_ = true; fitNext_ = true; ImGui::CloseCurrentPopup(); } ImGui::EndDisabled(); ImGui::SameLine(); if (ImGui::Button("Cancel")) { importRows_.clear(); ImGui::CloseCurrentPopup(); }
}

}  // namespace ctc
