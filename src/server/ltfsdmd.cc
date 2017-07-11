#include "ServerIncludes.h"

int main(int argc, char **argv)

{
 	Server ltfsdmd;
	int err = Error::LTFSDM_OK;
	bool detach = true;
	int opt;
	sigset_t set;
	bool dbUseMemory = false;
	Trace::traceLevel tl = Trace::error;

	MSG(LTFSDMX0029I, COMMIT_COUNT, BUILD_HASH);

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

	TRACE(Trace::always, getpid());

	try {
		ltfsdmd.initialize(dbUseMemory);

		if ( detach )
			ltfsdmd.daemonize();

		inventory = new OpenLTFSInventory();
		connector = new Connector(true);
		ltfsdmd.run(connector, set);
	}
	catch ( int initerr ) {
		err = initerr;
		goto end;
	}

end:
	if ( inventory )
		delete(inventory);
	if ( connector )
		delete(connector);
	return (int) err;
}
