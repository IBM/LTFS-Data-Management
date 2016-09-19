#include <string>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

typedef struct {
	string msgname;
	string msgtxt;
} message_t;

int main(int argc, char **argv)

{
	string first;
	string second;
	string line;
	vector<message_t> messages;
	vector<message_t>::iterator it;
	ifstream infile;
	ofstream outfile;

	if ( argc != 3 ) {
		cout << "usage: " << argv[0] << "< message text file name> <compiled message header>" << endl;
		return -1;
	}

	try {
		infile.open(argv[1]);
	}
	catch(...) {
		cout << "unable to open input file " << argv[1] << "." << endl;
	}

	try {
		outfile.open(argv[2]);
	}
	catch(...) {
		cout << "unable to open outout file " << argv[2] << "." << endl;
	}

	while (std::getline(infile, line))
	{
		// remove leading white spaces and tabs
		line = line.erase(0, line.find_first_not_of(" \t"));
		// if line is empty or a comment continue with next line
		if ( line [0] == 0 || line [0] == '#' ) {
			continue;
		}
		// if line starts with '"' append the message
		else if ( line[0] == '"' ) {
			messages.back().msgtxt += '\n';
			messages.back().msgtxt += "                      ";
			messages.back().msgtxt += string("+std::string(") + line + ")";
		}
		// new message
		else {
			if ( line[0] != 'O' ) {
				cout << "Line:" << endl;
				cout << ">>" << line << "<< " << endl;
				cout << "does not look correctly formatted." << endl;
				cout << "Message compilation stopped." << endl;
				return -1;
			}
			first = line.substr(0, line.find(' '));
			second = line.substr(line.find('"'), string::npos - 1);
			messages.push_back((message_t) {first, "std::string(" + second + ")"});
		}
	}

	infile.close();

	// create the header file
	outfile << "#ifndef _MESSAGE_DEFINITION_H" << endl;
	outfile << "#define _MESSAGE_DEFINITION_H" << endl;
	outfile << endl;
	outfile << "typedef std::string message_t[];" << endl;
	outfile << "typedef std::string msgname_t[];" << endl;
	outfile << endl;
	outfile << "enum msg_id {" << endl;
	for (it = messages.begin(); it != messages.end(); ++it) {
		if ( it + 1 != messages.end() )
			outfile << "    " << it->msgname << "," << endl;
		else
			outfile << "    " << it->msgname<< endl;
	}
	outfile << "};" << endl;
	outfile << endl;

	outfile << "const message_t messages = {" << endl;
	for (it = messages.begin(); it != messages.end(); ++it) {
		if ( it + 1 != messages.end() )
			outfile << "    " << "/* " << it->msgname << " */  " << it->msgtxt << "," << endl;
		else
			outfile << "    " << "/* " << it->msgname << " */  " << it->msgtxt << endl;
	}
	outfile << "};" << endl;
	outfile << endl;

	outfile << "const msgname_t msgname = {" << endl;
	for (it = messages.begin(); it != messages.end(); ++it) {
		if ( it + 1 != messages.end() )
			outfile << "    " << "\"" << it->msgname << "\"," << endl;
		else
			outfile << "    " << "\"" << it->msgname << "\"" << endl;
	}
	outfile << "};" << endl;
	outfile << endl;

	outfile << "#endif /* _MESSAGE_DEFINITION_H */" << endl;

	outfile.close();

	return 0;
}
