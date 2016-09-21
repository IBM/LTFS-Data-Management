#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <sstream>

#include "ltfsdm.pb.h"
#include "LTFSDmComm.h"

int main(int argc, char *argv[]) {
	LTFSDmCommClient command;

	command.connect();

	/* COMMAND WILL BE MIGRATION */
	LTFSDmProtocol::LTFSDmMigRequest *migreq = command.mutable_migrequest();
	migreq->set_key(1234);
	migreq->set_token(4321);
	migreq->set_pid(getpid());
	migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::MIGRATED);


	for ( int i=1; i<5; i++ ) {
		LTFSDmProtocol::LTFSDmMigRequest::FileName* filenames = migreq->add_filenames();
		std::stringstream st;
		st << "file" << i;
		filenames->set_filename(st.str());
	}

	command.send();

	printf("waiting for an answer\n");

	command.recv();

	const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp = command.migrequestresp();

	if( migreqresp.success() == true ) {
		printf("sending success, token: %llu\n", (unsigned long long) migreqresp.token());
		if ( getpid() != migreqresp.pid() )
			printf("WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE\n");
	}
	else {
		printf("error sending request\n");
	}

	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
