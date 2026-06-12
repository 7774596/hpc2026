#include "hpcsim/config.h"

#include <fstream>
#include <stdexcept>

namespace hpcsim {

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

}  // namespace

Config Config::from_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("config: cannot open " + path);

    Config cfg;
    std::string line;
    while (std::getline(in, line)) {
        auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty() || line.front() == '[') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (!key.empty()) cfg.kv_[key] = val;
    }
    return cfg;
}

std::string Config::get(const std::string& key, const std::string& def) const {
    auto it = kv_.find(key);
    return it == kv_.end() ? def : it->second;
}

int Config::get_int(const std::string& key, int def) const {
    auto it = kv_.find(key);
    return it == kv_.end() ? def : std::stoi(it->second);
}

double Config::get_double(const std::string& key, double def) const {
    auto it = kv_.find(key);
    return it == kv_.end() ? def : std::stod(it->second);
}

}  // namespace hpcsim
