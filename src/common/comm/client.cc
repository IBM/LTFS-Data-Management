#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <sstream>

#include "src/common/const/Const.h"

#include "ltfsdm.pb.h"
#include "LTFSDmComm.h"

int main(int argc, char *argv[]) {
	LTFSDmCommClient command;

	try {
		command.connect();
	}
	catch(...) {
		exit(-1);
	}

	/* COMMAND WILL BE MIGRATION */
	LTFSDmProtocol::LTFSDmMigRequest *migreq = command.mutable_migrequest();
	migreq->set_key(1234);
	migreq->set_reqnumber(4321);
	migreq->set_pid(getpid());
	migreq->set_state(LTFSDmProtocol::LTFSDmMigRequest::MIGRATED);


	for ( int i=1; i<5; i++ ) {
		LTFSDmProtocol::LTFSDmMigRequest::FileName* filenames = migreq->add_filenames();
		std::stringstream st;
		st << "file" << i;
		filenames->set_filename(st.str());
	}

	try {
		command.send();
	}
	catch(...) {
		printf("send error\n");
		exit(-1);
	}

	printf("waiting for an answer\n");

	try {
		command.recv();
	}
	catch(...) {
		printf("receive error\n");
		exit(-1);
	}

	const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp = command.migrequestresp();

	if( migreqresp.success() == true ) {
		printf("sending success, reqNumber: %llu\n", (unsigned long long) migreqresp.reqnumber());
		if ( getpid() != migreqresp.pid() )
			printf("WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE\n");
	}
	else {
		printf("error sending request\n");
	}

	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
