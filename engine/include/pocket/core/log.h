#pragma once
// PocketEngine — logging (printf-style, renkli, dosyaya da yazabilen)
#include "pocket/core/types.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>

namespace pocket {

enum class LogLevel : u8 {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class Logger {
public:
    static Logger& instance() {
        static Logger s;
        return s;
    }

    void setLevel(LogLevel l) { level_ = l; }
    void setFile(FILE* f) { file_ = f; }
    void setColored(bool c) { colored_ = c; }

    void log(LogLevel lvl, const char* tag, const char* fmt, ...) {
        if (lvl < level_) return;
        std::lock_guard<std::mutex> guard(mtx_);

        char buf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        // zaman damgası (qualify `::time`/`::localtime_r` so unqualified lookup
        // does not resolve to pocket::time() accessor once time.h is in scope)
        time_t now = ::time(nullptr);
        struct tm tmv;
        ::localtime_r(&now, &tmv);
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d",
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

        const char* lvlStr[] = {"TRACE","DEBUG","INFO ","WARN ","ERROR","FATAL"};
        const char* lvlCol[] = {"\033[37m","\033[36m","\033[32m","\033[33m","\033[31m","\033[41;37m"};

        if (colored_) {
            std::fprintf(stderr, "\033[90m%s\033[0m %s%-6s\033[0m \033[90m[%s]\033[0m %s\n",
                         tbuf, lvlCol[(int)lvl], lvlStr[(int)lvl], tag ? tag : "", buf);
        } else {
            std::fprintf(stderr, "%s %-6s [%s] %s\n", tbuf, lvlStr[(int)lvl], tag ? tag : "", buf);
        }

        if (file_) {
            std::fprintf(file_, "%s %-6s [%s] %s\n", tbuf, lvlStr[(int)lvl], tag ? tag : "", buf);
            std::fflush(file_);
        }
    }

private:
    Logger() : level_(LogLevel::Info), file_(nullptr), colored_(true) {}
    LogLevel level_;
    FILE* file_;
    bool  colored_;
    std::mutex mtx_;
};

} // namespace pocket

// ---- Makrolar ----
#define PE_LOG(lvl, tag, fmt, ...) \
    ::pocket::Logger::instance().log(::pocket::LogLevel::lvl, tag, fmt, ##__VA_ARGS__)

#define PE_TRACE(tag, fmt, ...) PE_LOG(Trace, tag, fmt, ##__VA_ARGS__)
#define PE_DEBUG(tag, fmt, ...) PE_LOG(Debug, tag, fmt, ##__VA_ARGS__)
#define PE_INFO(tag, fmt, ...)  PE_LOG(Info,  tag, fmt, ##__VA_ARGS__)
#define PE_WARN(tag, fmt, ...)  PE_LOG(Warn,  tag, fmt, ##__VA_ARGS__)
#define PE_ERROR(tag, fmt, ...) PE_LOG(Error, tag, fmt, ##__VA_ARGS__)
#define PE_FATAL(tag, fmt, ...) do { PE_LOG(Fatal, tag, fmt, ##__VA_ARGS__); std::abort(); } while(0)

#define PE_ASSERT(cond, msg) do { if(!(cond)) PE_FATAL("assert", "ASSERT FAIL %s:%d %s — %s", __FILE__, __LINE__, #cond, msg); } while(0)
#define PE_ASSERT_SOFT(cond, msg) do { if(!(cond)) PE_ERROR("assert", "SOFT FAIL %s:%d %s — %s", __FILE__, __LINE__, #cond, msg); } while(0)
