test:

$(TARGETLIB): $(TARGETLIB)($(call objfiles, $(TARGET_FILES)))

$(EXECUTABLE): $(LDLIBS)

$(BINDIR)/$(EXECUTABLE): $(EXECUTABLE)
	cp $^ $@

build:  .depend $(call objfiles, $(SOURCE_FILES)) $(TARGETLIB) test $(BINDIR)/$(EXECUTABLE)

clean:
	rm -f $(RELPATH)/lib/* *.o $(CLEANUP_FILES) $(EXECUTABLE) $(BINDIR)/* .depend

.depend: $(SOURCE_FILES)
	$(CXX) $(CXXFLAGS) -MM $(CFLAGS) $^ > $@

-include .depend
