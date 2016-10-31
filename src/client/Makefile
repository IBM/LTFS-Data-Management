RELPATH = ../..

LDFLAGS += -lprotobuf -l$(CONNECTOR)connector

LIB_SRC_FILES := OpenLTFSCommand.cc StartCommand.cc StopCommand.cc MigrationCommand.cc RecallCommand.cc HelpCommand.cc InfoRequestsCommand.cc InfoFilesCommand.cc StatusCommand.cc
CLEANUP_FILES := ltfsdm
BINARY := ltfsdm
POSTTARGET :=

include $(RELPATH)/definitions.mk
