// PocketEngine Editor — Unity-dark ImGui theme + landscape font scaling

#include "editor/theme.h"

#include "pocket/core/log.h"

#include "imgui.h"

#include <cstdio>
#include <cmath>

namespace pocket::editor {

namespace {
bool g_fontsLoaded = false;
}

bool loadEditorFonts() {
    if (g_fontsLoaded) return true;

    ImGuiIO& io = ImGui::GetIO();

    // Landscape-friendly size: 18px reads well at 1280x720 on a phone in
    // landscape. Bump to 20px on >=1920 width.
    float px = 18.0f;
    // We can't query the window here without dragging in window.h; the value
    // is good enough for Termux:X11's typical 1280–1920 widths.
    const char* fontPath = "assets/fonts/Roboto-Medium.ttf";
    ImFont* f = io.Fonts->AddFontFromFileTTF(fontPath, px);
    if (!f) {
        // Fallback: try a sibling path
        f = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", px);
    }
    if (!f) {
        PE_WARN("editor", "Roboto TTF not found under assets/fonts/ — using ImGui default font");
        io.Fonts->AddFontDefault();
        g_fontsLoaded = true;
        return false;
    }
    // Enable merging icons later if needed (e.g. FontAwesome) — placeholder.
    io.FontDefault = f;
    g_fontsLoaded = true;
    PE_INFO("editor", "Editor font loaded: %s @ %gpx", fontPath, (double)px);
    return true;
}

void applyUnityDarkStyle() {
    ImGuiStyle& s = ImGui::GetStyle();

    // Compact spacing for landscape density + 4px rounded corners.
    s.WindowPadding   = ImVec2(8.0f, 8.0f);
    s.FramePadding    = ImVec2(6.0f, 4.0f);
    s.ItemSpacing     = ImVec2(8.0f, 4.0f);
    s.ItemInnerSpacing= ImVec2(6.0f, 4.0f);
    s.IndentSpacing   = 18.0f;
    s.ScrollbarSize   = 14.0f;
    s.GrabMinSize     = 12.0f;

    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize  = 1.0f;
    s.PopupBorderSize  = 1.0f;
    s.FrameBorderSize  = 0.0f;
    s.TabBorderSize    = 0.0f;

    s.WindowRounding   = 4.0f;
    s.ChildRounding    = 4.0f;
    s.FrameRounding    = 4.0f;
    s.PopupRounding    = 4.0f;
    s.ScrollbarRounding= 6.0f;
    s.GrabRounding     = 4.0f;
    s.TabRounding      = 4.0f;

    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    s.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    // ---- Unity-dark palette ----
    // Background ~ #383838, panels ~ #2d2d2d, accent (teal) ~ #3d7d7a.
    ImVec4 bg          = ImVec4(0.220f, 0.220f, 0.224f, 1.00f); // #383838
    ImVec4 bgDark      = ImVec4(0.176f, 0.176f, 0.180f, 1.00f); // #2d2d2d
    ImVec4 bgDarker    = ImVec4(0.130f, 0.130f, 0.134f, 1.00f); // #212121
    ImVec4 panel       = ImVec4(0.196f, 0.196f, 0.200f, 1.00f); // #323233
    ImVec4 panelHover  = ImVec4(0.240f, 0.240f, 0.244f, 1.00f);
    ImVec4 panelActive = ImVec4(0.280f, 0.280f, 0.284f, 1.00f);
    ImVec4 text        = ImVec4(0.890f, 0.890f, 0.890f, 1.00f);
    ImVec4 textDim     = ImVec4(0.560f, 0.560f, 0.560f, 1.00f);
    ImVec4 accent      = ImVec4(0.239f, 0.490f, 0.478f, 1.00f); // #3d7d7a (teal)
    ImVec4 accentHover = ImVec4(0.290f, 0.580f, 0.560f, 1.00f);
    ImVec4 accentActive= ImVec4(0.190f, 0.400f, 0.390f, 1.00f);
    ImVec4 border      = ImVec4(0.120f, 0.120f, 0.120f, 1.00f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]            = text;
    c[ImGuiCol_TextDisabled]    = textDim;
    c[ImGuiCol_WindowBg]        = bgDark;
    c[ImGuiCol_ChildBg]         = bgDark;
    c[ImGuiCol_PopupBg]         = bgDarker;
    c[ImGuiCol_Border]          = border;
    c[ImGuiCol_BorderShadow]    = ImVec4(0,0,0,0);
    c[ImGuiCol_FrameBg]         = panel;
    c[ImGuiCol_FrameBgHovered]  = panelHover;
    c[ImGuiCol_FrameBgActive]   = panelActive;
    c[ImGuiCol_TitleBg]         = bg;
    c[ImGuiCol_TitleBgActive]   = bg;
    c[ImGuiCol_TitleBgCollapsed]= bgDarker;
    c[ImGuiCol_MenuBarBg]       = bg;
    c[ImGuiCol_ScrollbarBg]     = bgDarker;
    c[ImGuiCol_ScrollbarGrab]   = panelActive;
    c[ImGuiCol_ScrollbarGrabHovered] = panelHover;
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_CheckMark]       = accent;
    c[ImGuiCol_SliderGrab]      = accent;
    c[ImGuiCol_SliderGrabActive]= accentHover;
    c[ImGuiCol_Button]          = panel;
    c[ImGuiCol_ButtonHovered]   = panelHover;
    c[ImGuiCol_ButtonActive]    = accentActive;
    c[ImGuiCol_Header]          = accent;
    c[ImGuiCol_HeaderHovered]   = accentHover;
    c[ImGuiCol_HeaderActive]    = accentActive;
    c[ImGuiCol_Separator]       = border;
    c[ImGuiCol_SeparatorHovered]= panelHover;
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip]      = panelActive;
    c[ImGuiCol_ResizeGripHovered]=panelHover;
    c[ImGuiCol_ResizeGripActive]= accent;
    c[ImGuiCol_Tab]             = bg;
    c[ImGuiCol_TabHovered]      = accentHover;
    c[ImGuiCol_TabActive]       = accent;
    c[ImGuiCol_TabUnfocused]    = bgDarker;
    c[ImGuiCol_TabUnfocusedActive]= bg;
    c[ImGuiCol_DockingPreview]  = accent;
    c[ImGuiCol_DockingEmptyBg]  = bgDarker;
    c[ImGuiCol_PlotLines]       = accent;
    c[ImGuiCol_PlotLinesHovered]= accentHover;
    c[ImGuiCol_PlotHistogram]   = accent;
    c[ImGuiCol_PlotHistogramHovered] = accentHover;
    c[ImGuiCol_TableHeaderBg]   = bg;
    c[ImGuiCol_TableBorderStrong]= border;
    c[ImGuiCol_TableBorderLight]= border;
    c[ImGuiCol_TableRowBg]      = ImVec4(0,0,0,0);
    c[ImGuiCol_TableRowBgAlt]   = ImVec4(1,1,1,0.03f);
    c[ImGuiCol_TextSelectedBg]  = accent;
    c[ImGuiCol_DragDropTarget]  = accentHover;
    c[ImGuiCol_NavHighlight]    = accent;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1,1,1,0.30f);
    c[ImGuiCol_NavWindowingDimBg]      = ImVec4(0,0,0,0.50f);
    c[ImGuiCol_ModalWindowDimBg]       = ImVec4(0,0,0,0.55f);
}

void applyEditorTheme() {
    loadEditorFonts();
    applyUnityDarkStyle();
}

} // namespace pocket::editor
