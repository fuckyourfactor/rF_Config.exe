#pragma once

#include <filesystem>
#include <string>
#include <vector>

class ConfigFile {
public:
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path, std::string& error) const;
    bool hasSection(const std::string& section) const;
    std::string get(const std::string& section, const std::string& key, const std::string& fallback = "") const;
    int getInt(const std::string& section, const std::string& key, int fallback = 0) const;
    void set(const std::string& section, const std::string& key, const std::string& value);
    void setInt(const std::string& section, const std::string& key, int value);
    void addDefaults();

private:
    std::vector<std::string> lines_;
    std::string newline_ = "\r\n";
    bool endsWithNewline_ = true;
};
