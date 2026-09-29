#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
std::wstring ExecutableDirectory();
std::string FormatDuration(std::chrono::milliseconds value);
std::string LocalTimestamp(const char* format);
int HammingDistance(uint64_t a, uint64_t b);
std::string Narrow(const std::wstring& value);
