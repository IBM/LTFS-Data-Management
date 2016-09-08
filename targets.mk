test:

$(TARGETLIB): $(TARGETLIB)($(call objfiles, $(TARGET_FILES)))

$(EXECUTABLE): $(LDLIBS)

$(BINDIR)/$(EXECUTABLE): $(EXECUTABLE)
	cp $^ $@

build:  $(call objfiles, $(SOURCE_FILES)) $(TARGETLIB) test $(BINDIR)/$(EXECUTABLE)

clean:
	rm -f $(RELPATH)/lib/* *.o $(CLEANUP_FILES) $(EXECUTABLE) $(BINDIR)/*
