#pragma once
#include <filesystem>
#include "json/ReJson.hpp"

inline ReJson CONFIG((std::filesystem::path(__FILE__).parent_path() / "config.json").string());
