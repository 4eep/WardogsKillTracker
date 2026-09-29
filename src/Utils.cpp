#include "Utils.h"
#include <Windows.h>
#include <array>
#include <bit>
#include <iomanip>
#include <sstream>
std::wstring ExecutableDirectory(){ std::array<wchar_t,32768> b{}; auto n=GetModuleFileNameW(nullptr,b.data(),(DWORD)b.size()); return std::filesystem::path(std::wstring(b.data(),n)).parent_path().wstring(); }
std::string FormatDuration(std::chrono::milliseconds v){ auto n=std::max<int64_t>(0,v.count()); auto h=n/3600000; n%=3600000; auto m=n/60000; n%=60000; auto s=n/1000; auto ms=n%1000; std::ostringstream o; o<<std::setfill('0')<<std::setw(2)<<h<<':'<<std::setw(2)<<m<<':'<<std::setw(2)<<s<<':'<<std::setw(2)<<(ms/10); return o.str(); }
std::string LocalTimestamp(const char* f){ auto now=std::chrono::system_clock::now(); auto t=std::chrono::system_clock::to_time_t(now); tm x{}; localtime_s(&x,&t); std::ostringstream o; o<<std::put_time(&x,f); return o.str(); }
int HammingDistance(uint64_t a,uint64_t b){ return std::popcount(a^b); }
std::string Narrow(const std::wstring& v){ if(v.empty())return{}; int n=WideCharToMultiByte(CP_UTF8,0,v.data(),(int)v.size(),nullptr,0,nullptr,nullptr); std::string s(n,0); WideCharToMultiByte(CP_UTF8,0,v.data(),(int)v.size(),s.data(),n,nullptr,nullptr); return s; }
