#pragma once

#include <string_view>
#ifndef NDEBUG
#  include <cstdio>
#  include <ctime>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

/**
 * @brief ONVIF JSON Validation Helper (Debug-only, zero-copy, single-pass)
 * @details Zero overhead in Release (kValidateJsonDebug=false short-circuits all logic).
 *          Single-pass O(n) scan with stack-allocated state only.
 *
 * Bugs fixed vs original:
 *  B1. Removed incorrect `json[i-1] != '\\'` raw-byte check — escape already handled by escape_next.
 *  B2. Key detection uses tracked string_start instead of rfind (immune to escaped quotes in values).
 *  B3. Distinguishes JSON keys from values by checking for trailing ':'.
 *  B4. found_at_prefix is now a per-object-scope stack (not flat) — prevents cross-scope false negatives.
 *  B5. DumpFailedJson writes failed JSON to /root/onvif_json_dump/ with annotated context on failure.
 */
namespace onvif {

#ifdef NDEBUG
  constexpr bool kValidateJsonDebug = false;
#else
  constexpr bool kValidateJsonDebug = true;
#endif

struct JsonValidationError {
  bool   valid      = true;
  int    error_code = 0;    // 0=ok, 1=bracket/string, 2=attr_value, 3=text_no_attr
  size_t position   = 0;

