COMMONDIRS := src/common/msgcompiler
COMMONDIRS += src/common/util
COMMONDIRS += src/common/messages
COMMONDIRS += src/common/tracing
COMMONDIRS += src/common/comm
COMMONDIRS += src/common/configuration

CLIENTDIRS := src/client

SERVERDIRS := src/server/LEControl/ltfsadminlib
SERVERDIRS += src/server/LEControl
SERVERDIRS += src/server

CONNECDIRS := src/connector/fuse
ifneq ($(wildcard /usr/include/xfs/dmapi.h),)
    CONNECDIRS += src/connector/dmapi
endif
