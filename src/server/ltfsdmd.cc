#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/resource.h>
#include <errno.h>

#include <string>
#include <set>
#include <vector>
#include <condition_variable>
#include <thread>

#include "src/common/util/util.h"
#include "src/common/messages/Message.h"
#include "src/common/tracing/Trace.h"
#include "src/common/errors/errors.h"
#include "src/common/const/Const.h"

#include "src/connector/Connector.h"
#include "src/server/SubServer.h"
#include "src/server/Server.h"


int main(int argc, char **argv)

{
 	Server ltfsdmd;
	int err = Error::LTFSDM_OK;
	bool detach = true;
	int opt;
	sigset_t set;
	bool dbUseMemory = false;
	Trace::traceLevel tl = Trace::error;

	Connector *connector = NULL;

	opterr = 0;

	while (( opt = getopt(argc, argv, "fmd:")) != -1 ) {
		switch( opt ) {
			case 'f':
				detach = false;
				break;
			case 'm':
				dbUseMemory = true;
				break;
			case 'd':
				try {
					tl = (Trace::traceLevel) std::stoi(optarg);
				}
				catch(...) {
					tl = Trace::error;
				}
				break;
			default:
				std::cerr << messages[LTFSDMC0013E] << std::endl;
				err = Error::LTFSDM_GENERAL_ERROR;
				goto end;
		}
	}

	sigemptyset(&set);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGPIPE);
	sigaddset(&set, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	try {
		LTFSDM::init();
	}
	catch(...) {
		err = Error::LTFSDM_GENERAL_ERROR;
		goto end;
	}

	traceObject.setTrclevel(tl);

	TRACE(Trace::little, getpid());

	try {
		ltfsdmd.initialize(dbUseMemory);
		if ( detach )
			ltfsdmd.daemonize();

		connector = new Connector(true);
		ltfsdmd.run(connector, set);
	}
	catch ( int initerr ) {
		err = initerr;
		goto end;
	}

end:
	if ( connector )
		delete(connector);
	return (int) err;
}
