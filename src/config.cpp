#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
std::string trim(std::string value) {
    auto space = [](unsigned char c) { return std::isspace(c) != 0; };
    auto first = std::find_if_not(value.begin(), value.end(), space);
    auto last = std::find_if_not(value.rbegin(), value.rend(), space).base();
    return first < last ? std::string(first, last) : "";
}

std::string fold(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string sectionOf(const std::string& line) {
    std::string value = trim(line);
    if (value.size() < 2 || value.front() != '[' || value.back() != ']') return "";
    return fold(trim(value.substr(1, value.size() - 2)));
}

bool keyMatch(const std::string& line, const std::string& key) {
    std::string value = trim(line);
    if (value.empty() || value[0] == ';' || value[0] == '#' || value.rfind("//", 0) == 0) return false;
    size_t equal = value.find('=');
    return equal != std::string::npos && fold(trim(value.substr(0, equal))) == fold(key);
}
}

bool ConfigFile::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::ostringstream stream;
    stream << input.rdbuf();
    if (input.bad() || stream.bad()) return false;
    const std::string bytes = stream.str();

    std::vector<std::string> result;
    newline_ = bytes.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    endsWithNewline_ = bytes.empty() || bytes.back() == '\n';
    size_t start = 0;
    while (start < bytes.size()) {
        size_t end = bytes.find('\n', start);
        if (end == std::string::npos) end = bytes.size();
        size_t last = end;
        if (last > start && bytes[last - 1] == '\r') --last;
        result.emplace_back(bytes, start, last - start);
        start = end < bytes.size() ? end + 1 : end;
    }
    lines_ = std::move(result);
    return true;
}

bool ConfigFile::hasSection(const std::string& section) const {
    std::string wanted = fold(section);
    for (const auto& line : lines_)
        if (sectionOf(line) == wanted) return true;
    return false;
}

std::string ConfigFile::get(const std::string& section, const std::string& key,
                            const std::string& fallback) const {
    bool inside = false;
    std::string wanted = fold(section);
    for (const auto& line : lines_) {
        auto current = sectionOf(line);
        if (!current.empty()) { inside = current == wanted; continue; }
        if (inside && keyMatch(line, key)) {
            auto pos = line.find('=');
            return trim(line.substr(pos + 1));
        }
    }
    return fallback;
}

int ConfigFile::getInt(const std::string& section, const std::string& key, int fallback) const {
    std::string value = get(section, key);
    if (value.empty()) return fallback;
    try {
        size_t length = 0;
        int parsed = std::stoi(value, &length, 10);
        return length == value.size() ? parsed : fallback;
    } catch (...) { return fallback; }
}

void ConfigFile::set(const std::string& section, const std::string& key, const std::string& value) {
    bool inside = false;
    size_t insertion = lines_.size();
    std::string wanted = fold(section);
    for (size_t i = 0; i < lines_.size(); ++i) {
        auto current = sectionOf(lines_[i]);
        if (!current.empty()) {
            if (inside) { insertion = i; break; }
            if (current == wanted) { inside = true; insertion = i + 1; }
            continue;
        }
        if (inside) {
            insertion = i + 1;
            if (keyMatch(lines_[i], key)) {
                size_t eq = lines_[i].find('=');
                lines_[i].replace(eq + 1, std::string::npos, value);
                return;
            }
        }
    }
    if (inside) {
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(insertion), key + "=" + value);
    } else {
        if (!lines_.empty() && !lines_.back().empty()) lines_.emplace_back();
        lines_.push_back("[" + section + "]");
        lines_.push_back(key + "=" + value);
    }
    endsWithNewline_ = true;
}

void ConfigFile::setInt(const std::string& section, const std::string& key, int value) {
    set(section, key, std::to_string(value));
}

