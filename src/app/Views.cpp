// Contactica: the account list, detail editor, pin map, state tile map ("campaign map") and pipeline board.
#include "App.h"
#include <algorithm>
#include <imgui_stdlib.h>

namespace ctc {

static ImU32 stageColor(Stage s) { switch (s) { case Stage::Prospect: return IM_COL32(120, 150, 200, 255); case Stage::Contacted: return IM_COL32(52, 190, 200, 255); case Stage::Interested: return IM_COL32(120, 210, 120, 255); case Stage::Quoted: return IM_COL32(255, 176, 64, 255); case Stage::Won: return IM_COL32(255, 214, 90, 255); default: return IM_COL32(120, 120, 130, 255); } }
static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

bool App::field(const char* label, std::string& v, const char* hint, float labelW) {
    ImGui::PushID(label); if (labelW <= 0) labelW = 92.f; ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("%s", label); ImGui::SameLine(labelW); ImGui::SetNextItemWidth(-1);
    const bool c = hint ? ed(ImGui::InputTextWithHint("##v", hint, &v)) : ed(ImGui::InputText("##v", &v)); ImGui::PopID(); return c;
}

// ---------------------------------------------------------------- left: filters + list
void App::leftPanel() {
    const std::string t = today(); int due = 0, nAct = 0, nWon = 0, nLost = 0; for (auto& a : db.accounts) { if (a.stage == Stage::Won) ++nWon; else if (a.stage == Stage::Lost) ++nLost; else { ++nAct; if (a.dueBy(t)) ++due; } }
    ImGui::TextColored(col4(ui::pal::accent2()), "CONTACTICA"); ImGui::SameLine(ImGui::GetWindowWidth() - 190); if (ImGui::Button("+ New shop")) addAccount("New shop"); ImGui::SameLine(); if (ImGui::Button("Import...")) { auto f = plat.openFiles("Import accounts", "CSV files|*.csv;*.txt;*.tsv|All files|*.*", false); if (!f.empty()) importFiles(f); }
    if (focusSearch_) { ImGui::SetKeyboardFocusHere(); focusSearch_ = false; } ImGui::SetNextItemWidth(-60); ImGui::InputTextWithHint("##search", "Search name, city, contact, phone...  (Ctrl+F)", &filter.text); ImGui::SameLine(); if (ImGui::Button("Clear")) filter = Filter();
    static std::string quick; ImGui::SetNextItemWidth(-1); if (ImGui::InputTextWithHint("##quick", "Quick add: type a shop name and press Enter", &quick, ImGuiInputTextFlags_EnterReturnsTrue)) { if (!trim(quick).empty()) { addAccount(trim(quick)); quick.clear(); ImGui::SetKeyboardFocusHere(-1); } }
    // filter row
    auto combo = [&](const char* id, const char* any, std::string& v, const std::vector<std::string>& opts, float w) { ImGui::SetNextItemWidth(w); if (ImGui::BeginCombo(id, v.empty() ? any : v.c_str())) { if (ImGui::Selectable(any, v.empty())) v.clear(); for (auto& o : opts) if (ImGui::Selectable(o.c_str(), v == o)) v = o; ImGui::EndCombo(); } };
    const float w = (ImGui::GetContentRegionAvail().x - 3 * ImGui::GetStyle().ItemSpacing.x) / 4.f;
    combo("##reg", "Any region", filter.region, regionNames(), w); ImGui::SameLine(); { std::vector<std::string> codes; for (auto& p : places()) codes.push_back(p.code); combo("##st", "Any state", filter.state, codes, w); } ImGui::SameLine(); combo("##typ", "Any type", filter.type, accountTypes(), w); ImGui::SameLine(); combo("##brand", "Any brand", filter.brand, db.myBrands, w);
    ImGui::Checkbox(("Follow-ups due (" + std::to_string(due) + ")").c_str(), &filter.dueOnly); if (listTab == 0) { ImGui::SameLine(); ImGui::SetNextItemWidth(130); const char* sn[] = {"Any stage", "Prospect", "Contacted", "Interested", "Quoted"}; int si = filter.stage + 1; if (ImGui::Combo("##stg", &si, sn, 5)) filter.stage = si - 1; }
    // tabs
    if (ImGui::BeginTabBar("##lt")) {
        const std::string l0 = "Active (" + std::to_string(nAct) + ")", l1 = "Won (" + std::to_string(nWon) + ")", l2 = "Lost (" + std::to_string(nLost) + ")"; const char* ls[] = {l0.c_str(), l1.c_str(), l2.c_str()};
        static int shown = 0; const int want = listTab; int now = shown;
        for (int i = 0; i < 3; ++i) { if (ImGui::BeginTabItem(ls[i], nullptr, want != shown && i == want ? ImGuiTabItemFlags_SetSelected : 0)) { if (want == shown) now = i; ImGui::EndTabItem(); } }
        if (want == shown && now != want) { listTab = now; if (now != 0) filter.stage = -1; } shown = listTab;
        ImGui::EndTabBar(); }
    accountTable(listTab == 0, Stage::Prospect);
}

void App::accountTable(bool active, Stage) {
    dbgRows.clear(); const std::string t = today();
    const ImGuiTableFlags fl = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Hideable;
    if (!ImGui::BeginTable("##accts", 6, fl)) return;
    ImGui::TableSetupScrollFreeze(0, 1); ImGui::TableSetupColumn("Shop", ImGuiTableColumnFlags_DefaultSort, 3.f); ImGui::TableSetupColumn("City", 0, 1.6f); ImGui::TableSetupColumn("St", 0, 0.5f); ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_DefaultHide, 1.8f);
    ImGui::TableSetupColumn("Stage", 0, 1.1f); ImGui::TableSetupColumn(active ? "Follow-up" : "Closed", 0, 1.2f); ImGui::TableHeadersRow();
    if (ImGuiTableSortSpecs* ss = ImGui::TableGetSortSpecs()) if (ss->SpecsCount > 0) { sortCol_ = ss->Specs[0].ColumnIndex; sortAsc_ = ss->Specs[0].SortDirection == ImGuiSortDirection_Ascending; }
    const std::vector<int> ids = visibleIds(); ImGuiListClipper clip; clip.Begin((int)ids.size());
    while (clip.Step()) for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
        const Account* a = db.find(ids[(size_t)i]); if (!a) continue; ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::PushID(a->id);
        const bool sel = a->id == selected; if (ImGui::Selectable(a->name.empty() ? "(no name)" : a->name.c_str(), sel, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) selected = a->id;
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax(); dbgRows[a->id] = ImVec4(mn.x, mn.y, mx.x, mx.y);
        if (active && ImGui::BeginDragDropSource()) { const int id = a->id; ImGui::SetDragDropPayload("ACCT", &id, sizeof(id)); ImGui::TextUnformatted(a->name.c_str()); ImGui::EndDragDropSource(); }
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(a->city.c_str()); ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(a->state.c_str()); ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(a->type.c_str());
        ImGui::TableSetColumnIndex(4); ImGui::TextColored(col4(stageColor(a->stage)), "%s", stageName(a->stage)); ImGui::TableSetColumnIndex(5);
        if (active) { if (a->nextFollowUp.empty()) ImGui::TextDisabled("-"); else if (a->nextFollowUp < t) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", a->nextFollowUp.c_str()); else if (a->nextFollowUp == t) ImGui::TextColored(col4(ui::pal::accent2()), "today"); else ImGui::TextUnformatted(a->nextFollowUp.c_str()); }
        else { ImGui::TextUnformatted(a->closedDate.c_str()); if (a->firstOrder > 0) { ImGui::SameLine(); ImGui::TextDisabled("$%.0f", a->firstOrder); } }
        ImGui::PopID();
    }
    if (ids.empty()) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextDisabled(filter.any() ? "Nothing matches these filters." : active ? "No active accounts yet. Add a shop, or import a CSV." : "Nothing here yet."); }
    ImGui::EndTable();
}

