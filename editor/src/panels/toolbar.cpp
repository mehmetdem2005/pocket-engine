// PocketEngine Editor — Toolbar
//
// Horizontal strip at the top of the dockspace host (rendered between the
// main menu bar and the dockspace). Contains:
//   * Play / Pause / Stop buttons (tinted with play-mode colour)
//   * Tool selector: Move | Rotate | Scale (radio-style)
//   * Grid snap toggle
//   * Save scene button
//   * (right-aligned) play-mode status indicator
//
// Drawn with ImGui::Button + SameLine so the strip stays compact for landscape.

#include "editor/editor.h"
#include "editor/panels.h"

#include "pocket/core/log.h"

#include "imgui.h"

namespace pocket::editor {

namespace {

void playButton() {
    bool playing = Editor::instance().isPlaying();
    ImGui::PushStyleColor(ImGuiCol_Button,
        playing ? ImVec4(0.20f, 0.55f, 0.30f, 1.0f) : ImVec4(0.20f, 0.45f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.65f, 0.35f, 1.0f));
    if (ImGui::Button("▶ Play", ImVec2(80, 0))) Editor::instance().play();
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    bool paused = Editor::instance().isPaused();
    ImGui::PushStyleColor(ImGuiCol_Button,
        paused ? ImVec4(0.60f, 0.55f, 0.20f, 1.0f) : ImVec4(0.40f, 0.38f, 0.20f, 1.0f));
    if (ImGui::Button("❚❚ Pause", ImVec2(80, 0))) Editor::instance().pause();
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 1.0f));
    if (ImGui::Button("■ Stop", ImVec2(80, 0))) Editor::instance().stop();
    ImGui::PopStyleColor(2);
}

void toolSelector() {
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    ImGui::TextDisabled("Tool:");
    ImGui::SameLine();

    EditorTool t = Editor::instance().tool();
    bool move   = (t == EditorTool::Move);
    bool rotate = (t == EditorTool::Rotate);
    bool scale  = (t == EditorTool::Scale);

    ImGui::PushStyleColor(ImGuiCol_Button,
        move ? ImVec4(0.239f, 0.490f, 0.478f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("Move", ImVec2(60, 0))) Editor::instance().setTool(EditorTool::Move);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,
        rotate ? ImVec4(0.239f, 0.490f, 0.478f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("Rotate", ImVec2(60, 0))) Editor::instance().setTool(EditorTool::Rotate);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,
        scale ? ImVec4(0.239f, 0.490f, 0.478f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("Scale", ImVec2(60, 0))) Editor::instance().setTool(EditorTool::Scale);
    ImGui::PopStyleColor();
}

void gridSnapToggle() {
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    bool snap = Editor::instance().gridSnap();
    if (ImGui::Checkbox("Grid Snap", &snap)) Editor::instance().setGridSnap(snap);
}

void saveButton() {
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    if (ImGui::Button("Save Scene", ImVec2(100, 0))) {
        Editor::instance().saveScene();
    }
}

void statusIndicator() {
    ImGui::SameLine(ImGui::GetWindowContentRegionMaxX() - 200.0f);
    if (Editor::instance().isPlaying()) {
        if (Editor::instance().isPaused()) {
            ImGui::TextDisabled("⏸ Paused");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.90f, 0.55f, 1.0f));
            ImGui::Text("▶ Playing");
            ImGui::PopStyleColor();
        }
    } else {
        ImGui::TextDisabled("■ Editing");
    }
}

} // namespace

void renderToolbar() {
    // The toolbar is rendered INSIDE the dockspace host window. We use a
    // child region with a fixed height so the dockspace below it isn't
    // disturbed.
    float tbHeight = 28.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.176f, 0.176f, 0.180f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
    ImGui::BeginChild("##toolbar", ImVec2(0, tbHeight), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    playButton();
    toolSelector();
    gridSnapToggle();
    saveButton();
    statusIndicator();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace pocket::editor
