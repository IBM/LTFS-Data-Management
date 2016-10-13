#include <iostream>
#include <string>
#include <vector>

#include "FileOperation.h"
#include "InfoFiles.h"

void InfoFiles::start()

{
	int i;

	std::cout << "Info Files Request" << std::endl;

	std::cout << "pid: " << pid << std::endl;
	std::cout << "request number: " << reqNumber << std::endl;

	for(std::vector<std::string>::iterator it = fileList.begin(); it != fileList.end(); ++it) {
		std::cout << "file " << ++i << ": " << *it << std::endl;
	}

	std::cout << std::endl;
}
