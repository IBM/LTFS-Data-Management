#pragma once

class OpenLTFSException: public std::exception

{
private:
    static void addInfo(std::stringstream& info)
    {
    }

    template<typename T>
    static void addInfo(std::stringstream& info, T s)
    {
        info << "(" << s << ")";
    }

    template<typename T, typename ... Args>
    static void addInfo(std::stringstream& info, T s, Args ... args)
    {
        info << "(" << s << ")";
        addInfo(info, args ...);
    }

    struct exception_info_t
    {
        int error;
        std::string infostr;
    };

    exception_info_t exception_info;

public:
    OpenLTFSException(const OpenLTFSException& e) :
            exception_info(e.exception_info)
    {
    }

    OpenLTFSException(exception_info_t exception_info_) :
            exception_info(exception_info_)
    {
    }

    template<typename ... Args>
    static exception_info_t processArgs(const char *filename, int line,
            int error, Args ... args)
    {
        std::stringstream info;
        exception_info_t exception_info;

        exception_info.error = error;
        info << filename << ":" << line;
        addInfo(info, args ...);
        exception_info.infostr = info.str();

        return exception_info;
    }

    const int getError() const
    {
        return exception_info.error;
    }

    const char* what() const noexcept {
        return exception_info.infostr.c_str();
    }
};

#define THROW(args ...) throw(OpenLTFSException(OpenLTFSException::processArgs(__FILE__, __LINE__, ##args)))
