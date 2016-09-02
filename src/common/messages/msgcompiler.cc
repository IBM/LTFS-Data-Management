#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <fstream>
#include <vector>

using namespace std;

typedef struct {
	string msgname;
	string msgtxt;
} message_t;

int main(int argc, char **argv)

{
	ifstream infile(argv[1]);
	ofstream outfile(argv[2]);
	string first, second, line;
	vector<message_t> messages;
	vector<message_t>::iterator it;

	while (std::getline(infile, line))
	{
		// remove leading white space
		line = line.erase(0, line.find_first_not_of(' '));

		// if line starts with '"' append the message
		if ( line[0] == '"' ) {
			messages.back().msgtxt += '\n';
			messages.back().msgtxt += "                      ";
			messages.back().msgtxt += string("+std::string(") + line + ")";
		}
		// new message
		else {
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
