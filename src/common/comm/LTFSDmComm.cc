#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include <string>

#include "src/common/errors/errors.h"
#include "src/common/tracing/Trace.h"

#include "ltfsdm.pb.h"
#include "LTFSDmComm.h"

void LTFSDmCommClient::connect()

{
	struct sockaddr_un addr;

	if ( (socRefFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		TRACE(Trace::error, errno);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, Const::SOCKET_FILE.c_str(), sizeof(addr.sun_path)-1);

	if (::connect(socRefFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        TRACE(Trace::error, errno);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}

	GOOGLE_PROTOBUF_VERIFY_VERSION;

}

void LTFSDmCommServer::connect()

{
	struct sockaddr_un addr;

	if ( (socRefFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        TRACE(Trace::error, errno);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, Const::SOCKET_FILE.c_str(), sizeof(addr.sun_path)-1);

	unlink(Const::SOCKET_FILE.c_str());

	if (bind(socRefFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        TRACE(Trace::error, errno);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}

	if (listen(socRefFd, 5) == -1) {
        TRACE(Trace::error, errno);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}
}

void LTFSDmCommServer::accept()

{
	if ( (socAccFd = ::accept(socRefFd, NULL, NULL)) == -1)
        TRACE(Trace::error, errno);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
}

int LTFSDmComm::send(int fd)

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
		printf("error writing message to socketfd\n");
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


int LTFSDmComm::recv(int fd)

{
	unsigned long MessageSize;
	unsigned long rsize;
	char *buffer;
	int rc = 0;

	rsize = readx(fd, (char *) &MessageSize, sizeof(long));

	if (rsize != sizeof(long)) {
		printf("error reading size from socketfd, requested %lu, response: %ld, errno: %d\n", sizeof(long), rsize, errno);
		return -1;
	}

	buffer = (char *) malloc(MessageSize);
	memset(buffer, 0, MessageSize);

	rsize = readx(fd, buffer, MessageSize);

	if (rsize != MessageSize) {
		printf("error writing message from socketfd\n");
		free(buffer);
		return -1;
	}

	this->ParseFromArray(buffer, MessageSize);
	free(buffer);

	return rc;

}
