#pragma once
// ============================================================================
// NEXA :: CSV reader/writer (RFC 4180 quoting) for import/export.
// ============================================================================
#include <string>
#include "../data_structures/DynamicArray.hpp"

namespace nexa::util {

using Row = ds::DynamicArray<std::string>;

inline std::string csvEscape(const std::string& v) {
    bool needs = v.find_first_of(",\"\n\r") != std::string::npos;
    if (!needs) return v;
    std::string o = "\"";
    for (char c : v) { if (c == '"') o += "\"\""; else o += c; }
    return o + "\"";
}

inline std::string csvLine(const Row& r) {
    std::string o;
    for (std::size_t i = 0; i < r.size(); ++i) { if (i) o += ','; o += csvEscape(r[i]); }
    return o + "\r\n";
}

/// Parses a whole CSV document into rows. Handles quoted fields with commas,
/// escaped quotes and embedded newlines.
inline ds::DynamicArray<Row> parseCsv(const std::string& text) {
    ds::DynamicArray<Row> rows;
    Row cur;
    std::string field;
    bool inQuotes = false, any = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        any = true;
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }
                else inQuotes = false;
            } else field += c;
            continue;
        }
        if (c == '"') inQuotes = true;
        else if (c == ',') { cur.pushBack(field); field.clear(); }
        else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            cur.pushBack(field); field.clear();
            bool blank = cur.size() == 1 && cur[0].empty();
            if (!blank) rows.pushBack(cur);
            cur = Row();
            any = false;
        } else field += c;
    }
    if (any || !field.empty() || !cur.empty()) { cur.pushBack(field); if (!(cur.size() == 1 && cur[0].empty())) rows.pushBack(cur); }
    return rows;
}

} // namespace nexa::util
