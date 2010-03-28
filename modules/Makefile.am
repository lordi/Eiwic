OBJS = $(patsubst %.c,%.so,$(wildcard *.c))
SRCS = $(wildcard *.c)
INCLUDES = -I../
DEFS = @DEFS@ -DEIWIC_MODULE

CLEANFILES = $(OBJS)
DISTCLEANFILES = Makefile $(OBJS) 
EXTRA_DIST = $(wildcard *.c)

all: $(OBJS)

%.so: $(SRCS) $@
ADD_BUILD_FLAGS = if [ -e $(patsubst %.so,%.buildflags, $@) ]; then cat $(patsubst %.so,%.buildflags, $@); fi 
	@if [ -e  $(patsubst %.so,%.skip,$@) ]; then \
		echo "$@ skipped."; \
	else :;\
	if test $(patsubst %.so,%.c, $@) -nt $@ -o ! -e $@; then \
		echo "@CC@ @CFLAGS@ $(DEFS) $(INCLUDES) `$(ADD_BUILD_FLAGS)` -shared -fPIC $(patsubst %.so,%.c, $@) -o $@"; \
		@CC@ @CFLAGS@ $(DEFS) $(INCLUDES) `$(ADD_BUILD_FLAGS)`  -shared -fPIC $(patsubst %.so,%.c, $@) -o $@; \
	else :; fi; \
	fi

clean:
	rm -f $(OBJS)

install-exec-am: $(OBJS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pkglibdir)
	@list='$(OBJS)'; for p in $$list; do \
		if test -f $$p; then \
			echo "  $(INSTALL_PROGRAM) $$p $(DESTDIR)$(pkglibdir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`"; \
			$(INSTALL_PROGRAM) $$p $(DESTDIR)$(pkglibdir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
		else :; fi; \
	done

