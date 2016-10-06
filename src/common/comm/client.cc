#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <atomic>

#include "src/common/const/Const.h"

#include "ltfsdm.pb.h"
#include "LTFSDmComm.h"

int main(int argc, char *argv[]) {
	LTFSDmCommClient command;

	srandom(time(NULL));

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
	migreq->set_directory(false);

	printf("send:\n");

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


	std::ifstream filelist(argv[1]);
	std::string line;

	bool cont = true;

	int i;

	while (cont) {
		LTFSDmProtocol::LTFSDmMigRequestObj *migreqobj = command.mutable_migrequestobj();
		LTFSDmProtocol::LTFSDmMigRequestObj::FileName* filenames;
		for ( i = 0; (i < 7) && ((std::getline(filelist, line))); i++ ) {
			filenames = migreqobj->add_filenames();
			filenames->set_filename(line);
			printf("add: %s\n", line.c_str());
		}

		if ( i < 7 ) {
			cont = false;
			filenames = migreqobj->add_filenames();
			filenames->set_filename(""); //end
			printf("add: END\n");
		}

		try {
			command.send();
		}
		catch(...) {
			printf("send error\n");
			exit(-1);
		}

		try {
			command.recv();
		}
		catch(...) {
			printf("receive error\n");
			exit(-1);
		}

		if ( ! command.has_migrequestresp() ) {
			std::cout << "wrong message sent from the server." << std::endl;
			return 1;
		}

		const LTFSDmProtocol::LTFSDmMigRequestResp migreqresp = command.migrequestresp();

		if( migreqresp.success() == true ) {
			printf("sending success, reqNumber: %llu\n", (unsigned long long) migreqresp.reqnumber());
			if ( getpid() != migreqresp.pid() )
				printf("WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE -- WRONG RESPONSE\n");
		}
		else	 {
			printf("error sending request\n");
		}
	} // end con

	filelist.close();

	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
