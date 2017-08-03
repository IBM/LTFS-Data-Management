#pragma once

#include <string.h>
#include <atomic>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <mutex>

#include "boost/format.hpp"

#include "msgdefs.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

class Message
{
private:
    std::mutex mtx;
    std::ofstream messagefile;
public:
    enum LogType
    {
        STDOUT, LOGFILE
    };
private:
    std::atomic<Message::LogType> logType;

    inline void processParms(boost::format *fmter)
    {
    }
    template<typename T>
    void processParms(boost::format *fmter, T s)
    {
        *fmter % s;
    }
    template<typename T, typename ... Args>
    void processParms(boost::format *fmter, T s, Args ... args)
    {
        *fmter % s;
        processParms(fmter, args ...);
    }

    void writeOut(std::string msgstr);
    void writeLog(std::string msgstr);

    template<typename ... Args>
    void msgOut(msg_id msg, char *filename, int linenr, Args ... args)

    {
        std::string fmtstr = msgname[msg] + "(%d): "
                + messages[msg];
        boost::format fmter(fmtstr);
        fmter.exceptions(boost::io::all_error_bits);

        try {
            fmter % linenr;
            processParms(&fmter, args ...);
            writeOut(fmter.str());
        } catch (const std::exception& e) {
            std::cerr << messages[LTFSDMX0005E] << " (" << msgname[msg] << ":"
                    << filename << ":" << linenr << ")" << std::endl;
        }
    }
    template<typename ... Args>
    void msgLog(msg_id msg, char *filename, int linenr, Args ... args)
    {
        std::string fmtstr = msgname[msg] + "(%d): "
                + messages[msg];
        boost::format fmter(fmtstr);
        fmter.exceptions(boost::io::all_error_bits);

        try {
            fmter % linenr;
            processParms(&fmter, args ...);
            writeLog(fmter.str());
        } catch (const std::exception& e) {
            std::cerr << messages[LTFSDMX0005E] << " (" << msgname[msg] << ":"
                    << filename << ":" << linenr << ")" << std::endl;
        }
    }

public:
    Message() :
            logType(Message::STDOUT)
    {
    }
    ~Message();

    void init();

    void setLogType(Message::LogType type)
    {
        logType = type;
    }

    template<typename ... Args>
    void message(msg_id msg, char *filename, int linenr, Args ... args)
    {
        if (logType == Message::STDOUT)
            msgOut(msg, filename, linenr, args ...);
        else
            msgLog(msg, filename, linenr, args ...);
    }

    template<typename ... Args>
    void info(msg_id msg, char *filename, int linenr, Args ... args)
    {
        boost::format fmter(messages[msg]);
        fmter.exceptions(boost::io::all_error_bits);

        try {
            processParms(&fmter, args ...);
            writeOut(fmter.str());
        } catch (const std::exception& e) {
            std::cerr << messages[LTFSDMX0005E] << " (" << filename << ":"
                    << linenr << ")" << std::endl;
            exit((int) Error::LTFSDM_GENERAL_ERROR);
        }
    }
};

extern Message messageObject;

#define MSG(msg, args ...) messageObject.message(msg, (char *) __FILE__, __LINE__, ##args)
#define INFO(msg, args ...) messageObject.info(msg, (char *) __FILE__, __LINE__, ##args)
