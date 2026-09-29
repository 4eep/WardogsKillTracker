#pragma once
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
class Logger { public: static Logger& Get(); void Initialize(const std::filesystem::path&); void Info(const std::string&); void Warn(const std::string&); void Error(const std::string&); void Kill(const std::string&); private: void Write(const char*,const std::string&,WORD); std::ofstream file_; std::mutex mutex_; HANDLE console_=GetStdHandle(STD_OUTPUT_HANDLE); };
