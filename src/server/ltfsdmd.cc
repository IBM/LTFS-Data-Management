#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include <string>

#include "src/common/messages/messages.h"
#include "src/common/tracing/trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"


void lockServer()

{
	int lockfd;

	if ( (lockfd = open(Const::SERVER_LOCK_FILE.c_str(), O_RDWR | O_CREAT)) == -1 ) {
		MSG_OUT(OLTFSS0001E);
		TRACE(0, Const::SERVER_LOCK_FILE);
		TRACE(0, errno);
		throw(OLTFSErr::OLTFS_GENERAL_ERROR);
	}

	if ( flock(lockfd, LOCK_EX | LOCK_NB) == -1 ) {
		TRACE(0, errno);
		if ( errno == EWOULDBLOCK ) {
			MSG_OUT(OLTFSS0002I);
			throw(OLTFSErr::OLTFS_OK);
		}
		else {
			MSG_OUT(OLTFSS0001E);
			TRACE(0, errno);
			throw(OLTFSErr::OLTFS_GENERAL_ERROR);
		}
	}
}

void initialize()

{
	lockServer();
}

void daemonize()

{
	pid_t pid, sid;

	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		throw(OLTFSErr::OLTFS_OK);
	}

	sid = setsid();
	if (sid < 0) {
		throw(OLTFSErr::OLTFS_GENERAL_ERROR);
	}

	TRACE(0, "Server started");
	TRACE(0, getpid());

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* redirect stdout to log file */
// 	dup2(log, 0);
// 	dup2(log, 1);
// 	dup2(log, 2);
// 	close(log);

	/* seting line buffers*/
// 	setlinebuf(stdout);
// 	setlinebuf(stderr);
}

int main(int argc, char **argv)

{
	OLTFSErr err = OLTFSErr::OLTFS_OK;

	try {
		initialize();
		daemonize();
	}
	catch ( OLTFSErr initerr ) {
		err = initerr;
		goto end;
	}

	while (true) {};

end:
	return (int) err;
}