void ConfigFile::addDefaults() {
    if (hasSection("COMPONENTS")) return;
    lines_.push_back("//[[gMa1.002f (c)2006 ]] [[ ]]");
    lines_.push_back("[COMPONENTS]");
    const std::pair<const char*, const char*> values[] = {
        {"GamesDir", "RFM\\"}, {"SoundDir", "GameData\\Sounds\\"},
        {"VehicleSound", "GameData\\Sounds\\"}, {"OptionsAnim", "UIData\\Movies\\"},
        {"OptionsFlags", "GameData\\Nations\\"}, {"OptionsSounds", "UIData\\UISounds\\"},
        {"OSCFile", "ui.osc"}, {"OptionsDir", "UIData\\"},
        {"ReplayDir", "ReplayFridge\\Replays\\"},
        {"ReplayPluginsDir", "ReplayFridge\\Plugins\\"}, {"PluginsDir", "Plugins\\"},
        {"MovieDir", "MovieFiles\\"}, {"TracksDir", "GameData\\Locations\\"},
        {"PitsDir", "GameData\\Scripts\\"}, {"VehiclesDir", "GameData\\Vehicles\\"},
        {"DriversDir", "GameData\\Talent\\"}, {"HelmetsDir", "GameData\\Helmets\\"},
        {"CommentaryDir", "GameData\\Commentary\\"},
        {"CommentaryFile", "CommentaryEnglish-USA.ini"},
        {"LogDir", "UserData\\Log\\"}, {"SaveDir", "UserData\\"},
        {"MusicDir", "Music\\"}, {"CameraEditorResourceDir", "DevFiles\\CamEd\\"},
        {"ShadersDir", "GameData\\Shared\\"},
        {"ControllerDir", "UserData\\Controller\\"},
        {"ScreenShotsDir", "UserData\\ScreenShots\\"},
        {"SupportDir", "Support\\"}, {"LanguagesDir", "Support\\Languages\\"},
        {"LanguagesFile", "GAME.DIC"}, {"MasterPlayerFile", ""},
        {"CPURating", "4"}, {"SystemRAM", "0"}, {"VideoRAM", "0"},
        {"ObjDetail", "1"}, {"TexDetail", "1"},
        {"VideoGUID", ""}, {"VideoDriver", "0"}, {"VideoMode", "0"},
        {"VideoRefresh", "0"}, {"WindowedMode", "0"},
        {"RenderMethod", "0"}, {"RenderTexSize", "0"},
        {"RunBenchmarks", "0"}, {"FSAA", "0"}, {"VSync", "0"},
        {"ShaderLevel", "3"}, {"SubViews", "0"}, {"WidescreenUI", "1"},
        {"CustomVidRes", "(0, 0)"}, {"MinimumVidRes", "(800, 600)"},
        {"MaximumVidRes", "(8000, 6000)"}
    };
    for (const auto& kv : values) lines_.emplace_back(std::string(kv.first) + "=" + kv.second);
    lines_.emplace_back();
    lines_.emplace_back("[INSTALL]");
    lines_.emplace_back("InstallFrom=");
    lines_.emplace_back("InstallTo=");
    lines_.emplace_back();
    lines_.emplace_back("[VERSION]");
    lines_.emplace_back("Version=-1.000000");
    endsWithNewline_ = true;
}

bool ConfigFile::save(const std::filesystem::path& path, std::string& error) const {
    auto temp = path;
    temp += L".tmp";
    std::ofstream output(temp, std::ios::binary | std::ios::trunc);
    if (!output) { error = "Could not create the temporary config file."; return false; }
    for (size_t i = 0; i < lines_.size(); ++i) {
        output << lines_[i];
        if (i + 1 < lines_.size() || endsWithNewline_) output << newline_;
    }
    output.close();
    if (!output) { error = "Could not write the temporary config file."; return false; }
    std::error_code ec;
    const bool existing = std::filesystem::exists(path, ec);
    if (ec) { error = "Could not inspect Config.ini: " + ec.message(); return false; }
    if (existing) {
        auto backup = path;
        backup += L".bak";
        std::filesystem::copy_file(path, backup,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { error = "Could not create a backup: " + ec.message(); return false; }
    }
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Could not replace Config.ini (Windows error " + std::to_string(GetLastError()) + ").";
        return false;
    }
#else
    std::filesystem::rename(temp, path, ec);
    if (ec) { error = ec.message(); return false; }
#endif
    return true;
}
