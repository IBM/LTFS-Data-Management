#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <sstream>

#include "ltfsdm.pb.h"
#include "LTFSDmC.h"

char *socket_path = (char *) "/tmp/ltfsdmd";

int main(int argc, char *argv[]) {
	struct sockaddr_un addr;
	char buf[100];
	int fd,rc;

	if (argc > 1) socket_path=argv[1];

	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		exit(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("connect error");
		exit(-1);
	}

	//GOOGLE_PROTOBUF_VERIFY_VERSION;

	LTFSDmD::LTFSDmC command;

	/* COMMAND WILL BE MIGRATION */
	LTFSDmD::LTFSDmMigRequest *migreq = command.mutable_migrequest();
	migreq->set_key(1234);
	migreq->set_token(4321);
	migreq->set_pid(getpid());
	migreq->set_state(LTFSDmD::LTFSDmMigRequest::MIGRATED);


	for ( int i=1; i<5; i++ ) {
		LTFSDmD::LTFSDmMigRequest::FileName* filenames = migreq->add_filenames();
		std::stringstream st;
		st << "file" << i;
		filenames->set_filename(st.str());
	}

	command.send(fd);

	printf("waiting for an answer\n");

	command.recv(fd);

	const LTFSDmD::LTFSDmMigRequestResp migreqresp = command.migrequestresp();

	if( migreqresp.success() == true ) {
		printf("sending success, token: %lu\n", migreqresp.token());
		if ( getpid() != migreqresp.pid() )
			printf("WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE\n");
	}
	else {
		printf("error sending request\n");
	}



	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
