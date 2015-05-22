# common POSIX parts. Makefile.{Linux,OpenBSD,FreeBSD,NetBSD,Darwin} all inherit from this
# the values in here correspond to the

# --- config ---

# extend the list of $(CC), $(YACC), ... with extra standard programs variables
# so that we can tolerate the POSIX incompatible parts of Windows by override.
WHICH:=which
NULL:=/dev/null
MV:=mv
CAT:=cat
RM:=rm -rf
CP:=cp
MKDIR:=mkdir -p
LN:=ln -f


ifndef DLLEXT #hack: this guards against overwriting DLLEXT:=dll in the MinGW path
  DLLEXT:=so
endif

CFLAGS+=-Wall -Werror
CFLAGS+=-std=c99 #arrrrgh, this should be the default

#CFLAGS+=-DDEBUG

# strange, make comes with .LIBPATTERNS yet doesn't come with rules for actually making .so files
%.$(DLLEXT):
	$(CC) $(LDFLAGS) $^  -o $@  $(foreach L,$(LIBS),-l$L)


#svm.plugin: $(OS)/$(ARCH)/svm.plugin

# --- testing ---

STATA := $(shell which stata)
ifndef STATA
  # quick hack: be just a little bit case insensitive
  STATA := $(shell which Stata)
endif

ifneq ($(OS),Darwin) #dirty, encapsulation-breaking hack: avoid the warning about redefining printdeps (make wasn't realllly designed with OOPey inheritence in mind)
printdeps:
	readelf -d $^
endif

# --- cleaning ---

.PHONY: clean-posix
clean-posix:
	-$(RM) *.so


clean: clean-posix
