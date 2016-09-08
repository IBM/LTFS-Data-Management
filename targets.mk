default: build

#TARGETLIB := $(BUILD_ROOT)/lib/$(firstword $(subst /, , $(patsubst $(BUILD_ROOT)/src/%,%, $(CURDIR)))).a

test:

$(TARGETLIB): $(TARGETLIB)($(call objfiles, $(TARGET_FILES)))

build:  $(call objfiles, $(SOURCE_FILES)) test $(TARGETLIB)

clean:
	rm -f $(BUILD_ROOT)/lib/* *.o $(CLEANUP_FILES)
