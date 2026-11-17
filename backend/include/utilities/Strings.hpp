#pragma once
// ============================================================================
// NEXA :: string helpers (tokenising capability lists, case folding, etc.)
// ============================================================================
#include <string>
#include <cstdio>
#include <ctime>
#include "../data_structures/DynamicArray.hpp"

namespace nexa::util {

inline std::string lower(const std::string& s) {
    std::string o = s;
    for (char& c : o) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return o;
}

inline std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\n' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\n' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

/// "Medical, Trauma ,transport" -> ["medical","trauma","transport"]
inline ds::DynamicArray<std::string> splitList(const std::string& s, char sep = ',') {
    ds::DynamicArray<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { std::string t = lower(trim(cur)); if (!t.empty()) out.pushBack(t); cur.clear(); }
        else cur += c;
    }
    std::string t = lower(trim(cur));
    if (!t.empty()) out.pushBack(t);
    return out;
}

inline std::string joinList(const ds::DynamicArray<std::string>& v, const char* sep = ",") {
    std::string o;
    for (std::size_t i = 0; i < v.size(); ++i) { if (i) o += sep; o += v[i]; }
    return o;
}

/// Normalised capability string (lower-case, trimmed, de-duplicated).
inline std::string normaliseList(const std::string& s) {
    auto items = splitList(s);
    ds::DynamicArray<std::string> uniq;
    for (const auto& x : items) if (uniq.findIndex([&](const std::string& y) { return y == x; }) < 0) uniq.pushBack(x);
    return joinList(uniq);
}

/// Words of a free text (letters/digits only, length >= 2), lower-cased.
inline ds::DynamicArray<std::string> words(const std::string& s) {
    ds::DynamicArray<std::string> out;
    std::string cur;
    auto flush = [&]() { if (cur.size() >= 2) out.pushBack(cur); cur.clear(); };
    for (char c : s) {
        bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        if (alnum) cur += (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; else flush();
    }
    flush();
    return out;
}

inline long nowEpoch() { return (long)std::time(nullptr); }

inline std::string isoTime(long epoch) {
    if (epoch <= 0) return "";
    std::time_t t = (std::time_t)epoch;
    std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return buf;
}

inline std::string padNumber(long n, int width) {
    std::string s = std::to_string(n);
    while ((int)s.size() < width) s = "0" + s;
    return s;
}

} // namespace nexa::util