// ---------------------------------------------------------------- right: detail
void App::detailPanel() {
    const std::string t = today(); Account* a = db.find(selected);
    if (!a) {
        ImGui::TextColored(col4(ui::pal::accent2()), "TODAY"); std::vector<const Account*> due; for (auto& x : db.accounts) if (x.dueBy(t)) due.push_back(&x); std::sort(due.begin(), due.end(), [](const Account* p, const Account* q) { return p->nextFollowUp < q->nextFollowUp; });
        if (due.empty()) ImGui::TextDisabled("No follow-ups due. Pick a shop to see its details."); else { ImGui::Text("%zu follow-up%s due", due.size(), due.size() == 1 ? "" : "s"); ImGui::BeginChild("##due", ImVec2(0, ImGui::GetContentRegionAvail().y * 0.5f), true); for (auto* x : due) { ImGui::PushID(x->id); if (ImGui::Selectable((x->name + "  ").c_str(), false)) selected = x->id; ImGui::SameLine(); ImGui::TextColored(x->nextFollowUp < t ? ImVec4(1, 0.4f, 0.4f, 1) : col4(ui::pal::accent2()), "%s", x->nextFollowUp.c_str()); ImGui::PopID(); } ImGui::EndChild(); }
        ImGui::Spacing(); ImGui::SeparatorText("Territory"); auto st = tallyByState(db, t); int conquered = 0; for (auto& [s, ta] : st) if (s != "?" && ta.won > 0) ++conquered; ImGui::Text("%d states / provinces with a win", conquered); ImGui::TextDisabled("Open the Territories tab for the campaign map.");
        return;
    }
    ImGui::BeginChild("##dscroll"); const bool archived = !a->active();
    ImGui::SetNextItemWidth(-1); ed(ImGui::InputTextWithHint("##name", "Shop name", &a->name)); ImGui::SetNextItemWidth(-1); if (ImGui::BeginCombo("##type", a->type.c_str())) { for (auto& ty : accountTypes()) if (ImGui::Selectable(ty.c_str(), ty == a->type)) { snapshot(); a->type = ty; touch(); } ImGui::EndCombo(); }
    if (!archived) {
        ImGui::SeparatorText("Stage"); const float bw = (ImGui::GetContentRegionAvail().x - 3 * ImGui::GetStyle().ItemSpacing.x) / 4.f;
        for (int s = 0; s < 4; ++s) { if (s) ImGui::SameLine(); const bool cur = (int)a->stage == s; if (cur) ImGui::PushStyleColor(ImGuiCol_Button, col4(stageColor((Stage)s))), ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.07f, 1)); if (ImGui::Button(stageName((Stage)s), ImVec2(bw, 0))) stageTo(a->id, (Stage)s); if (cur) ImGui::PopStyleColor(2); }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 1)); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.75f, 0.25f, 1)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.06f, 0.0f, 1)); if (ImGui::Button("They bought: mark as WON", ImVec2(bw * 2 + ImGui::GetStyle().ItemSpacing.x, 0))) winOpen_ = true; ImGui::PopStyleColor(3);
        ImGui::SameLine(); if (ImGui::Button("Close as lost", ImVec2(-1, 0))) lostOpen_ = true;
        ImGui::SeparatorText("Follow-up"); ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Next"); ImGui::SameLine(92); ImGui::SetNextItemWidth(110); bool bad = !a->nextFollowUp.empty() && normDate(a->nextFollowUp).empty(); if (bad) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.5f, 0.4f, 1)); ed(ImGui::InputTextWithHint("##next", "yyyy-mm-dd", &a->nextFollowUp)); if (bad) ImGui::PopStyleColor();
        const std::pair<const char*, int> qk[] = {{"+3d", 3}, {"+1w", 7}, {"+2w", 14}, {"+1m", 30}}; for (auto& [lab, d] : qk) { ImGui::SameLine(); if (ImGui::SmallButton(lab)) { snapshot(); a->nextFollowUp = addDays(t, d); touch(); } } ImGui::SameLine(); if (ImGui::SmallButton("clear")) { snapshot(); a->nextFollowUp.clear(); touch(); }
    } else {
        ImGui::SeparatorText(a->stage == Stage::Won ? "WON" : "Closed as lost"); ImGui::Text("%s", a->closedDate.c_str()); if (a->firstOrder > 0) { ImGui::SameLine(); ImGui::Text(" first order $%.2f", a->firstOrder); } if (!a->closeNote.empty()) ImGui::TextWrapped("%s", a->closeNote.c_str());
        if (ImGui::Button("Reopen (back to the active list)")) { snapshot(); db.reopen(a->id); touch(); listTab = 0; }
    }
    ImGui::SeparatorText("Contact"); field("Contact", a->contact); field("Title", a->contactTitle);
    field("Phone", a->phone); if (!a->phone.empty()) { ImGui::SameLine(0, 0); } 
    if (!a->phone.empty() && ImGui::SmallButton("Call")) plat.openUrl("tel:" + digits(a->phone)); field("Email", a->email); if (!a->email.empty()) { if (ImGui::SmallButton("Write email")) plat.openUrl("mailto:" + a->email); }
    field("Website", a->website); if (!a->website.empty()) { if (ImGui::SmallButton("Open website")) plat.openUrl(a->website.rfind("http", 0) == 0 ? a->website : "https://" + a->website); }
    ImGui::SeparatorText("Location"); field("Street", a->address); field("City", a->city);
    { ImGui::PushID("stzip"); ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("State"); ImGui::SameLine(92); ImGui::SetNextItemWidth(70); const bool ch = ed(ImGui::InputText("##s", &a->state)); if (ch) { const std::string c = stateCode(a->state); if (!c.empty() && c != a->state && a->state.size() > 2) { a->state = c; } } ImGui::SameLine(); ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("ZIP"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1); ed(ImGui::InputText("##z", &a->zip)); ImGui::PopID(); }
    if (!a->state.empty() && stateCode(a->state).empty()) ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "Unrecognised state: it will not appear on the territory map."); else if (!a->state.empty()) ImGui::TextDisabled("Region: %s", a->region().c_str());
    field("Country", a->country);
    { static int lastId = 0; static std::string las, los; if (lastId != a->id) { lastId = a->id; las = a->hasCoords() ? std::to_string(a->lat) : ""; los = a->hasCoords() ? std::to_string(a->lon) : ""; }
      ImGui::PushID("coords"); ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Lat / Lon"); ImGui::SameLine(92); ImGui::SetNextItemWidth(95); const bool c1 = ImGui::InputText("##la", &las); ed(c1); ImGui::SameLine(); ImGui::SetNextItemWidth(-1); const bool c2 = ImGui::InputText("##lo", &los); ed(c2);
      if (c1 || c2) { double la, lo; if (parseNumber(las, la) && parseNumber(los, lo) && std::fabs(la) <= 90 && std::fabs(lo) <= 180) { a->lat = la; a->lon = lo; } else if (trim(las).empty() && trim(los).empty()) { a->lat = NAN; a->lon = NAN; } } ImGui::PopID();
      if (!a->hasCoords()) ImGui::TextDisabled("No coordinates: drawn near the middle of its state."); }
    ImGui::SeparatorText("Brands I'm pitching"); for (auto& b : db.myBrands) { const bool has = std::find(a->brands.begin(), a->brands.end(), b) != a->brands.end(); bool v = has; if (ImGui::Checkbox(b.c_str(), &v)) { snapshot(); if (v) a->brands.push_back(b); else a->brands.erase(std::remove(a->brands.begin(), a->brands.end(), b), a->brands.end()); touch(); } }
    ImGui::SeparatorText("Notes"); ImGui::SetNextItemWidth(-1); ed(ImGui::InputTextMultiline("##notes", &a->notes, ImVec2(-1, ImGui::GetTextLineHeight() * 4)));
    ImGui::SeparatorText("Activity"); ImGui::SetNextItemWidth(-190); const bool enter = ImGui::InputTextWithHint("##note", "What happened? (Enter to log)", &newNote_, ImGuiInputTextFlags_EnterReturnsTrue); ImGui::SameLine();
    auto logIt = [&](const char* kind) { snapshot(); a->log.push_back({t, std::string(kind) + (newNote_.empty() ? "" : ": " + newNote_)}); newNote_.clear(); if (a->stage == Stage::Prospect && a->active()) { a->stage = Stage::Contacted; } touch(); };
    if (enter && !trim(newNote_).empty()) logIt("Note"); if (ImGui::SmallButton("Call")) logIt("Called"); ImGui::SameLine(); if (ImGui::SmallButton("Email")) logIt("Emailed"); ImGui::SameLine(); if (ImGui::SmallButton("Visit")) logIt("Visited");
    for (size_t i = a->log.size(); i-- > 0;) { ImGui::TextDisabled("%s", a->log[i].date.c_str()); ImGui::SameLine(); ImGui::TextWrapped("%s", a->log[i].note.c_str()); }
    if (a->log.empty()) ImGui::TextDisabled("Nothing logged yet.");
    ImGui::Spacing(); ImGui::Separator(); ImGui::TextDisabled("Added %s%s%s", a->added.c_str(), a->source.empty() ? "" : "  |  source: ", a->source.c_str()); if (ImGui::SmallButton("Delete this account...")) deleteAsk_ = true;
    ImGui::EndChild();
}

