#include <stdio.h>
#include <string.h>
#include <string>

#include "messages.h"

int main(int argc, char **argv)

{
	MSG_OUT(OLTFSS0001X, 4);
	MSG_OUT(OLTFSC0001X, "text", 6);
	MSG_OUT(OLTFSS0002X);
	MSG_INFO(OLTFSC0002X);
}
