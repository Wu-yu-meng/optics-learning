#pragma once
#include <filesystem>
#include "json/ReJson.hpp"

inline ReJson CONFIG((std::filesystem::path(__FILE__).parent_path() / "config.json").string());
inline ReJson CONFIG_TEST((std::filesystem::path(__FILE__).parent_path() / "config_test.json").string());
