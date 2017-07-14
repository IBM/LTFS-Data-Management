#pragma once

class OpenLTFSException: public std::exception

{
private:
	const char *filename;
	int line;
	int error;
	std::stringstream traceInfo;

	void addInfo() {}

	template<typename T>
	void addInfo(T s)
	{
		traceInfo << "(" << s << ")";
	}

	template<typename T, typename ... Args>
	void addInfo(T s, Args ... args )
	{
		traceInfo << "(" << s << ")";
		addInfo(args ...);
	}

public:
    OpenLTFSException( const OpenLTFSException& e ) : filename(e.filename), line(e.line) {}

    OpenLTFSException(const char *filename_, int line_, int error_) : filename(filename_), line(line_), error(error_) {}

	template<typename ... Args>
	OpenLTFSException(const char *filename_, int line_, int error_, Args ... args) : filename(filename_), line(line_), error(error_)
	{
		addInfo(args ...);
	}

	const int getError() const { return error; }

	virtual const char* what() const throw()
	{
		try {
			std::stringstream info;
			if (traceInfo.str().size() > 0)
				info << filename << ":" << line << traceInfo.str();
			else
				info << filename << ":" << line;
			return info.str().c_str();
		}
		catch( const std::exception& e ) {
			return "error providing exception information";
		}
	}
};

#define EXCEPTION(args ...) OpenLTFSException(__FILE__, __LINE__, ##args)
