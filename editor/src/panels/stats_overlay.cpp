// PocketEngine Editor — Stats overlay
//
// A small always-on-top ImGui window pinned to the top-right corner of the
// viewport showing live metrics:
//   * FPS + frame time (ms)
//   * Draw calls (placeholder until engine exposes a counter)
//   * Entity count
//   * Resident memory (VmRSS via /proc/self/status, Linux/Termux only)
//
// Rendered with ImGuiWindowFlags_NoDecoration + NoNav so it stays out of the
// user's way and doesn't capture focus.

#include "editor/editor.h"
#include "editor/panels.h"

#include "pocket/core/time.h"
#include "pocket/ecs/ecs.h"

#include "imgui.h"

#include <cstdio>

namespace pocket::editor {

void renderStatsOverlay() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 size(180.0f, 92.0f);
    ImVec2 pos(vp->WorkPos.x + vp->WorkSize.x - size.x - 8.0f,
               vp->WorkPos.y + 32.0f + 8.0f); // below menu bar + toolbar

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 0.78f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.239f, 0.490f, 0.478f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

    ImGui::Begin("##stats_overlay", nullptr, flags);

    float fps = ::pocket::time().fps();
    float dt  = ::pocket::time().delta();
    int   ents = Editor::instance().entityCount();
    int   draws = Editor::instance().drawCallCount();
    float mem  = Editor::instance().memoryMB();

    ImGui::TextDisabled("Stats");
    ImGui::Text("FPS:   %.1f", (double)fps);
    ImGui::Text("Frame: %.2f ms", (double)(dt * 1000.0f));
    ImGui::Text("Draws: %d   Ents: %d", draws, ents);
    ImGui::Text("Mem:   %.2f MB", (double)mem);

    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace pocket::editor
