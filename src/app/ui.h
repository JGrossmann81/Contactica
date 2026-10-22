// Small interface helpers: theme, drawn icons (no icon font needed), sliders that reset on double-click, keyframe diamonds.
#pragma once
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <string>

namespace ctc::ui {

inline ImU32 col(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }
namespace pal {
    inline ImU32 accent() { return col(52, 190, 200); }       // teal
    inline ImU32 accent2() { return col(255, 176, 64); }      // amber (playhead, selection)
    inline ImU32 video() { return col(66, 110, 190); }
    inline ImU32 image() { return col(52, 150, 150); }
    inline ImU32 audio() { return col(70, 150, 90); }
    inline ImU32 text() { return col(140, 90, 180); }
    inline ImU32 solid() { return col(120, 120, 130); }
    inline ImU32 panel() { return col(30, 31, 36); }
    inline ImU32 lane() { return col(24, 25, 29); }
}

inline void applyTheme(float scale) {
    ImGuiStyle& s = ImGui::GetStyle(); s = ImGuiStyle(); ImGui::StyleColorsDark(); ImVec4* k = s.Colors;
    auto c = [](int r, int g, int b, int a = 255) { return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f); };
    k[ImGuiCol_WindowBg] = c(24, 25, 30); k[ImGuiCol_ChildBg] = c(28, 29, 34); k[ImGuiCol_PopupBg] = c(34, 35, 42); k[ImGuiCol_Border] = c(52, 54, 64); k[ImGuiCol_FrameBg] = c(40, 42, 50); k[ImGuiCol_FrameBgHovered] = c(52, 55, 66); k[ImGuiCol_FrameBgActive] = c(62, 66, 80);
    k[ImGuiCol_TitleBg] = c(20, 21, 25); k[ImGuiCol_TitleBgActive] = c(30, 31, 38); k[ImGuiCol_MenuBarBg] = c(26, 27, 32); k[ImGuiCol_Header] = c(46, 90, 100); k[ImGuiCol_HeaderHovered] = c(56, 120, 130); k[ImGuiCol_HeaderActive] = c(66, 140, 150);
    k[ImGuiCol_Button] = c(48, 52, 64); k[ImGuiCol_ButtonHovered] = c(64, 72, 90); k[ImGuiCol_ButtonActive] = c(52, 150, 160); k[ImGuiCol_SliderGrab] = c(52, 190, 200); k[ImGuiCol_SliderGrabActive] = c(100, 220, 230); k[ImGuiCol_CheckMark] = c(52, 190, 200);
    k[ImGuiCol_Tab] = c(36, 38, 46); k[ImGuiCol_TabHovered] = c(56, 120, 130); k[ImGuiCol_TabSelected] = c(46, 100, 110); k[ImGuiCol_Separator] = c(52, 54, 64); k[ImGuiCol_ScrollbarBg] = c(22, 23, 27); k[ImGuiCol_ScrollbarGrab] = c(64, 66, 78);
    k[ImGuiCol_Text] = c(226, 228, 234); k[ImGuiCol_TextDisabled] = c(120, 124, 138); k[ImGuiCol_DragDropTarget] = c(255, 176, 64); k[ImGuiCol_TextSelectedBg] = c(52, 190, 200, 90);
    s.WindowRounding = 4; s.ChildRounding = 4; s.FrameRounding = 3; s.GrabRounding = 3; s.PopupRounding = 4; s.TabRounding = 3; s.ScrollbarRounding = 4; s.FramePadding = ImVec2(6, 3); s.ItemSpacing = ImVec2(6, 5); s.WindowPadding = ImVec2(8, 8);
    s.ScaleAllSizes(scale); s.FontScaleMain = scale;
}

