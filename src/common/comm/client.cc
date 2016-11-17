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
	bool flist = false;
	bool dname = false;
	std::string fileList;
	std::string dirName;
	int opt;
	std::stringstream parmList;

	opterr = 0;

	while (( opt = getopt(argc, argv, "f:d:")) != -1 ) {
		switch( opt ) {
			case 'f':
				flist = true;
				fileList = std::string(optarg);
				break;
			case 'd':
				dname = true;
				dirName = std::string(optarg);
			default:
				std::cout << "usage: "<< argv[0] << "file ...|-f <file list>|-d <dir name>" << std::endl;
				return -1;
		}
	}

	if(flist && dname) {
		std::cout << "usage: "<< argv[0] << "file ...|-f <file list>|-d <dir name>" << std::endl;
		return -1;
	}

	if ( !flist && !dname ) {
		for ( int i=1; i<argc; i++ ) {
			parmList << argv[i] << std::endl;
			std::cout << "adding: " << argv[i] << std::endl;
		}
	}
	else if ( dname ) {
		parmList << dirName << std::endl;
	}

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

	std::istream *input;
	std::ifstream filelist;

	if ( flist ) {
		filelist.open(fileList);
		input =  dynamic_cast<std::istream*>(&filelist);
	}
	else {
		input = dynamic_cast<std::istream*>(&parmList);
	}

	//	std::ifstream filelist(argv[1]);
	std::string line;

	bool cont = true;

	int i;

	while (cont) {
		LTFSDmProtocol::LTFSDmSendObjects *sendobjects = command.mutable_sendobjects();
		LTFSDmProtocol::LTFSDmSendObjects::FileName* filenames;
		for ( i = 0; (i < 7) && ((std::getline(*input, line))); i++ ) {
			filenames = sendobjects->add_filenames();
			filenames->set_filename(line);
			printf("add: %s\n", line.c_str());
		}

		if ( i < 7 ) {
			cont = false;
			filenames = sendobjects->add_filenames();
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

	if ( flist )
		filelist.close();

	google::protobuf::ShutdownProtobufLibrary();

	return 0;
}
