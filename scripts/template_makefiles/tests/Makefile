#
# Test Makefile
#
SGX_SDK ?= /opt/intel/sgxsdk

TESTCC=g++ -std=c++11
TESTCFLAGS=-c -g3 -Wall -ggdb  -fno-optimize-sibling-calls -fno-omit-frame-pointer -Iposix/intercept.h -IInclude -I$(SGX_SDK)/include -DTEST_DEBUG -DDEBUG=1 -DMG_ENABLE_SSL=1 -DMG_SSL_IF=MG_SSL_IF_MBEDTLS -DMG_SSL_MBED_DUMMY_RANDOM -Ifuncs/sql_server_func -Ifuncs/tpcc_client_func -Ifuncs/network_server_func

include scripts/template_makefiles/syscall/syscall.mk
 
Crypto_Library_Name := sgx_tcrypto

SGX_LIBRARY_PATH := $(SGX_SDK)/lib64
  
TESTLDFLAGS=  -L$(SGX_LIBRARY_PATH) -l$(Crypto_Library_Name) -g3 -lgtest -lgtest_main \
	-Wl,-lpthread -Llib



TESTSRC=$(wildcard unit_tests/*.cpp)
LIBSRC= $(shell find src/ -type f -name '*.cpp')
CLIBSRC= $(shell find src/ -type f -name '*.c')
SLIBSRC= $(shell find src/ -type f -name '*.s')


ALIBSRC= $(shell find funcs/ -type f -name '*.cpp' ! -name "*func.cpp")
CALIBSRC= $(shell find funcs/ -type f -name '*.c')

TESTOBJECTS= $(patsubst unit_tests/%.cpp, unit_tests/testobjects/%.o, $(TESTSRC))

SRCPATH =  src
FUNCSRCPATH = funcs
BUILDDIR = unit_tests/libobjects
FUNCLIBOBJECTS = $(patsubst $(FUNCSRCPATH)/%.cpp, $(BUILDDIR)/%.o, $(ALIBSRC))
CFUNCLIBOBJECTS = $(patsubst $(FUNCSRCPATH)/%.c, $(BUILDDIR)/%.o, $(CALIBSRC))

LIBOBJECTS = $(patsubst $(SRCPATH)/%.cpp, $(BUILDDIR)/%.o, $(LIBSRC))
CLIBOBJECTS = $(patsubst $(SRCPATH)/%.c, $(BUILDDIR)/%.o, $(CLIBSRC))
SLIBOBJECTS = $(patsubst $(SRCPATH)/%.c, $(BUILDDIR)/%.o, $(SLIBSRC))

TESTEXECUTABLE=runtest

LIB_CC=gcc

$(TESTEXECUTABLE):  $(LIBOBJECTS) $(TESTOBJECTS)  $(CLIBOBJECTS) $(SLIBOBJECTS) $(FUNCLIBOBJECTS) $(CFUNCLIBOBJECTS)
	@$(TESTCC) $(TESTLINKSYSCALLS) $(LIBOBJECTS)  $(TESTOBJECTS) $(CLIBOBJECTS) $(SLIBOBJECTS) $(FUNCLIBOBJECTS) $(CFUNCLIBOBJECTS) $(TESTLDFLAGS) -o $@
	@echo "CXX  <=  $<"		

# func source code
unit_tests/libobjects/sql_server_func/sqlite3.o: funcs/sql_server_func/sqlite3.c
	@mkdir -p "$(@D)"
	@$(LIB_CC) $(TESTCFLAGS) -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_WAL -DSQLITE_DISABLE_LFS $< -o $@
	@echo "CC  <=  $<"

unit_tests/libobjects/network_server_func/mongoose.o: funcs/network_server_func/mongoose.c
	@mkdir -p "$(@D)"
	@$(LIB_CC) $(TESTCFLAGS) -DMG_ENABLE_SSL=1 -DMG_SSL_IF=MG_SSL_IF_MBEDTLS -Llib -lmbedtls_SGX_u $< -o $@
	@echo "CC  <=  $<"
#libos source code
unit_tests/libobjects/%.o: src/%.c
	@mkdir -p "$(@D)"
	@$(LIB_CC) $(TESTCFLAGS) $< -o $@
	@echo "CC  <=  $<"
unit_tests/libobjects/%.o: src/%.s
	@mkdir -p "$(@D)"
	@$(LIB_CC) $(TESTCFLAGS) $< -o $@
	@echo "CC  <=  $<"
unit_tests/libobjects/%.o: funcs/%.c
	@mkdir -p "$(@D)"
	@$(LIB_CC) $(TESTCFLAGS) $< -o $@
	@echo "CC  <=  $<"
unit_tests/libobjects/%.o: src/%.cpp 
	@mkdir -p "$(@D)"
	@$(TESTCC) $(TESTCFLAGS) $< -o $@
	@echo "CXX  <=  $<"
unit_tests/libobjects/%.o: funcs/%.cpp 
	@mkdir -p "$(@D)"
	@$(TESTCC) $(TESTCFLAGS) $< -o $@
	@echo "CXX  <=  $<"
#unit test code
unit_tests/testobjects/%.o: unit_tests/%.cpp 
	@mkdir -p "$(@D)"
	@$(TESTCC) $(TESTCFLAGS) $< -o $@
	@echo "CXX  <=  $<"

clean:
	@rm -f runtest 