// ---------------------------------------------------------------- centre: tabs
void App::centrePanel() {
    const char* names[] = {"Pin map", "Territories", "Pipeline board"}; for (int i = 0; i < 3; ++i) { if (i) ImGui::SameLine(); const bool cur = mapTab == i; if (cur) ImGui::PushStyleColor(ImGuiCol_Button, col4(ui::pal::accent())), ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.06f, 0.08f, 1)); if (ImGui::Button(names[i])) mapTab = i; if (cur) ImGui::PopStyleColor(2); }
    ImGui::SameLine(); ImGui::TextDisabled("  %s", filter.any() ? "(filters apply)" : "");
    if (mapTab == 0) pinMap(); else if (mapTab == 1) tileMap(); else board();
}

// ---------------------------------------------------------------- pin map
void App::pinMap() {
    if (!userMapLoaded_) { userMapLoaded_ = true; std::ifstream f(plat.dataDir() + "/map.geojson"); if (f) { auto j = json::parse(f, nullptr, false); auto addRing = [&](const json& ring) { std::vector<std::pair<double, double>> r; for (auto& p : ring) if (p.is_array() && p.size() >= 2) r.push_back({p[0].get<double>(), p[1].get<double>()}); if (r.size() > 2) userMap_.push_back(r); };
        auto addGeom = [&](const json& g) { if (!g.is_object() || !g.contains("type") || !g.contains("coordinates")) return; const std::string ty = g["type"]; if (ty == "Polygon") for (auto& ring : g["coordinates"]) addRing(ring); else if (ty == "MultiPolygon") for (auto& poly : g["coordinates"]) for (auto& ring : poly) addRing(ring); };
        if (!j.is_discarded()) { if (j.value("type", "") == "FeatureCollection") for (auto& ft : j["features"]) addGeom(ft.value("geometry", json())); else if (j.value("type", "") == "Feature") addGeom(j.value("geometry", json())); else addGeom(j); } } }
    const ImVec2 av = ImGui::GetContentRegionAvail(); const ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + av.x, p0.y + av.y - 2); mapSize_ = ImVec2(av.x, av.y);
    ImGui::InvisibleButton("##map", ImVec2(av.x, av.y - 2), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle); const bool hov = ImGui::IsItemHovered(); const ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList(); dl->PushClipRect(p0, p1, true); dl->AddRectFilled(p0, p1, IM_COL32(14, 16, 21, 255), 4.f);
    // collect what to draw
    struct Pin { int id; double wx, wy; ImVec2 s; Stage st; bool approx, due; };
    std::vector<Pin> pins; const std::string t = today(); double minx = 9, maxx = -9, miny = 9, maxy = -9;
    for (auto& a : db.accounts) { if (!matches(a) && !(a.stage == Stage::Won && showWon_ && !filter.any())) continue; if (a.stage == Stage::Won && !showWon_) continue; if (a.stage == Stage::Lost) continue; if (a.stage != Stage::Won && !a.active()) continue; if (!filter.any() || matches(a)) {
        double la, lo; a.where(la, lo); double x, y; project(la, lo, x, y); pins.push_back({a.id, x, y, ImVec2(), a.stage, !a.hasCoords(), a.dueBy(t)}); minx = std::min(minx, x); maxx = std::max(maxx, x); miny = std::min(miny, y); maxy = std::max(maxy, y); } }
    if (fitNext_ && av.x > 50) { fitNext_ = false; if (pins.empty()) { double x0, y0, x1, y1; project(50, -125, x0, y0); project(24, -66, x1, y1); viewX_ = (x0 + x1) / 2; viewY_ = (y0 + y1) / 2; zoom_ = std::max(x1 - x0, (y1 - y0) * av.x / av.y) * 1.15; } else { viewX_ = (minx + maxx) / 2; viewY_ = (miny + maxy) / 2; zoom_ = std::max({maxx - minx, (maxy - miny) * av.x / av.y, 0.004}) * 1.25; } }
    const double ppw = av.x / zoom_; auto toS = [&](double wx, double wy) { return ImVec2((float)(p0.x + av.x * 0.5 + (wx - viewX_) * ppw), (float)(p0.y + av.y * 0.5 + (wy - viewY_) * ppw)); };
    if (hov && io.MouseWheel != 0.f) { const ImVec2 m = io.MousePos; const double wx = viewX_ + (m.x - p0.x - av.x * 0.5) / ppw, wy = viewY_ + (m.y - p0.y - av.y * 0.5) / ppw; zoom_ = std::clamp(zoom_ / std::pow(1.2, io.MouseWheel), 0.0015, 2.0); const double np = av.x / zoom_; viewX_ = wx - (m.x - p0.x - av.x * 0.5) / np; viewY_ = wy - (m.y - p0.y - av.y * 0.5) / np; }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 4.f)) { viewX_ -= io.MouseDelta.x / ppw; viewY_ -= io.MouseDelta.y / ppw; } else if (ImGui::IsItemActive() && ImGui::IsMouseDragging(2, 0.f)) { viewX_ -= io.MouseDelta.x / ppw; viewY_ -= io.MouseDelta.y / ppw; }
    // graticule
    { double step = 10; if (zoom_ < 0.02) step = 1; if (zoom_ < 0.003) step = 0.1; for (double lon = -180; lon <= 180; lon += step) { double x, y; project(0, lon, x, y); const float sx = toS(x, 0).x; if (sx > p0.x && sx < p1.x) dl->AddLine(ImVec2(sx, p0.y), ImVec2(sx, p1.y), IM_COL32(26, 29, 38, 255)); }
      for (double lat = -80; lat <= 80; lat += step) { double x, y; project(lat, 0, x, y); const float sy = toS(0, y).y; if (sy > p0.y && sy < p1.y) dl->AddLine(ImVec2(p0.x, sy), ImVec2(p1.x, sy), IM_COL32(26, 29, 38, 255)); } }
    auto drawPoly = [&](const std::vector<std::pair<double, double>>& poly, ImU32 fill, ImU32 line) { std::vector<ImVec2> pts; pts.reserve(poly.size()); for (auto& [lo, la] : poly) { double x, y; project(la, lo, x, y); pts.push_back(toS(x, y)); } if (fill) dl->AddConcavePolyFilled(pts.data(), (int)pts.size(), fill); dl->AddPolyline(pts.data(), (int)pts.size(), line, 1.6f, ImDrawFlags_Closed); };
    if (userMap_.empty()) drawPoly(conusOutline(), IM_COL32(30, 34, 44, 255), IM_COL32(70, 78, 98, 255)); else for (auto& r : userMap_) drawPoly(r, IM_COL32(30, 34, 44, 255), IM_COL32(70, 78, 98, 255));
    if (zoom_ > 0.02) { for (auto& p : places()) { if (std::string(p.region) == "Canada") continue; double x, y; project(p.lat, p.lon, x, y); const ImVec2 s = toS(x, y); if (s.x > p0.x && s.x < p1.x && s.y > p0.y && s.y < p1.y) dl->AddText(ImVec2(s.x - 8, s.y - 6), IM_COL32(60, 66, 84, 255), p.code); } }
    // pins, clustered into cells so a crowded view stays readable
    dbgPins.clear(); for (auto& p : pins) p.s = toS(p.wx, p.wy); const float cell = 30.f; std::map<std::pair<int, int>, std::vector<size_t>> cells;
    for (size_t i = 0; i < pins.size(); ++i) { if (pins[i].s.x < p0.x - 20 || pins[i].s.x > p1.x + 20 || pins[i].s.y < p0.y - 20 || pins[i].s.y > p1.y + 20) continue; if (pins[i].st == Stage::Won) continue; cells[{(int)std::floor((pins[i].s.x - p0.x) / cell), (int)std::floor((pins[i].s.y - p0.y) / cell)}].push_back(i); }
    for (auto& p : pins) if (p.st == Stage::Won && p.s.x > p0.x - 10 && p.s.x < p1.x + 10 && p.s.y > p0.y - 10 && p.s.y < p1.y + 10) { dl->AddQuadFilled(ImVec2(p.s.x, p.s.y - 5), ImVec2(p.s.x + 5, p.s.y), ImVec2(p.s.x, p.s.y + 5), ImVec2(p.s.x - 5, p.s.y), IM_COL32(255, 214, 90, 235)); dbgPins[p.id] = p.s; }
    int hit = 0; size_t hitCluster = SIZE_MAX; ImVec2 hitPos; const ImVec2 m = io.MousePos; std::vector<std::pair<ImVec2, std::vector<size_t>>> clusters;
    for (auto& [k, v] : cells) {
        if (v.size() == 1 || zoom_ < 0.0016) { for (size_t i : v) { const Pin& p = pins[i]; const float r = p.id == selected ? 7.f : 5.5f; const ImU32 c = stageColor(p.st); if (p.due) dl->AddCircleFilled(p.s, r + 3.5f, IM_COL32(255, 90, 90, 150));
            if (p.approx) dl->AddCircle(p.s, r, c, 12, 2.f); else dl->AddCircleFilled(p.s, r, c); if (p.id == selected) dl->AddCircle(p.s, r + 3, IM_COL32(255, 255, 255, 255), 16, 2.f); dbgPins[p.id] = p.s;
            if (hov && std::hypot(m.x - p.s.x, m.y - p.s.y) < r + 4) hit = p.id; } }
        else { ImVec2 c(0, 0); bool sel = false, due = false; for (size_t i : v) { c.x += pins[i].s.x; c.y += pins[i].s.y; sel = sel || pins[i].id == selected; due = due || pins[i].due; } c.x /= v.size(); c.y /= v.size(); const float r = 10.f + std::min(8.f, std::log2((float)v.size()) * 2.f);
            dl->AddCircleFilled(c, r, IM_COL32(52, 190, 200, 210)); if (due) dl->AddCircle(c, r + 2, IM_COL32(255, 90, 90, 255), 20, 2.f); if (sel) dl->AddCircle(c, r + 3, IM_COL32(255, 255, 255, 255), 20, 2.f); const std::string n = std::to_string(v.size()); const ImVec2 ts = ImGui::CalcTextSize(n.c_str()); dl->AddText(ImVec2(c.x - ts.x / 2, c.y - ts.y / 2), IM_COL32(10, 14, 18, 255), n.c_str());
            if (hov && std::hypot(m.x - c.x, m.y - c.y) < r) { hitCluster = clusters.size(); hitPos = c; } clusters.push_back({c, v}); }
    }
    if (hit) { const Account* a = db.find(hit); if (a) { ImGui::SetTooltip("%s\n%s%s%s  |  %s%s", a->name.c_str(), a->city.c_str(), a->city.empty() ? "" : ", ", a->state.c_str(), stageName(a->stage), a->hasCoords() ? "" : "  (approximate position)"); } }
    else if (hitCluster != SIZE_MAX) ImGui::SetTooltip("%zu shops here. Click to zoom in.", clusters[hitCluster].second.size());
    if (hov && ImGui::IsMouseReleased(0) && !ImGui::IsMouseDragging(0, 4.f)) {
        if (hit) { selected = hit; if (const Account* a = db.find(hit)) listTab = a->active() ? 0 : a->stage == Stage::Won ? 1 : 2; }
        else if (hitCluster != SIZE_MAX) { const double wx = viewX_ + (hitPos.x - p0.x - av.x * 0.5) / ppw, wy = viewY_ + (hitPos.y - p0.y - av.y * 0.5) / ppw; viewX_ = wx; viewY_ = wy; zoom_ = std::max(0.0015, zoom_ / 2.5); }
    }
    dl->PopClipRect(); dl->AddRect(p0, p1, IM_COL32(60, 64, 80, 255), 4.f);
    // legend and tools (drawn on top)
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 8, p0.y + 8)); if (ImGui::SmallButton("Fit all")) fitNext_ = true; ImGui::SameLine(); ImGui::Checkbox("Show won", &showWon_);
    float lx = p0.x + 10, ly = p1.y - 22; for (int s = 0; s < 4; ++s) { dl->AddCircleFilled(ImVec2(lx + 5, ly + 8), 5, stageColor((Stage)s)); dl->AddText(ImVec2(lx + 14, ly), IM_COL32(170, 176, 192, 255), stageName((Stage)s)); lx += 18 + ImGui::CalcTextSize(stageName((Stage)s)).x + 12; }
    dl->AddQuadFilled(ImVec2(lx + 5, ly + 3), ImVec2(lx + 10, ly + 8), ImVec2(lx + 5, ly + 13), ImVec2(lx, ly + 8), IM_COL32(255, 214, 90, 235)); dl->AddText(ImVec2(lx + 14, ly), IM_COL32(170, 176, 192, 255), "Won"); dl->AddCircle(ImVec2(lx + 70, ly + 8), 5, IM_COL32(200, 200, 210, 255), 12, 2.f); dl->AddText(ImVec2(lx + 80, ly), IM_COL32(170, 176, 192, 255), "approximate position  (red halo = follow-up due)");
}

