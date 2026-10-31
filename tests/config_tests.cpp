#include "config.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {
void checkpoint(const char* message) {
    std::fprintf(stderr, "[config-tests] %s\n", message);
    std::fflush(stderr);
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct TemporaryDirectory {
    fs::path path;

    TemporaryDirectory() {
        checkpoint("resolving temporary directory");
        const auto root = fs::temp_directory_path();
        const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto candidate = root / ("rf-config-test-" + std::to_string(seed) + "-" + std::to_string(attempt));
            std::error_code error;
            if (fs::create_directory(candidate, error)) {
                path = candidate;
                return;
            }
            if (error && error != std::errc::file_exists)
                throw fs::filesystem_error("Could not create test directory", candidate, error);
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
    std::string result;
    char buffer[4096];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() != 0)
        result.append(buffer, static_cast<size_t>(input.gcount()));
    require(!input.bad(), "Could not read test file");
    return result;
}

void testExistingConfiguration(const fs::path& dir) {
    const auto path = dir / "Config.ini";
    const std::string original = "// Existing header\r\n[COMPONENTS]\r\nVideoMode=46\r\n"
                                 "FSAA=37\r\nOther=keep\r\n\r\n[MOD]\r\nExtra=yes\r\n";
    checkpoint("existing: writing initial configuration");
    writeFile(path, original);

    checkpoint("existing: loading initial configuration");
    ConfigFile config;
    require(config.load(path), "Could not load existing Config.ini");

    checkpoint("existing: reading initial values");
    require(config.getInt("components", "VIDEOmode") == 46, "VideoMode lookup must be case-insensitive");
    require(config.getInt("COMPONENTS", "fsaa") == 37, "FSAA lookup must be case-insensitive");

    checkpoint("existing: updating configuration");
    config.setInt("COMPONENTS", "VideoMode", 12);
    config.set("COMPONENTS", "LanguagesFile", "FRANCAIS.DIC");

    checkpoint("existing: saving configuration");
    std::string error;
    if (!config.save(path, error))
        throw std::runtime_error("Could not save existing Config.ini: " + error);

    checkpoint("existing: loading saved configuration");
    ConfigFile reread;
    require(reread.load(path), "Could not reload saved Config.ini");

    checkpoint("existing: validating saved values");
    require(reread.getInt("COMPONENTS", "VideoMode") == 12, "Updated VideoMode was not saved");
    require(reread.getInt("COMPONENTS", "FSAA") == 37, "Unchanged FSAA was not preserved");
    require(reread.get("MOD", "Extra") == "yes", "Other sections were not preserved");
    require(reread.get("COMPONENTS", "Other") == "keep", "Unrelated keys were not preserved");
    require(reread.get("COMPONENTS", "LanguagesFile") == "FRANCAIS.DIC", "New key was not saved");

    checkpoint("existing: checking CRLF and backup");
    const std::string saved = readFile(path);
    require(saved.find("// Existing header\r\n") == 0, "Header or CRLF line endings were lost");
    require(saved.find("[MOD]\r\nExtra=yes\r\n") != std::string::npos, "Custom section was modified");
    require(readFile(dir / "Config.ini.bak") == original, "Backup must match the original file");
    checkpoint("existing: PASS");
}

void testNewConfiguration(const fs::path& dir) {
    const auto path = dir / "New.ini";
    checkpoint("new: adding configuration defaults");
    ConfigFile fresh;
    fresh.addDefaults();
    fresh.set("COMPONENTS", "VideoGUID", "AABBCCDD-FFFF-EEEE-AAAA-BBBBBBBBBBBB");

    checkpoint("new: saving configuration");
    std::string error;
    if (!fresh.save(path, error))
        throw std::runtime_error("Could not save new Config.ini: " + error);

    checkpoint("new: loading configuration");
    ConfigFile loaded;
    require(loaded.load(path), "Could not load new Config.ini");

    checkpoint("new: validating default values");
    require(loaded.get("COMPONENTS", "VehicleSound") == "GameData\\Sounds\\", "Default VehicleSound is incorrect");
    require(loaded.get("COMPONENTS", "VideoGUID") == "AABBCCDD-FFFF-EEEE-AAAA-BBBBBBBBBBBB", "VideoGUID was not saved");
    require(loaded.get("VERSION", "Version") == "-1.000000", "Default version is incorrect");
    require(!fs::exists(dir / "New.ini.bak"), "New configuration must not create an unnecessary backup");
    checkpoint("new: PASS");
}
}

int main(int argc, char** argv) {
    try {
        checkpoint("starting");
        const TemporaryDirectory dir;
        checkpoint("temporary directory created");
        if (argc == 1 || std::string_view(argv[1]) == "existing")
            testExistingConfiguration(dir.path);
        if (argc == 1 || std::string_view(argv[1]) == "new")
            testNewConfiguration(dir.path);
        if (argc > 1 && std::string_view(argv[1]) != "existing" && std::string_view(argv[1]) != "new")
            throw std::runtime_error("Unrecognized test name");
        checkpoint("all selected tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[config-tests] FAIL: %s\n", error.what());
        return 1;
    } catch (...) {
        checkpoint("FAIL: unknown C++ exception");
        return 1;
    }
}
