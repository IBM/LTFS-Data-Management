#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <atomic>

#include "src/common/const/Const.h"

#include "ltfsdm.pb.h"
#include "LTFSDmComm.h"

int main(int argc, char *argv[]) {
	LTFSDmCommServer command;
	unsigned long pid;

	try {
		command.listen();
	}
	catch(...) {
		exit(-1);
	}

	while (1) {
		try {
			command.accept();
		}
		catch(...) {
			exit(-1);
		}

		try {
			command.recv();
		}
		catch(...) {
			printf("receive error\n");
			exit(-1);
		}

		printf("============================================================\n");

		// MIGRATION
		if ( command.has_migrequest() ) {
			printf("Migration Request\n");
			const LTFSDmProtocol::LTFSDmMigRequest migreq = command.migrequest();
			printf("key: %llu\n", (unsigned long long) migreq.key());
			printf("reqNumber: %llu\n", (unsigned long long) migreq.reqnumber());
			pid = migreq.pid();
			printf("client pid: %lu\n", pid);
			switch (migreq.state()) {
				case LTFSDmProtocol::LTFSDmMigRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmProtocol::LTFSDmMigRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			// RESPONSE

			LTFSDmProtocol::LTFSDmMigRequestResp *migreqresp = command.mutable_migrequestresp();

			migreqresp->set_success(true);
			migreqresp->set_reqnumber(time(NULL));
			migreqresp->set_pid(pid);

			try {
				command.send();
			}
			catch(...) {
				printf("send error\n");
				exit(-1);

			}

			bool cont = true;

			while (cont) {
				try {
					command.recv();
				}
				catch(...) {
					printf("receive error\n");
					exit(-1);
				}

				if ( ! command.has_sendobjects() ) {
					std::cout << "wrong message sent from the client." << std::endl;
					return 1;
				}

				const LTFSDmProtocol::LTFSDmSendObjects sendobjects = command.sendobjects();

				//std::cout <<  sendobjects.filenames_size() << "new list of %d number of objects received." << std::endl;

				for (int j = 0; j < sendobjects.filenames_size(); j++) {
					const LTFSDmProtocol::LTFSDmSendObjects::FileName& filename = sendobjects.filenames(j);
					if ( filename.filename().compare("") != 0 ) {
						printf("file name: %s\n", filename.filename().c_str());
					}
					else {
						cont = false;
						printf("END");
					}
				}

				printf("=============== filelist ===============\n");

				// RESPONSE

				LTFSDmProtocol::LTFSDmMigRequestResp *migreqresp = command.mutable_migrequestresp();

				migreqresp->set_success(true);
				migreqresp->set_reqnumber(time(NULL));
				migreqresp->set_pid(pid);

				try {
					command.send();
				}
				catch(...) {
					printf("send error\n");
					exit(-1);

				}

				printf("=============== response ===============\n");
			}
		}

		// SELECTIVE RECALL
		else if ( command.has_selrecrequest() ) {
			printf("Selective Recall Request\n");
			const LTFSDmProtocol::LTFSDmSelRecRequest selrecreq = command.selrecrequest();
			printf("key: %llu\n", (unsigned long long) selrecreq.key());
			printf("key: %llu\n", (unsigned long long) selrecreq.reqnumber());
			switch (selrecreq.state()) {
				case LTFSDmProtocol::LTFSDmSelRecRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmProtocol::LTFSDmSelRecRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			for (int j = 0; j < selrecreq.filenames_size(); j++) {
				const LTFSDmProtocol::LTFSDmSelRecRequest::FileName& filename = selrecreq.filenames(j);
				printf("file name: %s\n", filename.filename().c_str());
			}
		}
		else
			printf("unkown command\n");

		printf("============================================================\n");
	}

	return 0;
}
