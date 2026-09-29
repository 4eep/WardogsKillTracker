#include "Application.h"
#include "Config.h"
#include "Logger.h"
#include "Utils.h"
#include <Windows.h>
#include <opencv2/core.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
static Application* g_app=nullptr;
static BOOL WINAPI Ctrl(DWORD){if(g_app)g_app->RequestStop();return TRUE;}
static void EnsureDefaultConfig(const std::filesystem::path&p){
 if(std::filesystem::exists(p))return;
 std::ofstream f(p);f<<R"({
  "capture":{"monitorIndex":0,"fps":30,"killFeedRegion":{"x":0.0,"y":0.42,"width":0.30,"height":0.14}},
  "detection":{"templatePath":"assets/player_template.png","templateThreshold":0.85,"hashDistanceThreshold":10,"debugPreview":false,"centerKillConfirmation":{"enabled":true,"templatePath":"assets/Killfeed_KillConfirmed.png","templateThreshold":0.70,"fusionWindowMs":1200,"region":{"x":0.41,"y":0.64,"width":0.18,"height":0.05}}},
  "nvidia":{"hotkeyDetection":true,"recordingDirectory":"D:\\Videos\\NVIDIA","watchRecordingDirectory":true,"recordingConfirmationTimeoutMs":5000},
  "outputDirectory":"output","timestampOffsetMs":0
})";std::cout<<"Created default config.json beside executable. Edit it before normal use.\n";
}
int wmain(int argc,wchar_t**argv){
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 auto base=std::filesystem::path(ExecutableDirectory());Logger::Get().Initialize(base/"logs/latest.log");Logger::Get().Info("Program started");
 if(argc>1&&std::wstring(argv[1])==L"--list-monitors"){Application::ListMonitors();return 0;}
 auto configPath=base/"config.json";EnsureDefaultConfig(configPath);Config c;try{c=Config::Load(configPath);}catch(const std::exception&e){Logger::Get().Error(e.what());return 1;}
 Application app(c);g_app=&app;SetConsoleCtrlHandler(Ctrl,TRUE);
 try{if(argc>1){std::wstring a=argv[1];if(a==L"--calibrate")return app.Calibrate(false);if(a==L"--create-template")return app.Calibrate(true);if(a==L"--test-image"&&argc>2)return app.TestImage(argv[2]);if(a==L"--test-folder"&&argc>2)return app.TestFolder(argv[2]);std::cerr<<"Unknown or incomplete option\n";return 1;}return app.Run();}
 catch(const cv::Exception&e){Logger::Get().Error(e.what());return 4;}catch(const std::exception&e){Logger::Get().Error(e.what());return 5;}
}
