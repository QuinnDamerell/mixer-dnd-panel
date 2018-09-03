#pragma once

#include "common.h"

#include <iostream>
#include <sstream>

namespace DnDPanel
{
    enum class LoggerLevels
    {
        Info,
        Warn,
        Error
    };

    class Logger
    {
    public:
        static void Info(std::string&& msg)
        {
            Log(LoggerLevels::Info, std::move(msg));
        }

        static void Warn(std::string&& msg)
        {
            Log(LoggerLevels::Warn, std::move(msg));
        }

        static void Error(std::string&& msg)
        {
            Log(LoggerLevels::Error, std::move(msg));
        }

    private:
        static void Log(LoggerLevels level, std::string&& msg)
        {
            std::stringstream buff;
            switch (level)
            {
            case LoggerLevels::Info:
                buff << "Info: ";
                break;
            case LoggerLevels::Warn:
                buff << "Warn: ";
                break;
            case LoggerLevels::Error:
                buff << "Error: ";
                break;
            }
            buff << msg;
            buff.str();
            std::cout << buff.str();
        }
    };
}