#pragma once

#include <map>
#include <string>

namespace hpcsim {

// 极简 INI 风格配置：key = value，支持 # / ; 注释，[section] 行被忽略
class Config {
public:
    static Config from_file(const std::string& path);

    bool has(const std::string& key) const { return kv_.count(key) > 0; }
    std::string get(const std::string& key, const std::string& def) const;
    int get_int(const std::string& key, int def) const;
    double get_double(const std::string& key, double def) const;

private:
    std::map<std::string, std::string> kv_;
};

}  // namespace hpcsim
