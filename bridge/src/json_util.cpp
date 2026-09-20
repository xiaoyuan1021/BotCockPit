#include "botcockpit_bridge/json_util.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace botcockpit::json {
namespace {

size_t skip_ws(const std::string& s, size_t i)
{
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
    ++i;
  }
  return i;
}

// Find start index of value for "key" in a JSON object string.
// Returns npos if not found. Handles nested braces/brackets in values.
bool find_key_value(const std::string& obj, const std::string& key, size_t& value_pos)
{
  const std::string needle = "\"" + key + "\"";
  size_t search = 0;
  while (search < obj.size()) {
    size_t k = obj.find(needle, search);
    if (k == std::string::npos) {
      return false;
    }
    // Ensure this is a key: next non-ws is ':'
    size_t i = skip_ws(obj, k + needle.size());
    if (i >= obj.size() || obj[i] != ':') {
      search = k + needle.size();
      continue;
    }
    value_pos = skip_ws(obj, i + 1);
    return value_pos < obj.size();
  }
  return false;
}

bool parse_json_string_at(const std::string& s, size_t i, std::string& out)
{
  if (i >= s.size() || s[i] != '"') {
    return false;
  }
  ++i;
  out.clear();
  while (i < s.size()) {
    char c = s[i];
    if (c == '\\') {
      if (i + 1 >= s.size()) {
        return false;
      }
      char e = s[i + 1];
      switch (e) {
        case 'n': out.push_back('\n'); break;
        case 't': out.push_back('\t'); break;
        case 'r': out.push_back('\r'); break;
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        default: out.push_back(e); break;
      }
      i += 2;
      continue;
    }
    if (c == '"') {
      return true;
    }
    out.push_back(c);
    ++i;
  }
  return false;
}

}  // namespace

std::string escape(const std::string& s)
{
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(c);
        }
    }
  }
  return out;
}

bool has_key(const std::string& obj, const std::string& key)
{
  size_t vp = 0;
  return find_key_value(obj, key, vp);
}

bool get_string(const std::string& obj, const std::string& key, std::string& out)
{
  size_t vp = 0;
  if (!find_key_value(obj, key, vp)) {
    return false;
  }
  return parse_json_string_at(obj, vp, out);
}

bool get_int(const std::string& obj, const std::string& key, long long& out)
{
  size_t vp = 0;
  if (!find_key_value(obj, key, vp)) {
    return false;
  }
  if (vp < obj.size() && (obj[vp] == '-' || obj[vp] == '+')) {
    ++vp;
  }
  if (vp >= obj.size() || !std::isdigit(static_cast<unsigned char>(obj[vp]))) {
    return false;
  }
  char* end = nullptr;
  const std::string tmp = obj.substr(vp);
  out = std::strtoll(tmp.c_str(), &end, 10);
  return end != tmp.c_str();
}

bool get_double(const std::string& obj, const std::string& key, double& out)
{
  size_t vp = 0;
  if (!find_key_value(obj, key, vp)) {
    return false;
  }
  if (vp < obj.size() && (obj[vp] == '-' || obj[vp] == '+')) {
    ++vp;
  }
  if (vp >= obj.size() ||
      !(std::isdigit(static_cast<unsigned char>(obj[vp])) || obj[vp] == '.')) {
    return false;
  }
  char* end = nullptr;
  const std::string tmp = obj.substr(vp);
  out = std::strtod(tmp.c_str(), &end);
  return end != tmp.c_str();
}

bool get_bool(const std::string& obj, const std::string& key, bool& out)
{
  size_t vp = 0;
  if (!find_key_value(obj, key, vp)) {
    return false;
  }
  if (obj.compare(vp, 4, "true") == 0) {
    out = true;
    return true;
  }
  if (obj.compare(vp, 5, "false") == 0) {
    out = false;
    return true;
  }
  return false;
}

}  // namespace botcockpit::json
