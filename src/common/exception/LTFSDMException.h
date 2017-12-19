#pragma once

class LTFSDMException: public std::exception

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
        int errnum;
        Error error;
        std::string infostr;
    };

    exception_info_t exception_info;

public:
    LTFSDMException(const LTFSDMException& e) :
            exception_info(e.exception_info)
    {
    }

    LTFSDMException(exception_info_t exception_info_) :
            exception_info(exception_info_)
    {
    }

    static exception_info_t processArgs(const char *filename, int line,
            Error error)
    {
        std::stringstream info;
        exception_info_t exception_info;

        exception_info.errnum = errno;
        exception_info.error = error;
        info << filename << ":" << line;

        return exception_info;
    }

    template<typename ... Args>
    static exception_info_t processArgs(const char *filename, int line,
            Error error, Args ... args)
    {
        std::stringstream info;
        exception_info_t exception_info;

        exception_info.errnum = errno;
        exception_info.error = error;
        info << filename << ":" << line;
        addInfo(info, args ...);
        exception_info.infostr = info.str();

        return exception_info;
    }

    const Error getError() const
    {
        return exception_info.error;
    }

    const int getErrno() const
    {
        return exception_info.errnum;
    }

    const char* what() const noexcept {
        return exception_info.infostr.c_str();
    }
};

#define THROW(args ...) throw(LTFSDMException(LTFSDMException::processArgs(__FILE__, __LINE__, ##args)))
