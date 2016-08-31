#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include <string>

#include "ltfsdm.pb.h"

#include "LTFSDmC.h"

int LTFSDmD::LTFSDmC::send(int fd)

{
	unsigned long MessageSize;
	unsigned long rsize;
	char *buffer;

	MessageSize = this->ByteSize();

	printf("Size: %ld\n", MessageSize);

	buffer = (char *) malloc(MessageSize + sizeof(long));
	memset(buffer, 0, MessageSize + sizeof(long));
	memcpy(buffer, &MessageSize, sizeof(long));

	if ( this->SerializeToArray(buffer + sizeof(long), MessageSize) == false ) {
		free(buffer);
		printf("error serializing message\n");
		return -1;
	}

	rsize = write(fd, buffer, MessageSize + sizeof(long));

	if ( rsize != MessageSize + sizeof(long) ) {
		free(buffer);
		printf("error writing message to fd\n");
		return -1;
	}

	free(buffer);

	return 0;
}

unsigned long readx(int fd, char *buffer, unsigned long size)

{
    unsigned long bread = 0;
    unsigned long rsize;
    while (bread < size)
    {
        rsize = read(fd, buffer + bread, size - bread);
        if (rsize < 1 )
            return -1;

        bread += rsize;
    }

	return bread;
}


int LTFSDmD::LTFSDmC::recv(int fd)

{
	unsigned long MessageSize;
	unsigned long rsize;
	char *buffer;
	int rc = 0;

	rsize = readx(fd, (char *) &MessageSize, sizeof(long));

	if (rsize != sizeof(long)) {
		printf("error reading size from fd, requested %lu, response: %ld, errno: %d\n", sizeof(long), rsize, errno);
		return -1;
	}

	buffer = (char *) malloc(MessageSize);
	memset(buffer, 0, MessageSize);

	rsize = readx(fd, buffer, MessageSize);

	if (rsize != MessageSize) {
		printf("error writing message from fd\n");
		free(buffer);
		return -1;
	}

	this->ParseFromArray(buffer, MessageSize);
	free(buffer);

	return rc;

}
