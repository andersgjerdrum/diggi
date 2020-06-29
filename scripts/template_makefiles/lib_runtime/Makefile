#
# Library Func Makefile
# add -DRELEASE if release mode
SGX_SDK ?= /opt/intel/sgxsdk

include scripts/template_makefiles/syscall/syscall.mk
 
LIB_CXX+=g++ -std=c++11
CFLAGS+=-c -Wno-unused -O3 -m64 -fPIC -Bsymbolic -fvisibility=hidden -Wall -IInclude -I$(SGX_SDK)/include -DMG_ENABLE_SSL=1 -DMG_SSL_IF=MG_SSL_IF_MBEDTLS -Ifuncs/$(FUNC_NAME)/
#remove tcmalloc to valgrind
LDFLAGS+= -shared -fvisibility=hidden -ltcmalloc_minimal -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
HARNESSSRC+=$(wildcard lib_harness/*.cpp)
LIBSRC+=$(shell find src/ -type f -name '*.cpp')
CLIBSRC+= $(shell find src/ -type f -name '*.c')
SLIBSRC+= $(shell find src/ -type f -name '*.s')

FUNCSRCPATH = funcs/$(FUNC_NAME)

Crypto_Library_Name := sgx_tcrypto

ifeq ($(SGX_DEBUG), 1)
		CFLAGS += -g3
else
		CFLAGS += -O3 -DRELEASE
endif

ALIBSRC= $(shell find $(FUNCSRCPATH) -type f -name '*.cpp')
CALIBSRC= $(shell find $(FUNCSRCPATH)  -type f -name '*.c')

FUNCLIBOBJECTS = $(patsubst $(FUNCSRCPATH)/%.cpp, $(BUILDDIR)/$(FUNC_NAME)/%.o, $(ALIBSRC))
CFUNCLIBOBJECTS = $(patsubst $(FUNCSRCPATH)/%.c, $(BUILDDIR)/$(FUNC_NAME)/%.o, $(CALIBSRC))

HARNESSOBJECTS+=$(HARNESSSRC:.cpp=.o)
LIB_CC=gcc

SRCPATH =  src
BUILDDIR = lib_harness
LIBOBJECTS = $(patsubst $(SRCPATH)/%.cpp, $(BUILDDIR)/%.o, $(LIBSRC))
CLIBOBJECTS = $(patsubst $(SRCPATH)/%.c, $(BUILDDIR)/%.o, $(CLIBSRC))
SLIBOBJECTS = $(patsubst $(SRCPATH)/%.c, $(BUILDDIR)/%.o, $(SLIBSRC))



FUNC_LIB ?= func.so

$(FUNC_LIB):  $(LIBOBJECTS) $(HARNESSOBJECTS) $(CLIBOBJECTS) $(SLIBOBJECTS) $(FUNCLIBOBJECTS) $(CFUNCLIBOBJECTS) $(EXTERNALOBJECTS) 
	@$(LIB_CXX) $(LIBOBJECTS) $(HARNESSOBJECTS) $(CLIBOBJECTS) $(SLIBOBJECTS) $(FUNCLIBOBJECTS) $(CFUNCLIBOBJECTS) $(EXTERNALOBJECTS) $(TESTLINKSYSCALLS) $(LDFLAGS) -o $@
	@echo "LINK =>  $@"


lib_harness/$(FUNC_NAME)/%.o: funcs/$(FUNC_NAME)/%.c
	@mkdir -p "$(@D)"
	@$(LIB_CC) $(CFLAGS) $< -o $@
	@echo "CC  <=  $<"

lib_harness/$(FUNC_NAME)/%.o: funcs/$(FUNC_NAME)/%.cpp 
	@mkdir -p "$(@D)"
	@$(LIB_CXX) $(CFLAGS) $< -o $@
	@echo "CXX  <=  $<"

lib_harness/%.o: src/%.c
	@mkdir -p "$(@D)"
	@$(LIB_CC) $(CFLAGS) $< -o $@
	@echo "CC  <=  $<"

lib_harness/%.o: src/%.cpp 
	@mkdir -p "$(@D)"
	@$(LIB_CXX) $(CFLAGS) $< -o $@
	@echo "CXX  <=  $<"

lib_harness/%.o: lib_harness/%.cpp
	@$(LIB_CXX) $(CFLAGS) $< -o $@
	@echo "CXX  <=  $<"



clean:
	@rm -f *$(FUNC_LIB)