// ---------------------------------------------------------------- campaign map: one tile per state / province
void App::tileMap() {
    const std::string t = today(); auto st = tallyByState(db, t); auto rg = tallyByRegion(db, t);
    int conquered = 0; for (auto& p : places()) { auto it = st.find(p.code); if (it != st.end() && it->second.won > 0) ++conquered; }
    ImGui::AlignTextToFramePadding(); ImGui::TextColored(col4(ui::pal::accent2()), "CAMPAIGN MAP"); ImGui::SameLine(); ImGui::TextDisabled("  %d of %zu states / provinces conquered", conquered, places().size()); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 250); ImGui::SetNextItemWidth(250);
    const char* metrics[] = {"Shade by: active prospects", "Shade by: wins", "Shade by: win rate", "Shade by: follow-ups due"}; ImGui::Combo("##metric", &tileMetric_, metrics, 4);
    const ImVec2 av = ImGui::GetContentRegionAvail(); const float sumH = 170.f; const float gridH = std::max(120.f, av.y - sumH); const int cols = 12, rows = 10; const float ts = std::clamp(std::min((av.x - 20) / cols, gridH / rows), 28.f, 78.f);
    const ImVec2 org(ImGui::GetCursorScreenPos().x + (av.x - ts * cols) * 0.5f, ImGui::GetCursorScreenPos().y + 6); ImDrawList* dl = ImGui::GetWindowDrawList();
    float maxv = 1; for (auto& [k, v] : st) { if (k == "?") continue; maxv = std::max(maxv, tileMetric_ == 0 ? (float)v.active : tileMetric_ == 1 ? (float)v.won : tileMetric_ == 2 ? (v.total() ? (float)v.won / v.total() : 0.f) : (float)v.due); }
    ImGui::InvisibleButton("##tiles", ImVec2(av.x, ts * rows + 8)); const bool hov = ImGui::IsItemHovered(); const ImVec2 m = ImGui::GetIO().MousePos; const Place* hoverPlace = nullptr; dbgTiles.clear();
    dl->AddText(ImVec2(org.x + ts * 1, org.y + ts * 8.55f), IM_COL32(120, 126, 142, 255), "CANADA");
    for (auto& p : places()) {
        const ImVec2 a(org.x + p.gx * ts + 2, org.y + p.gy * ts + 2), b(a.x + ts - 4, a.y + ts - 4); Tally tl; auto it = st.find(p.code); if (it != st.end()) tl = it->second;
        const float v = tileMetric_ == 0 ? (float)tl.active : tileMetric_ == 1 ? (float)tl.won : tileMetric_ == 2 ? (tl.total() ? (float)tl.won / tl.total() : 0.f) : (float)tl.due; const float k = std::clamp(v / maxv, 0.f, 1.f);
        const ImU32 base = tileMetric_ == 1 || tileMetric_ == 2 ? IM_COL32(255, 190, 60, 255) : tileMetric_ == 3 ? IM_COL32(255, 100, 100, 255) : IM_COL32(52, 190, 200, 255); const ImVec4 bc = col4(base); const ImVec4 dark(0.13f, 0.14f, 0.18f, 1.f);
        const ImVec4 fill(dark.x + (bc.x * 0.8f - dark.x) * k, dark.y + (bc.y * 0.8f - dark.y) * k, dark.z + (bc.z * 0.8f - dark.z) * k, 1.f); dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(fill), 5.f);
        const bool isF = filter.state == p.code; dl->AddRect(a, b, isF ? IM_COL32(255, 255, 255, 255) : tl.won > 0 ? IM_COL32(255, 214, 90, 200) : IM_COL32(60, 64, 80, 255), 5.f, 0, isF ? 2.5f : 1.2f);
        const ImU32 txt = k > 0.55f ? IM_COL32(10, 12, 16, 255) : IM_COL32(220, 224, 232, 255); dl->AddText(ImVec2(a.x + 4, a.y + 2), txt, p.code);
        char buf[16]; if (tileMetric_ == 2) std::snprintf(buf, sizeof(buf), "%d%%", (int)std::lround(v * 100)); else std::snprintf(buf, sizeof(buf), "%d", (int)v); if (v > 0 && ts >= 34) { const ImVec2 sz = ImGui::CalcTextSize(buf); dl->AddText(ImVec2(a.x + (b.x - a.x - sz.x) / 2, a.y + (b.y - a.y - sz.y) / 2 + 3), txt, buf); }
        if (tl.won > 0) ui::drawIcon(dl, ui::Icon::Flag, ImVec2(b.x - 9, a.y + 10), 6.f, k > 0.55f ? IM_COL32(10, 12, 16, 255) : IM_COL32(255, 214, 90, 255)); if (tl.due > 0) dl->AddCircleFilled(ImVec2(a.x + 6, b.y - 6), 3.5f, IM_COL32(255, 90, 90, 255));
        dbgTiles[p.code] = ImVec4(a.x, a.y, b.x, b.y); if (hov && m.x >= a.x && m.x < b.x && m.y >= a.y && m.y < b.y) hoverPlace = &p;
    }
    if (hoverPlace) { Tally tl; auto it = st.find(hoverPlace->code); if (it != st.end()) tl = it->second; ImGui::SetTooltip("%s (%s)\n%d active   %d won   %d lost\n%d follow-ups due%s", hoverPlace->name, hoverPlace->region, tl.active, tl.won, tl.lost, tl.due, tl.wonValue > 0 ? ("\nFirst orders: $" + std::to_string((int)tl.wonValue)).c_str() : ""); if (ImGui::IsMouseReleased(0)) { filter.state = filter.state == hoverPlace->code ? "" : hoverPlace->code; listTab = 0; } }
    // region summary
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + 10, org.y + ts * rows + 14));
    if (ImGui::BeginTable("##regions", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Region", 0, 1.4f); ImGui::TableSetupColumn("Active"); ImGui::TableSetupColumn("Won"); ImGui::TableSetupColumn("Lost"); ImGui::TableSetupColumn("Win rate"); ImGui::TableSetupColumn("First orders"); ImGui::TableHeadersRow();
        std::vector<std::string> names = regionNames(); names.push_back("Unplaced");
        for (auto& r : names) { auto it = rg.find(r); Tally tl; if (it != rg.end()) tl = it->second; if (r == "Unplaced" && tl.total() == 0) continue; ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); if (ImGui::Selectable(r.c_str(), filter.region == r, ImGuiSelectableFlags_SpanAllColumns)) { filter.region = filter.region == r || r == "Unplaced" ? "" : r; filter.state.clear(); listTab = 0; }
            ImGui::TableSetColumnIndex(1); ImGui::Text("%d", tl.active); ImGui::TableSetColumnIndex(2); ImGui::TextColored(col4(stageColor(Stage::Won)), "%d", tl.won); ImGui::TableSetColumnIndex(3); ImGui::TextDisabled("%d", tl.lost); ImGui::TableSetColumnIndex(4); if (tl.won + tl.lost > 0) ImGui::Text("%d%%", (int)std::lround(100.0 * tl.won / (tl.won + tl.lost))); else ImGui::TextDisabled("-"); ImGui::TableSetColumnIndex(5); ImGui::Text("$%.0f", tl.wonValue); }
        ImGui::EndTable(); }
}

