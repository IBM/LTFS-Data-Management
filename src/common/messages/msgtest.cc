#include <string.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

#include "Message.h"

using namespace std;

int main(int argc, char **argv)

{
	stringstream buffer;
	string messageOut;
	string savedOut;
	string line;

	cout.rdbuf(buffer.rdbuf());

	MSG(LTFSDMS0001X, 4);
	MSG(LTFSDMC0001X, "text", 6);
	MSG(LTFSDMS0002X);
	INFO(LTFSDMC0002X);

	messageOut = buffer.str();

	ifstream compare("msgtest.out");

	while (std::getline(compare, line))
	{
		savedOut += line.append("\n");
	}

	int rc;
	if ( (rc = messageOut.compare(savedOut)) != 0 ) {
		cerr << "size of messageOut: " << messageOut.size() << endl;
		cerr << "size of savedOut: " << savedOut.size() << endl;
		cerr << "return value: " << rc << endl;
		cerr << "compare failed, commands output:" << endl;
		cerr << ">>" << messageOut << "<<" << endl;
		cerr << "saved output:" << endl;
		cerr << ">>" << savedOut << "<<" << endl;
		return -1;
	}
	else {
		return 0;
	}
}
