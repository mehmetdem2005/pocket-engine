#pragma once
// PocketEngine — Unity-dark ImGui theme + landscape font scaling
//
// applyEditorTheme() is called once after ImGui::CreateContext(). It:
//   * loads a TTF font (assets/fonts/Roboto-Medium.ttf, fallback ProggyClean)
//     at a size appropriate for landscape Termux:X11 (18px default)
//   * sets ImGuiStyle colors to a Unity-dark palette:
//       background ~ #383838, panels ~ #2d2d2d, accent (teal) ~ #3d7d7a
//   * sets compact spacing/rounding for landscape density (rounding 4px)

#include "pocket/core/types.h"

namespace pocket::editor {

// Apply Unity-dark theme + load landscape font. Safe to call multiple times
// (idempotent on the style side; font load is skipped after first success).
void applyEditorTheme();

// Re-apply just the style colors (used after ImGui::StyleColorsDark reset).
void applyUnityDarkStyle();

// Load fonts into the current ImGuiIO. Returns true if the Roboto TTF was
// found; false if it fell back to the default font.
bool loadEditorFonts();

} // namespace pocket::editor