// ---------------------------------------------------------------- pipeline board
void App::board() {
    const std::string t = today(); const ImVec2 av = ImGui::GetContentRegionAvail(); const float colW = (av.x - 5 * ImGui::GetStyle().ItemSpacing.x) / 5.f;
    for (int s = 0; s < 5; ++s) {
        if (s) ImGui::SameLine(); ImGui::PushID(s); ImGui::BeginChild("##col", ImVec2(colW, av.y - 4), true);
        if (s < 4) { int n = 0; for (auto& a : db.accounts) if ((int)a.stage == s && matches(a)) ++n; ImGui::TextColored(col4(stageColor((Stage)s)), "%s", stageName((Stage)s)); ImGui::SameLine(); ImGui::TextDisabled("(%d)", n); }
        else { ImGui::TextColored(col4(stageColor(Stage::Won)), "WON"); ImGui::TextDisabled("Drop a shop here when they buy."); }
        ImGui::Separator();
        if (s < 4) { std::vector<const Account*> col; for (auto& a : db.accounts) if ((int)a.stage == s && matches(a)) col.push_back(&a); std::sort(col.begin(), col.end(), [](const Account* x, const Account* y) { return (x->nextFollowUp.empty() ? "9" : x->nextFollowUp) < (y->nextFollowUp.empty() ? "9" : y->nextFollowUp); });
            for (auto* a : col) { ImGui::PushID(a->id); const bool sel = a->id == selected; ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.24f, 0.3f, 1)); if (ImGui::Selectable("##card", sel, 0, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 2.1f))) selected = a->id; ImGui::PopStyleColor();
                if (ImGui::BeginDragDropSource()) { const int id = a->id; ImGui::SetDragDropPayload("ACCT", &id, sizeof(id)); ImGui::TextUnformatted(a->name.c_str()); ImGui::EndDragDropSource(); }
                const ImVec2 mn = ImGui::GetItemRectMin(); ImDrawList* dl = ImGui::GetWindowDrawList(); dl->AddText(ImVec2(mn.x + 4, mn.y + 2), IM_COL32(230, 232, 240, 255), a->name.c_str()); const std::string sub = a->city + (a->city.empty() ? "" : ", ") + a->state; dl->AddText(ImVec2(mn.x + 4, mn.y + ImGui::GetTextLineHeight() + 3), IM_COL32(130, 136, 154, 255), sub.c_str());
                if (a->dueBy(t)) dl->AddCircleFilled(ImVec2(ImGui::GetItemRectMax().x - 10, mn.y + 10), 4, IM_COL32(255, 90, 90, 255)); dbgRows[a->id] = ImVec4(mn.x, mn.y, ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y); ImGui::PopID(); } }
        // drop target: the whole column
        if (ImGui::BeginDragDropTarget()) { if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ACCT")) { const int id = *(const int*)pl->Data; if (s < 4) stageTo(id, (Stage)s); else { selected = id; winOpen_ = true; } } ImGui::EndDragDropTarget(); }
        ImGui::EndChild(); ImGui::PopID();
    }
}

}  // namespace ctc
