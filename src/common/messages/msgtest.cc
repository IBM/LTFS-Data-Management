#include <stdio.h>
#include <string.h>
#include <string>

#include "messages.h"

int main(int argc, char **argv)

{
	MSG_OUT(OLTFS000001, 4);
	MSG_OUT(OLTFS000002, "text", 6);
	MSG_OUT(OLTFS000003);
}
