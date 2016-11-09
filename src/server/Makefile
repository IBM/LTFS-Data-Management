RELPATH = ../..

LDFLAGS := -lprotobuf -lpthread -lsqlite3 -l$(CONNECTOR)connector

LIB_SRC_FILES := Server.cc
LIB_SRC_FILES += DataBase.cc
LIB_SRC_FILES += SubServer.cc
LIB_SRC_FILES += Receiver.cc
LIB_SRC_FILES += MessageParser.cc
LIB_SRC_FILES += FileOperation.cc
LIB_SRC_FILES += Migration.cc
LIB_SRC_FILES += SelRecall.cc
LIB_SRC_FILES += InfoFiles.cc
LIB_SRC_FILES += Updater.cc
LIB_SRC_FILES += Scheduler.cc

CLEANUP_FILES := ltfsdmd
BINARY := ltfsdmd
POSTTARGET :=

include $(RELPATH)/definitions.mk
