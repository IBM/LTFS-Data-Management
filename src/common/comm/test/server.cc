#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

#include "ltfsdm.pb.h"
#include "LTFSDmC.h"

char *socket_path = (char *) "/tmp/ltfsdmd";

int main(int argc, char *argv[]) {
	struct sockaddr_un addr;
	char buf[100];
	int fd,cl,rc;
	unsigned long pid;

	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		exit(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);

	unlink(socket_path);

	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("bind error");
		exit(-1);
	}

	if (listen(fd, 5) == -1) {
		perror("listen error");
		exit(-1);
	}

	LTFSDmD::LTFSDmC command;

	while (1) {
		if ( (cl = accept(fd, NULL, NULL)) == -1) {
			perror("accept error");
			continue;
		}

		// command.ParseFromFileDescriptor(cl);
		command.recv(cl);

		printf("============================================================\n");

		// MIGRATION
		if ( command.has_migrequest() ) {
			printf("Migration Request\n");
			const LTFSDmD::LTFSDmMigRequest migreq = command.migrequest();
			printf("key: %lu\n", migreq.key());
			printf("token: %lu\n", migreq.token());
			pid = migreq.pid();
			printf("client pid: %lu\n", pid);
			switch (migreq.state()) {
				case LTFSDmD::LTFSDmMigRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmD::LTFSDmMigRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			for (int j = 0; j < migreq.filenames_size(); j++) {
				const LTFSDmD::LTFSDmMigRequest::FileName& filename = migreq.filenames(j);
				printf("file name: %s\n", filename.filename().c_str());
			}

			for (int i=0; i<1; i++) {
				printf("... wait\n");
				sleep(1);
			}

			// RESPONSE

			LTFSDmD::LTFSDmMigRequestResp *migreqresp = command.mutable_migrequestresp();

			migreqresp->set_success(true);
			migreqresp->set_token(time(NULL));
			migreqresp->set_pid(pid);

			command.send(cl);
		}

		// SELECTIVE RECALL
		else if ( command.has_selrecrequest() ) {
			printf("Selective Recall Request\n");
			const LTFSDmD::LTFSDmSelRecRequest selrecreq = command.selrecrequest();
			printf("key: %lu\n", selrecreq.key());
			printf("key: %lu\n", selrecreq.token());
			switch (selrecreq.state()) {
				case LTFSDmD::LTFSDmSelRecRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmD::LTFSDmSelRecRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			for (int j = 0; j < selrecreq.filenames_size(); j++) {
				const LTFSDmD::LTFSDmSelRecRequest::FileName& filename = selrecreq.filenames(j);
				printf("file name: %s\n", filename.filename().c_str());
			}
		}

		// TRANSPARENT RECALL
		else if ( command.has_selrecrequest() ) {
			printf("Transparent Recall Request\n");
			const LTFSDmD::LTFSDmTransRecRequest transrecreq = command.transrecrequest();
			printf("key: %lu\n", transrecreq.key());
			printf("key: %lu\n", transrecreq.token());
			switch (transrecreq.state()) {
				case LTFSDmD::LTFSDmTransRecRequest::MIGRATED:
					printf("files to be migrated\n");
					break;
				case LTFSDmD::LTFSDmTransRecRequest::PREMIGRATED:
					printf("files to be premigrated\n");
					break;
				default:
					printf("unkown target state\n");
			}

			printf("file with inode %lu will be recalled transparently\n", transrecreq.inum());
		}
		else
			printf("unkown command\n");

		printf("============================================================\n");

	}


	return 0;
}
