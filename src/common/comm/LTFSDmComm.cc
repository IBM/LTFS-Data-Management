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

void LTFSDmCommServer::listen()

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

	if (::listen(socRefFd, 5) == -1) {
		TRACE(Trace::error, errno);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}
}

void LTFSDmCommServer::accept()

{
	if ( (socAccFd = ::accept(socRefFd, NULL, NULL)) == -1) {
		TRACE(Trace::error, errno);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}
}

void LTFSDmComm::send(int fd)

{
	unsigned long MessageSize;
	unsigned long rsize;
	char *buffer;

	MessageSize = this->ByteSize();

	buffer = (char *) malloc(MessageSize + sizeof(long));
	memset(buffer, 0, MessageSize + sizeof(long));
	memcpy(buffer, &MessageSize, sizeof(long));

	if ( this->SerializeToArray(buffer + sizeof(long), MessageSize) == false ) {
		TRACE(Trace::error, buffer);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}

	rsize = write(fd, buffer, MessageSize + sizeof(long));

	if ( rsize != MessageSize + sizeof(long) ) {
		free(buffer);
		TRACE(Trace::error, rsize);
		TRACE(Trace::error, MessageSize);
		TRACE(Trace::error, sizeof(long));
		printf("error writing message to socketfd\n");
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}

	free(buffer);
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


void LTFSDmComm::recv(int fd)

{
	unsigned long MessageSize;
	unsigned long rsize;
	char *buffer;

	rsize = readx(fd, (char *) &MessageSize, sizeof(long));

	if (rsize != sizeof(long)) {
		TRACE(Trace::error, rsize);
		TRACE(Trace::error, sizeof(long));
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}

	buffer = (char *) malloc(MessageSize);
	memset(buffer, 0, MessageSize);

	rsize = readx(fd, buffer, MessageSize);

	if (rsize != MessageSize) {
		TRACE(Trace::error, rsize);
		TRACE(Trace::error, MessageSize);
		free(buffer);
		throw(OLTFSErr::OLTFS_COMM_ERROR);
	}

	this->ParseFromArray(buffer, MessageSize);
	free(buffer);
}
