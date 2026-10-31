#include "config.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TemporaryDirectory {
    fs::path path;

    TemporaryDirectory() {
        const auto root = fs::temp_directory_path();
        const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto candidate = root / ("rf-config-test-" + std::to_string(seed) + "-" + std::to_string(attempt));
            std::error_code error;
            if (fs::create_directory(candidate, error)) {
                path = candidate;
                return;
            }
            if (error) throw fs::filesystem_error("Could not create test directory", candidate, error);
        }
        throw std::runtime_error("Could not reserve a unique test directory");
    }

    ~TemporaryDirectory() {
        if (!path.empty()) {
            std::error_code error;
            fs::remove_all(path, error);
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
};

void writeFile(const fs::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "Could not open test file for writing");
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    require(static_cast<bool>(output), "Could not write test file");
}

std::string readFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Could not open test file for reading");
    const std::string contents(std::istreambuf_iterator<char>{input}, {});
    require(!input.bad(), "Could not read test file");
    return contents;
}

void testExistingConfiguration(const fs::path& dir) {
    const auto path = dir / "Config.ini";
    const std::string original = "// Existing header\r\n[COMPONENTS]\r\nVideoMode=46\r\n"
                                 "FSAA=37\r\nOther=keep\r\n\r\n[MOD]\r\nExtra=yes\r\n";
    writeFile(path, original);

    ConfigFile config;
    require(config.load(path), "Could not load existing Config.ini");
    require(config.getInt("components", "VIDEOmode") == 46, "VideoMode lookup must be case-insensitive");
    require(config.getInt("COMPONENTS", "fsaa") == 37, "FSAA lookup must be case-insensitive");

    config.setInt("COMPONENTS", "VideoMode", 12);
    config.set("COMPONENTS", "LanguagesFile", "FRANCAIS.DIC");
    std::string error;
    require(config.save(path, error), "Could not save existing Config.ini");

    ConfigFile reread;
    require(reread.load(path), "Could not reload saved Config.ini");
    require(reread.getInt("COMPONENTS", "VideoMode") == 12, "Updated VideoMode was not saved");
    require(reread.getInt("COMPONENTS", "FSAA") == 37, "Unchanged FSAA was not preserved");
    require(reread.get("MOD", "Extra") == "yes", "Other sections were not preserved");
    require(reread.get("COMPONENTS", "Other") == "keep", "Unrelated keys were not preserved");
    require(reread.get("COMPONENTS", "LanguagesFile") == "FRANCAIS.DIC", "New key was not saved");

    const std::string saved = readFile(path);
    require(saved.find("// Existing header\r\n") == 0, "Header or CRLF line endings were lost");
    require(saved.find("\n") != std::string::npos, "Saved file contains no line endings");
    require(saved.find("\r\n") != std::string::npos, "CRLF line endings were not preserved");
    require(saved.find("[MOD]\r\nExtra=yes\r\n") != std::string::npos, "Custom section was modified");
    require(readFile(dir / "Config.ini.bak") == original, "Backup must match the original file");
}

void testNewConfiguration(const fs::path& dir) {
    const auto path = dir / "New.ini";
    ConfigFile fresh;
    fresh.addDefaults();
    fresh.set("COMPONENTS", "VideoGUID", "AABBCCDD-FFFF-EEEE-AAAA-BBBBBBBBBBBB");
    std::string error;
    require(fresh.save(path, error), "Could not save new Config.ini");

    ConfigFile loaded;
    require(loaded.load(path), "Could not load new Config.ini");
    require(loaded.get("COMPONENTS", "VehicleSound") == "GameData\\Sounds\\", "Default VehicleSound is incorrect");
    require(loaded.get("COMPONENTS", "VideoGUID") == "AABBCCDD-FFFF-EEEE-AAAA-BBBBBBBBBBBB", "VideoGUID was not saved");
    require(loaded.get("VERSION", "Version") == "-1.000000", "Default version is incorrect");
    require(!fs::exists(dir / "New.ini.bak"), "New configuration must not create an unnecessary backup");
}
}

int main() {
    try {
        const TemporaryDirectory dir;
        testExistingConfiguration(dir.path);
        testNewConfiguration(dir.path);
        std::cout << "Configuration tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Configuration tests failed: " << error.what() << '\n';
        return 1;
    }
}
