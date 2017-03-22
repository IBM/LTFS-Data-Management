RELPATH = ../..

LDFLAGS += -lprotobuf -lconnector -latomic

ARC_SRC_FILES := OpenLTFSCommand.cc
ARC_SRC_FILES += StartCommand.cc
ARC_SRC_FILES += StopCommand.cc
ARC_SRC_FILES += AddCommand.cc
ARC_SRC_FILES += MigrationCommand.cc
ARC_SRC_FILES += RecallCommand.cc
ARC_SRC_FILES += HelpCommand.cc
ARC_SRC_FILES += InfoRequestsCommand.cc
ARC_SRC_FILES += InfoJobsCommand.cc
ARC_SRC_FILES += InfoFilesCommand.cc
ARC_SRC_FILES += InfoFsCommand.cc
ARC_SRC_FILES += StatusCommand.cc
CLEANUP_FILES := ltfsdm
BINARY := ltfsdm
POSTTARGET :=

include $(RELPATH)/definitions.mk
