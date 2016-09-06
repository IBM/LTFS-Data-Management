export objfiles = $(patsubst %.cc,%.o, $(1))

msgdefs.h: $(call objfiles, msgcompiler.cc) messages.cfg
	$(CXX) $(CPPFLAGS) -o msgcompiler msgcompiler.o
	./msgcompiler messages.cfg msgdefs.h


$(call objfiles, messages.cc ): msgdefs.h

msgs: $(call objfiles, messages.cc )

msgtest: $(call objfiles, msgtest.cc )
	$(CXX) $(CPPFLAGS) -o msgtest msgtest.o
	./msgtest

build: msgs msgtest

clean:
	rm -f *.o msgcompiler msgtest msgdefs.h
