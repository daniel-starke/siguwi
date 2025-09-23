APPS = siguwi
COMMA = ,

siguwi_version = 1.2.0
siguwi_version_nums = $(subst .,$(COMMA),$(siguwi_version)),0
siguwi_version_date = 2025-09-23
siguwi_author = Daniel Starke

CPPMETAFLAGS = '-DSIGUWI_VERSION="$(siguwi_version) ($(siguwi_version_date))"' '-DSIGUWI_VERSION_NUMS=$(siguwi_version_nums)' '-DSIGUWI_AUTHOR="$(siguwi_author)"'
CPPFLAGS += $(CPPMETAFLAGS)

siguwi_obj = \
	argpus \
	getopt \
	htableo \
	siguwi-config \
	siguwi-ini \
	siguwi-main \
	siguwi-process \
	siguwi-registry \
	siguwi-translate \
	rcwstr \
	ustrbuf \
	utf8 \
	vector \

siguwi_lib = \
	libcomctl32 \
	libcredui \
	libcrypt32 \
	libole32 \
	libpsapi \
	librpcrt4 \
	libshlwapi \
	libwinscard \

all: $(DSTDIR) $(APPS:%=$(DSTDIR)/%$(BINEXT))

.PHONY: $(DSTDIR)
$(DSTDIR):
	mkdir -p $(DSTDIR)

.PHONY: clean
clean:
	$(RM) -r $(DSTDIR)/*$(LIBEXT)
	$(RM) -r $(DSTDIR)/*$(BINEXT)
	$(RM) -r $(DSTDIR)/*.ii
	$(RM) -r $(DSTDIR)/*.manifest
	$(RM) -r $(DSTDIR)/*.map
	$(RM) -r $(DSTDIR)/*$(OBJEXT)

$(DSTDIR)/siguwi$(BINEXT): $(addprefix $(DSTDIR)/,$(addsuffix $(OBJEXT),$(siguwi_obj))) | $(DSTDIR)/resource$(OBJEXT)
	$(AR) rs $(DSTDIR)/siguwi.a $+
	$(LD) $(LDFLAGS) -Wl,-Map,$(DSTDIR)/siguwi.map -o $@ $(DSTDIR)/siguwi.a $(siguwi_lib:lib%=-l%) $(DSTDIR)/resource$(OBJEXT)

$(DSTDIR)/%$(OBJEXT): $(SRCDIR)/%$(CEXT)
	mkdir -p "$(dir $@)"
	$(CC) $(CWFLAGS) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

$(DSTDIR)/%$(OBJEXT): $(SRCDIR)/%$(CXXEXT)
	mkdir -p "$(dir $@)"
	$(CXX) $(CWFLAGS) $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

$(DSTDIR)/%$(OBJEXT): $(SRCDIR)/%$(RCEXT) |$(SRCDIR)/siguwi.exe.manifest
	mkdir -p "$(dir $@)"
	$(CXX) -x c++ -E $(CPPMETAFLAGS) -o $@.ii $<
	cp $| $(dir $@)
	$(WINDRES) $@.ii $@

# dependencies
$(DSTDIR)/argpus$(OBJEXT): \
	$(SRCDIR)/argp.h \
	$(SRCDIR)/argp.i \
	$(SRCDIR)/argpus.h \
	$(SRCDIR)/getopt.h \
	$(SRCDIR)/target.h
$(DSTDIR)/getopt$(OBJEXT): \
	$(SRCDIR)/argp.h \
	$(SRCDIR)/argpus.h \
	$(SRCDIR)/getopt.h
$(DSTDIR)/htableo$(OBJEXT): \
	$(SRCDIR)/htableo.h
$(DSTDIR)/rcwstr$(OBJEXT): \
	$(SRCDIR)/rcwstr.h
$(DSTDIR)/resource$(OBJEXT): \
	$(SRCDIR)/resource.h
$(SRCDIR)/siguwi.h: \
	$(SRCDIR)/argp.h \
	$(SRCDIR)/argpus.h \
	$(SRCDIR)/getopt.h \
	$(SRCDIR)/htableo.h \
	$(SRCDIR)/rcwstr.h \
	$(SRCDIR)/resource.h \
	$(SRCDIR)/target.h \
	$(SRCDIR)/ustrbuf.h \
	$(SRCDIR)/utf8.h \
	$(SRCDIR)/vector.h
$(DSTDIR)/siguwi-config$(OBJEXT): \
	$(SRCDIR)/siguwi.h
$(DSTDIR)/siguwi-ini$(OBJEXT): \
	$(SRCDIR)/siguwi.h
$(DSTDIR)/siguwi-main$(OBJEXT): \
	$(SRCDIR)/siguwi.h
$(DSTDIR)/siguwi-process$(OBJEXT): \
	$(SRCDIR)/siguwi.h
$(DSTDIR)/siguwi-registry$(OBJEXT): \
	$(SRCDIR)/siguwi.h
$(DSTDIR)/siguwi-translate$(OBJEXT): \
	$(SRCDIR)/siguwi.h
$(DSTDIR)/ustrbuf$(OBJEXT): \
	$(SRCDIR)/strbuf.i \
	$(SRCDIR)/target.h \
	$(SRCDIR)/ustrbuf.h
$(DSTDIR)/utf8$(OBJEXT): \
	$(SRCDIR)/utf8.h
$(DSTDIR)/vector$(OBJEXT): \
	$(SRCDIR)/vector.h