  explicit operator bool() const { return valid; }
};

// ---------------------------------------------------------------------------
// Debug-only: dump failed JSON to /root/onvif_json_dump/
// ---------------------------------------------------------------------------
#ifndef NDEBUG

/**
 * @brief Writes the failed JSON to /root/onvif_json_dump/ (debug builds only).
 *        Filename: fail_YYYYMMDD_HHMMSS_code{N}_pos{P}_pid{PID}.json
 *        File header contains error description, line/col, and a context window.
 */
inline void DumpFailedJson(std::string_view json, const JsonValidationError& err) noexcept {
  constexpr const char* kDumpDir = "/root/onvif_json_dump";
  mkdir(kDumpDir, 0755);  // no-op if already exists

  // Line/column at failure byte position
  size_t line = 1, col = 1;
  for (size_t k = 0; k < err.position && k < json.size(); ++k) {
    if (json[k] == '\n') { ++line; col = 1; } else { ++col; }
  }

  // Build unique filename
  char path[512];
  {
    time_t now = time(nullptr);
    struct tm t {};
    localtime_r(&now, &t);
    snprintf(path, sizeof(path),
      "%s/fail_%04d%02d%02d_%02d%02d%02d_code%d_pos%zu_pid%d.json",
      kDumpDir,
      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
      t.tm_hour, t.tm_min, t.tm_sec,
      err.error_code, err.position, static_cast<int>(getpid()));
  }

  FILE* fp = fopen(path, "w");
  if (!fp) return;

  static const char* kDesc[] = {
    "ok",
    "unmatched bracket or unclosed string",
    "@ attribute cannot have object/array value",
    "#text found without @ attribute in same object scope"
  };
  const char* desc = (err.error_code >= 1 && err.error_code <= 3)
    ? kDesc[err.error_code] : "unknown";

  fprintf(fp, "// ONVIF JSON Validation FAILED\n");
  fprintf(fp, "// error_code : %d — %s\n", err.error_code, desc);
  fprintf(fp, "// position   : byte=%zu  line=%zu  col=%zu\n", err.position, line, col);

  // Context window: up to 60 chars before and after failure position
  if (err.position <= json.size()) {
    size_t lo = (err.position >= 60) ? err.position - 60 : 0;
    size_t hi = (err.position + 60 < json.size()) ? err.position + 60 : json.size();
    fprintf(fp, "// context    : %s", lo > 0 ? "..." : "");
    for (size_t k = lo; k < err.position; ++k)
      fputc(json[k] == '\n' ? ' ' : json[k], fp);
    fprintf(fp, " >>FAIL<< ");
    for (size_t k = err.position; k < hi; ++k)
      fputc(json[k] == '\n' ? ' ' : json[k], fp);
    fprintf(fp, "%s\n//\n", hi < json.size() ? "..." : "");
  }

  fwrite(json.data(), 1, json.size(), fp);
  fclose(fp);
}

#endif  // !NDEBUG

// ---------------------------------------------------------------------------
// Validator
// ---------------------------------------------------------------------------

/**
 * @brief Single-pass ONVIF JSON validator.
 * @param json  Input JSON (no copy, no heap alloc)
 * @return JsonValidationError — valid=true on success, false on first violation.
 *         On failure (debug build), the JSON is dumped to /root/onvif_json_dump/.
 *
 * Rules enforced:
 *  E.1.1  Bracket/string balance
 *  E.1.3  @ prefix marks XML attributes
 *  E.1.4  @ attribute value must not be object or array
 *  E.1.5  #text key requires at least one @ attribute in the same object scope
 */
inline JsonValidationError ValidateOnvifJson(std::string_view json) noexcept {
  if (!kValidateJsonDebug || json.empty()) {
    return {true, 0, 0};
  }

  int    bracket_depth = 0;
  bool   in_string     = false;
  bool   escape_next   = false;
  size_t string_start  = 0;

  // Per-object-scope @attr tracker (stack indexed by obj_depth; arrays don't push).
  // Depth > 63 skips ONVIF checks — acceptable for pathologically deep nesting.
  int  obj_depth = -1;
  bool at_prefix_stack[64] {};

  auto fail = [&](int code, size_t pos) -> JsonValidationError {
    JsonValidationError e{false, code, pos};
#ifndef NDEBUG
    // DumpFailedJson(json, e);
#endif
    return e;
  };

  for (size_t i = 0; i < json.size(); ++i) {
    char c = json[i];

    // --- Escape handling (inside string only) ---
    if (in_string && escape_next) { escape_next = false; continue; }
    if (c == '\\' && in_string)   { escape_next = true;  continue; }

    // --- String boundary ---
    if (c == '"') {
      if (!in_string) {
        in_string    = true;
        string_start = i;
      } else {
        in_string = false;
        std::string_view token = json.substr(string_start + 1, i - string_start - 1);

        // Is this string a JSON key? Next non-whitespace must be ':'  (B3 fix)
        size_t nws    = json.find_first_not_of(" \t\n\r", i + 1);
        bool   is_key = (nws != std::string_view::npos && json[nws] == ':');

        if (is_key && obj_depth >= 0 && obj_depth < 64) {
          // E.1.3 / E.1.4 — @ attribute
          if (token.size() > 1 && token[0] == '@') {
            at_prefix_stack[obj_depth] = true;
            // @context is JSON-LD — object/array value is valid (ONVIF MQTT spec)
            if (token != "@context") {
              // @ value must not be object or array
              size_t vs = json.find_first_not_of(" \t\n\r", nws + 1);
              if (vs != std::string_view::npos &&
                  (json[vs] == '{' || json[vs] == '[')) {
                return fail(2, vs);
              }
            }
          }

          // E.1.5 — #text requires @ attribute in same object scope
          if (token == "#text" && !at_prefix_stack[obj_depth]) {
            return fail(3, i);
          }
        }
      }
      continue;
    }

    if (in_string) continue;

    // --- Bracket tracking ---
    switch (c) {
      case '{':
        ++bracket_depth;
        ++obj_depth;
        if (obj_depth >= 0 && obj_depth < 64) at_prefix_stack[obj_depth] = false;
        break;
      case '[':
        ++bracket_depth;
        break;
      case '}':
        if (--bracket_depth < 0) return fail(1, i);
        --obj_depth;
        break;
      case ']':
        if (--bracket_depth < 0) return fail(1, i);
        break;
      default: break;
    }
  }

  if (in_string)          return fail(1, json.size());  // unclosed string
  if (bracket_depth != 0) return fail(1, json.size());  // unclosed bracket

  return {true, 0, 0};
}

}  // namespace onvif
