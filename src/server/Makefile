RELPATH = ../..

LDFLAGS := -lprotobuf -lpthread -lsqlite3 -lconnector -latomic -lxml2

ARC_SRC_FILES := SQLStatements.cc
ARC_SRC_FILES += Server.cc
ARC_SRC_FILES += DataBase.cc
ARC_SRC_FILES += SubServer.cc
ARC_SRC_FILES += Receiver.cc
ARC_SRC_FILES += MessageParser.cc
ARC_SRC_FILES += FileOperation.cc
ARC_SRC_FILES += Migration.cc
ARC_SRC_FILES += SelRecall.cc
ARC_SRC_FILES += TransRecall.cc
ARC_SRC_FILES += Scheduler.cc
ARC_SRC_FILES += Status.cc
ARC_SRC_FILES += OpenLTFSDrive.cc
ARC_SRC_FILES += OpenLTFSCartridge.cc
ARC_SRC_FILES += OpenLTFSInventory.cc
ARC_SRC_FILES += OpenLTFSPool.cc

CLEANUP_FILES := ltfsdmd
BINARY := ltfsdmd
POSTTARGET :=

include $(RELPATH)/definitions.mk
