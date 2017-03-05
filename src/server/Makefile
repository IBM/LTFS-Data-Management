RELPATH = ../..

LDFLAGS := -lprotobuf -lpthread -lsqlite3 -lconnector -latomic

ARC_SRC_FILES := Server.cc
ARC_SRC_FILES += DataBase.cc
ARC_SRC_FILES += SubServer.cc
ARC_SRC_FILES += Receiver.cc
ARC_SRC_FILES += MessageParser.cc
ARC_SRC_FILES += FileOperation.cc
ARC_SRC_FILES += Migration.cc
ARC_SRC_FILES += SelRecall.cc
ARC_SRC_FILES += TransRecall.cc
ARC_SRC_FILES += Scheduler.cc

CLEANUP_FILES := ltfsdmd
BINARY := ltfsdmd
POSTTARGET :=

include $(RELPATH)/definitions.mk
