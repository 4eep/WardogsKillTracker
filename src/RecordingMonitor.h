#pragma once
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <thread>
enum class RecordingState { Idle,Starting,Recording,Stopping };
class RecordingMonitor { public: using HotkeyCallback=std::function<void()>;using FileCallback=std::function<void(const std::filesystem::path&)>;RecordingMonitor(std::filesystem::path,bool,HotkeyCallback,FileCallback);~RecordingMonitor();bool InstallHook();void RunMessageLoop(std::stop_token);void Stop(); private:static LRESULT CALLBACK HookProc(int,WPARAM,LPARAM);void Watch(std::stop_token);static RecordingMonitor* instance_;std::filesystem::path directory_;bool watch_;HotkeyCallback hotkey_;FileCallback file_;HHOOK hook_{};std::jthread watcher_;std::chrono::steady_clock::time_point lastHotkey_{};std::atomic_bool f9Down_=false; };
