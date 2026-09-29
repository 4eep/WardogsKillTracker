#include "Logger.h"
#include "Utils.h"
#include <iostream>
Logger& Logger::Get(){static Logger x;return x;}
void Logger::Initialize(const std::filesystem::path&p){std::filesystem::create_directories(p.parent_path());file_.open(p,std::ios::app);}
void Logger::Write(const char*l,const std::string&m,WORD c){std::scoped_lock lock(mutex_);auto line="["+LocalTimestamp("%H:%M:%S")+"] ["+l+"] "+m;SetConsoleTextAttribute(console_,c);std::cout<<"\r"<<line<<"                                      \n";SetConsoleTextAttribute(console_,7);if(file_){file_<<line<<'\n';file_.flush();}}
void Logger::Info(const std::string&m){Write("INFO",m,7);} void Logger::Warn(const std::string&m){Write("WARN",m,14);} void Logger::Error(const std::string&m){Write("ERROR",m,12);} void Logger::Kill(const std::string&m){Write("KILL",m,10);}
