// PocketEngine Editor — Console panel + log capture
//
// The EditorConsole captures every PE_LOG call by redirecting stderr to a
// non-blocking pipe. A background reader thread drains the pipe, parses each
// formatted log line ("HH:MM:SS LEVEL  [tag] message") back into a structured
// ConsoleEntry, and pushes it into a thread-safe ring buffer (max 1024).
//
// The Console panel renders the ring buffer with filters (level checkboxes +
// free-text search). Colours match the engine's LogLevel palette.
//
// Why stderr capture?  The engine's `pocket::Logger` writes formatted lines
// to stderr unconditionally (with or without ANSI colour). We can't modify
// the Logger to add a sink callback (engine files are frozen), so we dup2
// the write-end of a pipe over STDERR_FILENO. This is portable across Linux
// / Termux and captures all engine + editor log output uniformly.
//
// To make parsing deterministic we also call Logger::setColored(false) so the
// captured lines are plain text (no ANSI escapes).

#include "editor/panels.h"
#include "editor/editor.h"

#include "pocket/core/log.h"
#include "pocket/core/types.h"

#include "imgui.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <chrono>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <cstdarg>
#include <algorithm>

namespace pocket::editor {

struct ConsoleEntry {
    pocket::LogLevel level;
    std::string      timestamp;
    std::string      tag;
    std::string      message;
};

class EditorConsole::Impl {
public:
    std::deque<ConsoleEntry> entries_;
    mutable std::mutex       mtx_;
    std::thread              reader_;
    std::atomic<bool>        running_{false};
    int                      readFd_     = -1;
    int                      savedStderr_= -1;
    char                     textFilter_[128] = {0};
    u32                      levelMask_ = 0xFF; // all levels

    void parseLine(const std::string& line) {
        // Expected format: "HH:MM:SS LEVEL  [tag] message"
        if (line.size() < 16) { push(pocket::LogLevel::Info, "", line.c_str()); return; }
        ConsoleEntry e;
        e.timestamp = line.substr(0, 8);
        std::string lvlStr = line.substr(9, 6);
        while (!lvlStr.empty() && lvlStr.back() == ' ') lvlStr.pop_back();
        if      (lvlStr == "TRACE") e.level = pocket::LogLevel::Trace;
        else if (lvlStr == "DEBUG") e.level = pocket::LogLevel::Debug;
        else if (lvlStr == "INFO")  e.level = pocket::LogLevel::Info;
        else if (lvlStr == "WARN")  e.level = pocket::LogLevel::Warn;
        else if (lvlStr == "ERROR") e.level = pocket::LogLevel::Error;
        else if (lvlStr == "FATAL") e.level = pocket::LogLevel::Fatal;
        else                        e.level = pocket::LogLevel::Info;

        size_t lb = line.find('[', 16);
        size_t rb = (lb == std::string::npos) ? std::string::npos : line.find(']', lb);
        if (lb != std::string::npos && rb != std::string::npos) {
            e.tag = line.substr(lb + 1, rb - lb - 1);
            size_t msgStart = rb + 2; // skip "] "
            e.message = (msgStart < line.size()) ? line.substr(msgStart) : "";
        } else {
            e.tag = "";
            e.message = line;
        }
        pushEntry(std::move(e));
    }

    void pushEntry(ConsoleEntry&& e) {
        std::lock_guard<std::mutex> g(mtx_);
        entries_.push_back(std::move(e));
        if (entries_.size() > 1024) entries_.pop_front();
    }

    void push(pocket::LogLevel lvl, const char* tag, const char* fmt, ...) {
        ConsoleEntry e;
        e.level = lvl;
        e.tag   = tag ? tag : "";
        char buf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        e.message = buf;
        // Timestamp
        std::time_t now = std::time(nullptr);
        char tb[16];
        std::strftime(tb, sizeof(tb), "%H:%M:%S", std::localtime(&now));
        e.timestamp = tb;
        pushEntry(std::move(e));
    }

    void readerLoop() {
        char buf[4096];
        std::string pending;
        while (running_) {
            ssize_t n = read(readFd_, buf, sizeof(buf));
            if (n > 0) {
                pending.append(buf, buf + n);
                size_t pos;
                while ((pos = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, pos);
                    pending.erase(0, pos + 1);
                    if (!line.empty()) parseLine(line);
                }
            } else if (n == 0) {
                break; // EOF
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } else {
                    break; // fatal read error
                }
            }
        }
        // Final drain after stop signal
        ssize_t n;
        while ((n = read(readFd_, buf, sizeof(buf))) > 0) {
            pending.append(buf, buf + n);
            size_t pos;
            while ((pos = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, pos);
                pending.erase(0, pos + 1);
                if (!line.empty()) parseLine(line);
            }
        }
    }
};

EditorConsole::EditorConsole() : impl_(new Impl()) {}
EditorConsole::~EditorConsole() { shutdown(); delete impl_; }

EditorConsole& EditorConsole::instance() {
    static EditorConsole s;
    return s;
}

