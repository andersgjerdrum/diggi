#
# Process Func Makefile
#

######## SGX SDK Settings ########


SGX_SDK ?= /opt/intel/sgxsdk
SGX_MODE ?= SIM  

Crypto_Library_Name := sgx_tcrypto
LIB_CC=gcc


SGX_COMMON_CFLAGS := -m64 
SGX_LIBRARY_PATH := $(SGX_SDK)/lib64
SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
SGX_EDGER8R := $(SGX_SDK)/bin/x64/sgx_edger8r

ifeq ($(SGX_DEBUG), 1)
ifeq ($(SGX_PRERELEASE), 1)
$(error Cannot set SGX_DEBUG and SGX_PRERELEASE at the same time!!)
endif
endif

ifeq ($(SGX_DEBUG), 1)
		SGX_COMMON_CFLAGS += -g3
else
		SGX_COMMON_CFLAGS += -O3 -DRELEASE 
endif

######## App Settings ########

ifneq ($(SGX_MODE), HW)
	Urts_Library_Name := sgx_urts_sim
else
	Urts_Library_Name := sgx_urts
endif

APP_LIB_SOURCEDIR := src
APP_LIB_BUILDDIR := generated/lib

APP_LIB_SOURCES = $(shell find $(APP_LIB_SOURCEDIR)/ -type f -name '*.cpp')
SAPP_LIB_SOURCES = $(shell find $(APP_LIB_SOURCEDIR)/ -type f -name '*.s')
CAPP_LIB_SOURCES = $(shell find $(APP_LIB_SOURCEDIR)/ -type f -name '*.c')

App_Lib_Objects := $(patsubst $(APP_LIB_SOURCEDIR)/%.cpp, $(APP_LIB_BUILDDIR)/%.o, $(APP_LIB_SOURCES))
SApp_Lib_Objects := $(patsubst $(APP_LIB_SOURCEDIR)/%.s, $(APP_LIB_BUILDDIR)/%.o, $(SAPP_LIB_SOURCES))
CApp_Lib_Objects := $(patsubst $(APP_LIB_SOURCEDIR)/%.c, $(APP_LIB_BUILDDIR)/%.o, $(CAPP_LIB_SOURCES))

App_Include_Paths := -IInclude -I$(SGX_SDK)/include -Igenerated  


App_C_Flags := -c -rdynamic -DUNTRUSTED_APP -DMG_ENABLE_SSL=1 -DMG_SSL_IF=MG_SSL_IF_MBEDTLS -DMG_SSL_MBED_DUMMY_RANDOM $(SGX_COMMON_CFLAGS) -fPIC -Wno-attributes $(App_Include_Paths) 
# Three configuration modes - Debug, prerelease, release
#   Debug - Macro DEBUG enabled.
#   Prerelease - Macro NDEBUG and EDEBUG enabled.
#   Release - Macro NDEBUG enabled.
ifeq ($(SGX_DEBUG), 1)
		App_C_Flags += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(SGX_PRERELEASE), 1)
		App_C_Flags += -DNDEBUG -DEDEBUG -UDEBUG
else
		App_C_Flags += -DNDEBUG -UEDEBUG -UDEBUG
endif

# remove ltcmalloc_minimal to run valgrind
App_Cpp_Flags := -std=c++11 $(App_C_Flags)
App_Link_Flags :=  $(SGX_COMMON_CFLAGS) -L$(SGX_LIBRARY_PATH) -l$(Urts_Library_Name) -lsgx_ukey_exchange -l$(Crypto_Library_Name) -ltcmalloc_minimal -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -lpthread -ldl -Wl,--export-dynamic 

ifneq ($(SGX_MODE), HW)
	App_Link_Flags += -lsgx_uae_service_sim
else
	App_Link_Flags += -lsgx_uae_service
endif

App_Name ?= regular_executable

.PHONY: all run

all: $(App_Name)

run: all


######## App Objects ########

generated/enclave_u.c: $(SGX_EDGER8R) enclave/enclave.edl
	@mkdir -p "$(@D)"
	@cd generated && $(SGX_EDGER8R) --untrusted ../enclave/enclave.edl --search-path ../enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

generated/enclave_u.o: generated/enclave_u.c
	@mkdir -p "$(@D)"
	@$(CC) $(App_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

generated/%.o: generated/%.cpp
	@mkdir -p "$(@D)"
	@$(CXX) $(App_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

$(App_Name): generated/enclave_u.o $(App_Lib_Objects) $(CApp_Lib_Objects)  $(SApp_Lib_Objects)
	@mkdir -p "$(@D)"
	@$(CXX) $^ -o $@ $(App_Link_Flags)
	@echo "LINK =>  $@"

######## Library Objects ########

generated/lib/%.o: src/%.cpp
	@mkdir -p "$(@D)"
	@$(CXX) $(App_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"
generated/lib/%.o: src/%.s
	@mkdir -p "$(@D)"
	@$(CC) $(App_Cpp_Flags) -c $< -o $@
	@echo "CC  <=  $<"
generated/lib/%.o: src/%.c
	@mkdir -p "$(@D)"
	@$(LIB_CC) $(App_C_Flags) $< -o $@
	@echo "CC  <=  $<"
.PHONY: clean

print-%  : ; @echo $* = $($*)

rundebug:
	bash -c 'make SGX_MODE=HW SGX_DEBUG=1 && gdb --eval-command="catch throw" --eval-command=run --eval-command=quit ./regular_executable'
clean:
	@rm -f *$(App_Name) *enclave.so *.signed.so generated/*.o generated/enclave_u.* enclave/*.o enclave/enclave_t.*