enum class Icon { Play, Pause, Stop, ToStart, ToEnd, StepBack, StepFwd, Record, Razor, Magnet, Trash, Plus, Eye, EyeOff, Lock, Unlock, Speaker, Mute, Solo, Up, Down, Cut, Link, Diamond, Check, Flag, Star, Search, Phone };
inline void drawIcon(ImDrawList* dl, Icon ic, ImVec2 c, float r, ImU32 color) {
    const float t = std::max(1.5f, r * 0.18f);
    switch (ic) {
        case Icon::Play: dl->AddTriangleFilled(ImVec2(c.x - r * 0.55f, c.y - r * 0.7f), ImVec2(c.x - r * 0.55f, c.y + r * 0.7f), ImVec2(c.x + r * 0.75f, c.y), color); break;
        case Icon::Pause: dl->AddRectFilled(ImVec2(c.x - r * 0.6f, c.y - r * 0.65f), ImVec2(c.x - r * 0.15f, c.y + r * 0.65f), color); dl->AddRectFilled(ImVec2(c.x + r * 0.15f, c.y - r * 0.65f), ImVec2(c.x + r * 0.6f, c.y + r * 0.65f), color); break;
        case Icon::Stop: dl->AddRectFilled(ImVec2(c.x - r * 0.55f, c.y - r * 0.55f), ImVec2(c.x + r * 0.55f, c.y + r * 0.55f), color); break;
        case Icon::ToStart: dl->AddRectFilled(ImVec2(c.x - r * 0.7f, c.y - r * 0.65f), ImVec2(c.x - r * 0.45f, c.y + r * 0.65f), color); dl->AddTriangleFilled(ImVec2(c.x + r * 0.7f, c.y - r * 0.65f), ImVec2(c.x + r * 0.7f, c.y + r * 0.65f), ImVec2(c.x - r * 0.35f, c.y), color); break;
        case Icon::ToEnd: dl->AddRectFilled(ImVec2(c.x + r * 0.45f, c.y - r * 0.65f), ImVec2(c.x + r * 0.7f, c.y + r * 0.65f), color); dl->AddTriangleFilled(ImVec2(c.x - r * 0.7f, c.y - r * 0.65f), ImVec2(c.x - r * 0.7f, c.y + r * 0.65f), ImVec2(c.x + r * 0.35f, c.y), color); break;
        case Icon::StepBack: dl->AddTriangleFilled(ImVec2(c.x + r * 0.5f, c.y - r * 0.65f), ImVec2(c.x + r * 0.5f, c.y + r * 0.65f), ImVec2(c.x - r * 0.5f, c.y), color); break;
        case Icon::StepFwd: dl->AddTriangleFilled(ImVec2(c.x - r * 0.5f, c.y - r * 0.65f), ImVec2(c.x - r * 0.5f, c.y + r * 0.65f), ImVec2(c.x + r * 0.5f, c.y), color); break;
        case Icon::Record: dl->AddCircleFilled(c, r * 0.6f, color); break;
        case Icon::Razor: dl->AddLine(ImVec2(c.x, c.y - r * 0.85f), ImVec2(c.x, c.y + r * 0.85f), color, t); dl->AddTriangleFilled(ImVec2(c.x - r * 0.45f, c.y - r * 0.85f), ImVec2(c.x + r * 0.45f, c.y - r * 0.85f), ImVec2(c.x, c.y - r * 0.35f), color); break;
        case Icon::Cut: dl->AddLine(ImVec2(c.x - r * 0.6f, c.y - r * 0.6f), ImVec2(c.x + r * 0.6f, c.y + r * 0.6f), color, t); dl->AddLine(ImVec2(c.x + r * 0.6f, c.y - r * 0.6f), ImVec2(c.x - r * 0.6f, c.y + r * 0.6f), color, t); break;
        case Icon::Magnet: dl->PathArcTo(c, r * 0.6f, 3.14159f, 6.28318f, 12); dl->PathStroke(color, 0, t * 1.4f); dl->AddLine(ImVec2(c.x - r * 0.6f, c.y), ImVec2(c.x - r * 0.6f, c.y + r * 0.7f), color, t * 1.4f); dl->AddLine(ImVec2(c.x + r * 0.6f, c.y), ImVec2(c.x + r * 0.6f, c.y + r * 0.7f), color, t * 1.4f); break;
        case Icon::Trash: dl->AddRect(ImVec2(c.x - r * 0.5f, c.y - r * 0.4f), ImVec2(c.x + r * 0.5f, c.y + r * 0.75f), color, 1.f, 0, t); dl->AddLine(ImVec2(c.x - r * 0.7f, c.y - r * 0.5f), ImVec2(c.x + r * 0.7f, c.y - r * 0.5f), color, t); dl->AddLine(ImVec2(c.x - r * 0.2f, c.y - r * 0.8f), ImVec2(c.x + r * 0.2f, c.y - r * 0.8f), color, t); break;
        case Icon::Plus: dl->AddLine(ImVec2(c.x - r * 0.6f, c.y), ImVec2(c.x + r * 0.6f, c.y), color, t * 1.3f); dl->AddLine(ImVec2(c.x, c.y - r * 0.6f), ImVec2(c.x, c.y + r * 0.6f), color, t * 1.3f); break;
        case Icon::Eye: dl->AddEllipse(c, ImVec2(r * 0.8f, r * 0.45f), color, 0.f, 16, t); dl->AddCircleFilled(c, r * 0.22f, color); break;
        case Icon::EyeOff: dl->AddEllipse(c, ImVec2(r * 0.8f, r * 0.45f), color, 0.f, 16, t); dl->AddLine(ImVec2(c.x - r * 0.7f, c.y + r * 0.6f), ImVec2(c.x + r * 0.7f, c.y - r * 0.6f), color, t); break;
        case Icon::Lock: dl->AddRectFilled(ImVec2(c.x - r * 0.5f, c.y - r * 0.1f), ImVec2(c.x + r * 0.5f, c.y + r * 0.7f), color, 1.f); dl->PathArcTo(ImVec2(c.x, c.y - r * 0.1f), r * 0.35f, 3.14159f, 6.28318f, 10); dl->PathStroke(color, 0, t); break;
        case Icon::Unlock: dl->AddRectFilled(ImVec2(c.x - r * 0.5f, c.y - r * 0.1f), ImVec2(c.x + r * 0.5f, c.y + r * 0.7f), color, 1.f); dl->PathArcTo(ImVec2(c.x + r * 0.4f, c.y - r * 0.1f), r * 0.35f, 3.14159f, 5.2f, 10); dl->PathStroke(color, 0, t); break;
        case Icon::Speaker: dl->AddRectFilled(ImVec2(c.x - r * 0.7f, c.y - r * 0.25f), ImVec2(c.x - r * 0.3f, c.y + r * 0.25f), color); dl->AddTriangleFilled(ImVec2(c.x - r * 0.3f, c.y - r * 0.25f), ImVec2(c.x - r * 0.3f, c.y + r * 0.25f), ImVec2(c.x + r * 0.1f, c.y + r * 0.7f), color); dl->AddTriangleFilled(ImVec2(c.x - r * 0.3f, c.y - r * 0.25f), ImVec2(c.x + r * 0.1f, c.y - r * 0.7f), ImVec2(c.x + r * 0.1f, c.y + r * 0.7f), color); dl->PathArcTo(c, r * 0.6f, -0.7f, 0.7f, 8); dl->PathStroke(color, 0, t); break;
        case Icon::Mute: drawIcon(dl, Icon::Speaker, c, r, color); dl->AddLine(ImVec2(c.x - r * 0.7f, c.y + r * 0.8f), ImVec2(c.x + r * 0.8f, c.y - r * 0.8f), color, t * 1.3f); break;
        case Icon::Solo: dl->AddCircle(c, r * 0.6f, color, 14, t); dl->AddCircleFilled(c, r * 0.22f, color); break;
        case Icon::Up: dl->AddTriangleFilled(ImVec2(c.x - r * 0.6f, c.y + r * 0.4f), ImVec2(c.x + r * 0.6f, c.y + r * 0.4f), ImVec2(c.x, c.y - r * 0.55f), color); break;
        case Icon::Down: dl->AddTriangleFilled(ImVec2(c.x - r * 0.6f, c.y - r * 0.4f), ImVec2(c.x + r * 0.6f, c.y - r * 0.4f), ImVec2(c.x, c.y + r * 0.55f), color); break;
        case Icon::Link: dl->AddCircle(ImVec2(c.x - r * 0.3f, c.y), r * 0.45f, color, 12, t); dl->AddCircle(ImVec2(c.x + r * 0.3f, c.y), r * 0.45f, color, 12, t); break;
        case Icon::Diamond: dl->AddQuadFilled(ImVec2(c.x, c.y - r * 0.7f), ImVec2(c.x + r * 0.7f, c.y), ImVec2(c.x, c.y + r * 0.7f), ImVec2(c.x - r * 0.7f, c.y), color); break;
        case Icon::Flag: dl->AddLine(ImVec2(c.x - r * 0.5f, c.y - r * 0.8f), ImVec2(c.x - r * 0.5f, c.y + r * 0.8f), color, t); dl->AddTriangleFilled(ImVec2(c.x - r * 0.5f, c.y - r * 0.8f), ImVec2(c.x + r * 0.8f, c.y - r * 0.35f), ImVec2(c.x - r * 0.5f, c.y + r * 0.1f), color); break;
        case Icon::Star: { ImVec2 pts[10]; for (int i = 0; i < 10; ++i) { const float a = -1.5708f + i * 0.62832f, rr = (i % 2) ? r * 0.4f : r * 0.85f; pts[i] = ImVec2(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr); } dl->AddConvexPolyFilled(pts, 10, color); break; }
        case Icon::Search: dl->AddCircle(ImVec2(c.x - r * 0.15f, c.y - r * 0.15f), r * 0.5f, color, 14, t); dl->AddLine(ImVec2(c.x + r * 0.2f, c.y + r * 0.2f), ImVec2(c.x + r * 0.75f, c.y + r * 0.75f), color, t * 1.3f); break;
        case Icon::Phone: dl->AddRectFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.75f), ImVec2(c.x + r * 0.35f, c.y + r * 0.75f), color, 2.f); break;
        case Icon::Check: dl->AddLine(ImVec2(c.x - r * 0.6f, c.y), ImVec2(c.x - r * 0.15f, c.y + r * 0.5f), color, t * 1.4f); dl->AddLine(ImVec2(c.x - r * 0.15f, c.y + r * 0.5f), ImVec2(c.x + r * 0.7f, c.y - r * 0.5f), color, t * 1.4f); break;
    }
}
// a square button with a drawn icon; `on` shows it as switched on
inline bool iconButton(const char* id, Icon ic, bool on = false, float size = 0.f, const char* tip = nullptr, ImU32 onColor = 0) {
    if (size <= 0.f) size = ImGui::GetFrameHeight(); ImVec2 p = ImGui::GetCursorScreenPos(); const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive(); ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = on ? (onColor ? onColor : pal::accent()) : (act ? col(70, 76, 96) : hov ? col(60, 64, 80) : col(44, 46, 56)); dl->AddRectFilled(p, ImVec2(p.x + size, p.y + size), bg, 4.f);
    drawIcon(dl, ic, ImVec2(p.x + size * 0.5f, p.y + size * 0.5f), size * 0.32f, on ? col(12, 14, 18) : col(220, 224, 232));
    if (tip && hov) ImGui::SetTooltip("%s", tip); return pressed;
}
// slider with its label on the left; double-click resets to `def`
inline bool slider(const char* label, float* v, float lo, float hi, float def, const char* fmt = "%.2f", float labelW = 0.f) {
    ImGui::PushID(label); if (labelW <= 0.f) labelW = ImGui::GetContentRegionAvail().x * 0.34f; ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label); ImGui::SameLine(labelW); ImGui::SetNextItemWidth(-1);
    const bool ch = ImGui::SliderFloat("##s", v, lo, hi, fmt); if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) { *v = def; ImGui::PopID(); return true; } ImGui::PopID(); return ch;
}
inline std::string timecode(double t, double fps) {
    if (t < 0) t = 0; const long long fr = (long long)std::floor(t * fps + 1e-6); const int f = (int)(fr % (long long)std::lround(fps)), s = (int)((fr / (long long)std::lround(fps)) % 60), m = (int)((fr / ((long long)std::lround(fps) * 60)) % 60), h = (int)(fr / ((long long)std::lround(fps) * 3600));
    char b[40]; std::snprintf(b, sizeof(b), "%02d:%02d:%02d:%02d", h, m, s, f); return b;
}

}  // namespace ctc::ui
