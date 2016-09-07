default: build

test:

build:  $(call objfiles, $(SOURCE_FILES)) test

clean:
	rm -f *.o $(FILES_CLEANUP)
