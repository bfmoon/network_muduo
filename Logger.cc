#include"Logger.h"
#include"Timestamp.h"


#include<iostream>

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level)
{
    logLevel_=level;
}

void Logger::log(std::string msg)
{
    switch(logLevel_)
    {
    case INFO:
        std::cout << "[INFO] ";
        break;

    case ERROR:
        std::cout << "[WARN] ";
        break;

    case FATAL:
        std::cout << "[ERROR] ";
        break;

    case DEBUG:
        std::cout << "[DEBUG] ";
        break;

    default:
        break;
    }

    //打印日志信息msg
    std::cout<<Timestamp::now().toString()<<msg<<std::endl;
}