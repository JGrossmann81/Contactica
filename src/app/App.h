// Contactica - the application: account list, map views, detail editor, import / export, archive of won accounts.
#pragma once
#include <deque>
#include <functional>
#include "../core/csv.h"
#include "Platform.h"
#include "ui.h"
#include <imgui.h>

namespace ctc {

struct Filter {
    std::string text, state, region, brand, type; int stage = -1; bool dueOnly = false;      // stage: -1 any, else a Stage
    bool any() const { return !text.empty() || !state.empty() || !region.empty() || !brand.empty() || !type.empty() || stage >= 0 || dueOnly; }
};

class App {
public:
    explicit App(Platform& p);
    ~App();
    void frame();                                   // between ImGui::NewFrame() and ImGui::Render()
    bool wantsClose();                              // true when it is fine to exit (autosave is flushed)
    void importFiles(const std::vector<std::string>& files);
    bool quitRequested = false;

    // ---- exposed for tests
    Platform& plat; Database db; Filter filter; int selected = 0; int mapTab = 0; int listTab = 0;
    int addAccount(const std::string& name);                        // quick manual entry; selects it
    void stageTo(int id, Stage s);
    void winAccount(int id, const std::string& note, double value); // moves it to the archive
    void loseAccount(int id, const std::string& note);
    void undo(); void redo(); bool canUndo() const { return !undo_.empty(); }
    void flush();                                                   // write the database now
    std::vector<int> visibleIds();                                  // active (or archived) accounts after filters, in list order
    std::map<int, ImVec4> dbgRows;                                  // where list rows / pins were drawn (x0,y0,x1,y1)
    std::map<int, ImVec2> dbgPins;
    std::map<std::string, ImVec4> dbgTiles;
    void testOpen(const char* which) { if (!std::strcmp(which, "import")) importOpen_ = true; else if (!std::strcmp(which, "settings")) settingsOpen_ = true; else if (!std::strcmp(which, "about")) aboutOpen_ = true; else if (!std::strcmp(which, "win")) winOpen_ = true; }
    void importText(const std::string& csv);                        // same as pasting it into the import window
    std::string status() const { return status_; }
    std::string dbPath() const { return plat.dataDir() + "/contactica.json"; }
    bool dirty() const { return dirty_; }

private:
    void menuBar(); void leftPanel(); void centrePanel(); void detailPanel(); void statusBar(); void modals();
    void pinMap(); void tileMap(); void board();
    void accountTable(bool active, Stage onlyStage);
    void importModal(); void settingsModal(); void aboutModal(); void winModal(); void confirmModals();
    void exportAs(ExportKind k, int which);                         // which: 0 active, 1 won, 2 lost, 3 everything
    void touch() { dirty_ = true; dirtySince_ = ImGui::GetTime(); }
    void snapshot();                                                // remember the database for Undo
    bool ed(bool changed) { if (ImGui::IsItemActivated()) snapshot(); if (changed) touch(); return changed; }
    void setStatus(const std::string& s, double secs = 6.0) { status_ = s; statusUntil_ = ImGui::GetTime() + secs; }
    bool matches(const Account& a) const;
    bool field(const char* label, std::string& v, const char* hint = nullptr, float labelW = 0);
    void applyDocumentTitle();

    std::deque<Database> undo_, redo_; bool dirty_ = false; double dirtySince_ = 0, lastSave_ = 0; std::string status_; double statusUntil_ = 0;
    // pin map view: centre in world units (0..1), zoom = world widths across the view
    double viewX_ = 0.2, viewY_ = 0.37, zoom_ = 0.22; bool fitNext_ = true; bool showWon_ = true; ImVec2 mapSize_;
    std::vector<std::vector<std::pair<double, double>>> userMap_; bool userMapLoaded_ = false;
    int tileMetric_ = 0; std::vector<int> sortedIds_; int sortCol_ = 0; bool sortAsc_ = true; bool focusSearch_ = false;
    // dialogs
    bool importOpen_ = false, settingsOpen_ = false, aboutOpen_ = false, winOpen_ = false, deleteAsk_ = false, lostOpen_ = false, resultOpen_ = false;
    std::vector<Row> importRows_; ImportPlan plan_; std::string importName_; std::string importPaste_; ImportResult lastResult_; std::string winNote_, lostNote_; double winValue_ = 0; std::string newBrand_, newNote_; std::string closeDate_;
    bool tableDirtyOrder_ = true;
};

}  // namespace ctc
