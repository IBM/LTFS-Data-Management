#pragma once

class OpenLTFSException: public std::exception

{
private:
	std::stringstream info;
	std::string infostr;
	int error;

	void addInfo() {}

	template<typename T>
	void addInfo(T s)
	{
		info << "(" << s << ")";
	}

	template<typename T, typename ... Args>
	void addInfo(T s, Args ... args )
	{
		info << "(" << s << ")";
		addInfo(args ...);
	}

public:
	OpenLTFSException( const OpenLTFSException& e ) : error(e.error)
	{
		info << e.info.str();
	}

    OpenLTFSException(const char *filename, int line, int error_) : error(error_)
	{
		info << filename << ":" << line;
		infostr = info.str();
	}

	template<typename ... Args>
	OpenLTFSException(const char *filename, int line, int error_, Args ... args) : error(error_)
	{
		info << filename << ":" << line;
		addInfo(args ...);
		infostr = info.str();
	}

	const int getError() const { return error; }

	const char* what() const throw()
	{
		try {
			return  infostr.c_str();
		}
		catch( const std::exception& e ) {
			return "error providing exception information";
		}
	}
};

#define EXCEPTION(args ...) OpenLTFSException(__FILE__, __LINE__, ##args)
