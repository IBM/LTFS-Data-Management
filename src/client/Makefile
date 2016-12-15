RELPATH = ../..

LDFLAGS += -lprotobuf -l$(CONNECTOR)connector

LIB_SRC_FILES := OpenLTFSCommand.cc
LIB_SRC_FILES += StartCommand.cc
LIB_SRC_FILES += StopCommand.cc
LIB_SRC_FILES += AddCommand.cc
LIB_SRC_FILES += MigrationCommand.cc
LIB_SRC_FILES += RecallCommand.cc
LIB_SRC_FILES += HelpCommand.cc
LIB_SRC_FILES += InfoRequestsCommand.cc
LIB_SRC_FILES += InfoFilesCommand.cc
LIB_SRC_FILES += StatusCommand.cc
CLEANUP_FILES := ltfsdm
BINARY := ltfsdm
POSTTARGET :=

include $(RELPATH)/definitions.mk
