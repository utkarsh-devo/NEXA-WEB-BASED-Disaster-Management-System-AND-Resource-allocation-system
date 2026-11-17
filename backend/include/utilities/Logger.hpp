#pragma once
// ============================================================================
// NEXA :: TXT logger — appends timestamped lines to logs/nexa.log
// (spec §2.3: "TXT logging will remain supported"). Thread-safe.
// ============================================================================
#include <fstream>
#include <mutex>
#include <string>
#include "Strings.hpp"

namespace nexa::util {

class Logger {
public:
    static Logger& instance() { static Logger l; return l; }

    void setFile(const std::string& path) { std::lock_guard<std::mutex> g(m_); path_ = path; }
    void setEcho(bool echo) { echo_ = echo; }

    void log(const std::string& level, const std::string& source, const std::string& message, const std::string& actor = "system") {
        std::lock_guard<std::mutex> g(m_);
        std::string line = isoTime(nowEpoch()) + " [" + level + "] " + source + " (" + actor + "): " + message;
        if (!path_.empty()) { std::ofstream f(path_, std::ios::app); if (f) f << line << '\n'; }
        if (echo_) { std::fprintf(stderr, "%s\n", line.c_str()); }
    }
    void info(const std::string& src, const std::string& msg, const std::string& actor = "system")  { log("INFO", src, msg, actor); }
    void warn(const std::string& src, const std::string& msg, const std::string& actor = "system")  { log("WARN", src, msg, actor); }
    void error(const std::string& src, const std::string& msg, const std::string& actor = "system") { log("ERROR", src, msg, actor); }
    void audit(const std::string& src, const std::string& msg, const std::string& actor)            { log("AUDIT", src, msg, actor); }

private:
    Logger() = default;
    std::mutex m_;
    std::string path_;
    bool echo_ = false;
};

} // namespace nexa::util
