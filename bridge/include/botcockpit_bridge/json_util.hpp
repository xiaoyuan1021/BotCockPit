#pragma once

#include <string>

namespace botcockpit::json {

std::string escape(const std::string& s);

// Minimal field extractors for flat / shallow protocol payloads.
bool get_string(const std::string& obj, const std::string& key, std::string& out);
bool get_int(const std::string& obj, const std::string& key, long long& out);
bool get_double(const std::string& obj, const std::string& key, double& out);
bool get_bool(const std::string& obj, const std::string& key, bool& out);
bool has_key(const std::string& obj, const std::string& key);

}  // namespace botcockpit::json
