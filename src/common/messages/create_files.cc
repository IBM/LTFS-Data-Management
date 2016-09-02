#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;


int main(int argc, char **argv)

{
	ifstream infile(argv[1]);
	string a, b, line;
	while (std::getline(infile, line))
	{
		istringstream iss(line);

		//printf("%s\n", line.c_str());

		string delim = " ";
		string first = line.substr(0, line.find(delim));
		string second = line.erase(0, first.size());
		int num = second.find_first_not_of(' ');
		string third = second.erase(0, num);
		printf(">>%s<< >>%s<<\n", first.c_str(), third.c_str());
	}



	/*
	while (infile >> a >> b)
	{
		printf(">>%s<< >>%s<<\n", a.c_str(), b.c_str());
	}
	*/

	return 0;
}
