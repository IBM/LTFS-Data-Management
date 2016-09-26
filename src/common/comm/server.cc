#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include <string>

#include "src/common/const/Const.h"

#include "ltfsdm.pb.h"
#include "LTFSDmComm.h"

char *socket_path = (char *) "/tmp/ltfsdmd";

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

		// command.ParseFromFileDescriptor(cl);
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

			for (int j = 0; j < migreq.filenames_size(); j++) {
				const LTFSDmProtocol::LTFSDmMigRequest::FileName& filename = migreq.filenames(j);
				printf("file name: %s\n", filename.filename().c_str());
			}

			for (int i=0; i<1; i++) {
				printf("... wait\n");
				sleep(1);
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

		// TRANSPARENT RECALL
		else if ( command.has_selrecrequest() ) {
			printf("Transparent Recall Request\n");
			const LTFSDmProtocol::LTFSDmTransRecRequest transrecreq = command.transrecrequest();
			printf("key: %llu\n", (unsigned long long) transrecreq.key());
			printf("key: %llu\n", (unsigned long long) transrecreq.reqnumber());
			switch (transrecreq.state()) {
				case LTFSDmProtocol::LTFSDmTransRecRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmProtocol::LTFSDmTransRecRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			printf("file with inode %llu will be recalled transparently\n", (unsigned long long) transrecreq.inum());
		}
		else
			printf("unkown command\n");

		printf("============================================================\n");

	}


	return 0;
}
