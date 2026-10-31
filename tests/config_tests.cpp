#include "config.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main() {
    auto dir = std::filesystem::temp_directory_path() / "rf_config_test";
    std::filesystem::create_directories(dir);
    auto path = dir / "Config.ini";
    {
        std::ofstream output(path, std::ios::binary);
        output << "// Existing header\r\n[COMPONENTS]\r\nVideoMode=46\r\n"
                  "FSAA=37\r\nOther=keep\r\n\r\n[MOD]\r\nExtra=yes\r\n";
    }
    ConfigFile config;
    assert(config.load(path));
    assert(config.getInt("components", "VIDEOmode") == 46);
    assert(config.getInt("COMPONENTS", "fsaa") == 37);
    config.setInt("COMPONENTS", "VideoMode", 12);
    config.set("COMPONENTS", "LanguagesFile", "FRANCAIS.DIC");
    std::string error;
    assert(config.save(path, error));
    ConfigFile reread;
    assert(reread.load(path));
    assert(reread.getInt("COMPONENTS", "VideoMode") == 12);
    assert(reread.getInt("COMPONENTS", "FSAA") == 37);
    assert(reread.get("MOD", "Extra") == "yes");
    assert(reread.get("COMPONENTS", "Other") == "keep");
    std::ifstream input(path, std::ios::binary);
    std::string text(std::istreambuf_iterator<char>{input}, {});
    assert(text.find("\r\n") != std::string::npos);
    assert(text.find("// Existing header\r\n") == 0);
    assert(std::filesystem::exists(dir / "Config.ini.bak"));
    auto other = dir / "New.ini";
    ConfigFile fresh;
    fresh.addDefaults();
    fresh.set("COMPONENTS", "VideoGUID", "AABBCCDD-FFFF-EEEE-AAAA-BBBBBBBBBBBB");
    assert(fresh.save(other, error));
    ConfigFile loaded;
    assert(loaded.load(other));
    assert(loaded.get("COMPONENTS", "VehicleSound") == "GameData\\Sounds\\");
    assert(loaded.get("COMPONENTS", "VideoGUID") == "AABBCCDD-FFFF-EEEE-AAAA-BBBBBBBBBBBB");
    assert(loaded.get("VERSION", "Version") == "-1.000000");
    std::filesystem::remove_all(dir);
    std::cout << "Configuration tests passed\n";
}