void EditorConsole::init() {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        PE_ERROR("editor", "Console: pipe() failed — logs will not be captured");
        return;
    }
    // Non-blocking read end
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    // Save original stderr so we can restore on shutdown.
    impl_->savedStderr_ = dup(STDERR_FILENO);
    // Redirect stderr to the pipe write end.
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    impl_->readFd_ = pipefd[0];

    // Ask the engine Logger to disable ANSI colour codes for clean parsing.
    pocket::Logger::instance().setColored(false);

    impl_->running_ = true;
    impl_->reader_ = std::thread([this]{ impl_->readerLoop(); });
    PE_INFO("editor", "Console log capture started (stderr → pipe, 1024-entry ring buffer)");
}

void EditorConsole::shutdown() {
    if (!impl_->running_) return;
    impl_->running_ = false;
    // Wake the reader: write a sentinel newline so read() returns.
    if (impl_->readFd_ != -1) {
        // Give the reader a moment to drain remaining logs.
        if (impl_->reader_.joinable()) impl_->reader_.join();
        close(impl_->readFd_);
        impl_->readFd_ = -1;
    }
    // Restore stderr
    if (impl_->savedStderr_ != -1) {
        dup2(impl_->savedStderr_, STDERR_FILENO);
        close(impl_->savedStderr_);
        impl_->savedStderr_ = -1;
    }
}

void EditorConsole::getEntries(pocket::Vector<ConsoleEntry>& out, u32 filterMask,
                               const char* textFilter) const {
    std::lock_guard<std::mutex> g(impl_->mtx_);
    out.clear();
    out.reserve(impl_->entries_.size());
    for (const auto& e : impl_->entries_) {
        if (((1u << (u32)e.level) & filterMask) == 0) continue;
        if (textFilter && textFilter[0]) {
            if (std::strstr(e.message.c_str(), textFilter) == nullptr &&
                std::strstr(e.tag.c_str(), textFilter) == nullptr) continue;
        }
        out.push_back(e);
    }
}

void EditorConsole::clear() {
    std::lock_guard<std::mutex> g(impl_->mtx_);
    impl_->entries_.clear();
}

void EditorConsole::push(pocket::LogLevel lvl, const char* tag, const char* fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    impl_->push(lvl, tag, "%s", buf);
}

u32 EditorConsole::levelMask() const { return impl_->levelMask_; }
void EditorConsole::setLevelMask(u32 mask) { impl_->levelMask_ = mask; }
char* EditorConsole::textFilterBuffer() { return impl_->textFilter_; }
const char* EditorConsole::textFilter() const { return impl_->textFilter_; }

// ============================================================
// Console panel rendering
// ============================================================
namespace {

ImVec4 levelColor(pocket::LogLevel l) {
    using L = pocket::LogLevel;
    switch (l) {
        case L::Trace: return ImVec4(0.62f, 0.62f, 0.62f, 1.0f);
        case L::Debug: return ImVec4(0.20f, 0.80f, 0.80f, 1.0f);
        case L::Info:  return ImVec4(0.55f, 0.90f, 0.55f, 1.0f);
        case L::Warn:  return ImVec4(0.95f, 0.80f, 0.30f, 1.0f);
        case L::Error: return ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
        case L::Fatal: return ImVec4(0.95f, 0.20f, 0.20f, 1.0f);
    }
    return ImVec4(1,1,1,1);
}

const char* levelStr(pocket::LogLevel l) {
    using L = pocket::LogLevel;
    switch (l) {
        case L::Trace: return "TRACE";
        case L::Debug: return "DEBUG";
        case L::Info:  return "INFO";
        case L::Warn:  return "WARN";
        case L::Error: return "ERROR";
        case L::Fatal: return "FATAL";
    }
    return "?";
}

} // namespace

void renderConsolePanel() {
    ImGui::Begin("Console");

    EditorConsole& con = EditorConsole::instance();
    u32 mask = con.levelMask();
    char* filterBuf = con.textFilterBuffer();

    // Toolbar: level filter checkboxes + text filter + clear
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    {
        const char* names[] = { "Trace","Debug","Info","Warn","Error","Fatal" };
        for (int i = 0; i < 6; ++i) {
            if (i > 0) ImGui::SameLine();
            bool on = (mask & (1u << i)) != 0;
            if (ImGui::Checkbox(names[i], &on)) {
                if (on) mask |= (1u << i);
                else    mask &= ~(1u << i);
                con.setLevelMask(mask);
            }
        }
    }
    ImGui::SameLine();
    ImGui::PushItemWidth(180.0f);
    ImGui::InputTextWithHint("##console_filter", "Filter (text)...",
                             filterBuf, 128);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Clear")) con.clear();
    ImGui::PopStyleVar();

    ImGui::Separator();

    // Body: scrollable list of entries
    ImGui::BeginChild("##console_body", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    pocket::Vector<ConsoleEntry> snapshot;
    con.getEntries(snapshot, mask, filterBuf);

    // Auto-scroll to bottom if we were at the bottom last frame.
    static bool stickToBottom = true;
    if (stickToBottom) ImGui::SetScrollHereY(1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 1));
    for (const auto& e : snapshot) {
        ImVec4 col = levelColor(e.level);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("[%s] %s [%s] %s",
                    e.timestamp.c_str(), levelStr(e.level), e.tag.c_str(), e.message.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    // Track whether user is at the bottom (for auto-scroll)
    stickToBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

    ImGui::EndChild();
    ImGui::End();
}

} // namespace pocket::editor
