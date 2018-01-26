#include <libgen.h>

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

typedef struct
{
	std::string msgname;
	std::string msgtxt;
} message_t;

const std::string IDENTIFIER = "LTFSDM";

std::string escape(std::string input)

{
    std::string result;

    for ( char c : input ) {
        switch(c) {
            case '<':
                result += "\\<";
                break;
            case '>':
                result += "\\>";
                break;
            default:
                result += c;
        }
    }

    return result;
}

int main(int argc, char **argv)

{
	std::string first;
	std::string second;
	std::string line;
	std::vector<message_t> messages;
	std::vector<message_t> documentation;
	std::vector<message_t>::iterator it;
	std::ifstream infile;
	std::ofstream outfile;
	std::ofstream doxfile;

	if (argc != 4) {
		std::cout << "usage: " << argv[0]
				<< " <message text file name> <compiled message header> <compiled documentation file>"
				<< std::endl;
		return -1;
	}

	try {
		infile.open(argv[1]);
	} catch (const std::exception& e) {
		std::cout << "unable to open input file " << argv[1] << "."
				<< std::endl;
	}

	try {
		outfile.open(argv[2]);
	} catch (const std::exception& e) {
		std::cout << "unable to open output file " << argv[2] << "."
				<< std::endl;
	}

    try {
        doxfile.open(argv[3]);
    } catch (const std::exception& e) {
        std::cout << "unable to open output file " << argv[3] << "."
                << std::endl;
    }

	while (std::getline(infile, line)) {
		// remove leading white spaces and tabs
		line = line.erase(0, line.find_first_not_of(" \t"));
		// if line is empty or a comment continue with next line
		if (line[0] == 0 || line[0] == '#') {
			continue;
		}
		// if line starts with '"' append the message
		else if (line[0] == '"') {
			messages.back().msgtxt += '\n';
			messages.back().msgtxt += "                       ";
			messages.back().msgtxt += "+std::string(" + line + ")";
			documentation.back().msgtxt += "<BR>";
			documentation.back().msgtxt += escape(line);
		}
		// new message
		else {
			if (line.compare(0, IDENTIFIER.size(), IDENTIFIER)) {
				std::cout << "Line:" << std::endl;
				std::cout << ">>" << line << "<< " << std::endl;
				std::cout << "does not look correctly formatted." << std::endl;
				std::cout << "Message compilation stopped." << std::endl;
				return -1;
			}
			first = line.substr(0, line.find(' '));
			second = line.substr(line.find('"'), std::string::npos - 1);
			messages.push_back(
					(message_t ) { first, "std::string(" + second + ")" });
			documentation.push_back(
			        (message_t ) { first, escape(second)});
		}
	}

	infile.close();

	// create the header file
	outfile << "#pragma once" << std::endl;
	outfile << std::endl;
	outfile << "typedef std::string ltfsdm_message_t[];" << std::endl;
	outfile << "typedef std::string ltfsdm_msgname_t[];" << std::endl;
	outfile << std::endl;
	outfile << "enum ltfsdm_msg_id {" << std::endl;
	for (it = messages.begin(); it != messages.end(); ++it) {
		if (it + 1 != messages.end())
			outfile << "    " << it->msgname << "," << std::endl;
		else
			outfile << "    " << it->msgname << std::endl;
	}
	outfile << "};" << std::endl;
	outfile << std::endl;

	outfile << "const ltfsdm_message_t ltfsdm_messages = {" << std::endl;
	for (it = messages.begin(); it != messages.end(); ++it) {
		if (it + 1 != messages.end())
			outfile << "    " << "/* " << it->msgname << " */  " << it->msgtxt
					<< "," << std::endl;
		else
			outfile << "    " << "/* " << it->msgname << " */  " << it->msgtxt
					<< std::endl;
	}
	outfile << "};" << std::endl;
	outfile << std::endl;

	outfile << "const ltfsdm_msgname_t ltfsdm_msgname = {" << std::endl;
	for (it = messages.begin(); it != messages.end(); ++it) {
		if (it + 1 != messages.end())
			outfile << "    " << "\"" << it->msgname << "\"," << std::endl;
		else
			outfile << "    " << "\"" << it->msgname << "\"" << std::endl;
	}
	outfile << "};" << std::endl;
	outfile << std::endl;

	outfile.close();

	for (it = documentation.begin(); it != documentation.end(); ++it ) {
        doxfile << "ALIASES += " << it->msgname << "=" << it->msgtxt << std::endl;
	}

	doxfile.close();

	return 0;
}
