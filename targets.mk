default: build

COMPONENT := $(firstword $(subst /, , $(patsubst $(BUILD_ROOT)/src/%,%, $(CURDIR))))

test:

addarch: $(call objfiles, $(ARCH_FILES))
	ar rv $(BUILD_ROOT)/lib/$(COMPONENT).a $^

build:  $(call objfiles, $(SOURCE_FILES)) test addarch

clean:
	rm -f *.o $(FILES_CLEANUP)
