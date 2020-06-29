#
# Enclave func Makefile
#

######## SGX SDK Settings ########


SGX_SDK ?= /opt/intel/sgxsdk
SGX_MODE ?= SIM


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
		SGX_COMMON_CFLAGS += -O2 -DRELEASE
endif

######## Enclave Settings ########

ifneq ($(SGX_MODE), HW)
	Trts_Library_Name := sgx_trts_sim
	Service_Library_Name := sgx_tservice_sim
else
	Trts_Library_Name := sgx_trts
	Service_Library_Name := sgx_tservice
endif
Crypto_Library_Name := sgx_tcrypto


ENC_LIB_SOURCEDIR = src
ENC_LIB_BUILDDIR = enclave/lib

ENC_LIB_SOURCES += $(shell find  $(ENC_LIB_SOURCEDIR)/ -type f -name '*.cpp')
CENC_LIB_SOURCES += $(shell find  $(ENC_LIB_SOURCEDIR)/ -type f -name '*.c')
SENC_LIB_SOURCES += $(shell find  $(ENC_LIB_SOURCEDIR)/ -type f -name '*.s')

Enclave_Lib_Objects += $(patsubst $(ENC_LIB_SOURCEDIR)/%.cpp, $(ENC_LIB_BUILDDIR)/%.o, $(ENC_LIB_SOURCES))
CEnclave_Lib_Objects += $(patsubst $(ENC_LIB_SOURCEDIR)/%.c, $(ENC_LIB_BUILDDIR)/%.o, $(CENC_LIB_SOURCES))
SEnclave_Lib_Objects += $(patsubst $(ENC_LIB_SOURCEDIR)/%.s, $(ENC_LIB_BUILDDIR)/%.o, $(SENC_LIB_SOURCES))

FUNCSRCPATH = funcs/$(FUNC_NAME)
  

ALIBSRC= $(shell find $(FUNCSRCPATH) -type f -name '*.cpp')
CALIBSRC= $(shell find $(FUNCSRCPATH)  -type f -name '*.c')

FUNCLIBOBJECTS = $(patsubst $(FUNCSRCPATH)/%.cpp, $(ENC_LIB_BUILDDIR)/$(FUNC_NAME)/%.o, $(ALIBSRC))
CFUNCLIBOBJECTS = $(patsubst $(FUNCSRCPATH)/%.c, $(ENC_LIB_BUILDDIR)/$(FUNC_NAME)/%.o, $(CALIBSRC))


Enclave_Include_Paths += -I$(SGX_SDK)/include -I$(SGX_SDK)/include/libcxx -IInclude/posix/tlibc -I$(SGX_SDK)/include/stlport -IInclude -Ienclave -IInclude/posix -Ifuncs/$(FUNC_NAME)/

Enclave_C_Flags += -DDIGGI_ENCLAVE $(SGX_COMMON_CFLAGS) -DNOSTDLIBS -DMG_ENABLE_SSL=1 -DMG_SSL_IF=MG_SSL_IF_MBEDTLS -DMG_SSL_MBED_DUMMY_RANDOM -fno-builtin -nostdinc -fvisibility=hidden -fpie -fstack-protector $(Enclave_Include_Paths)

CC_BELOW_4_9 := $(shell expr "`$(CC) -dumpversion`" \< "4.9")
ifeq ($(CC_BELOW_4_9), 1)
	Enclave_C_Flags += -ffunction-sections -fdata-sections -fstack-protector
else
	Enclave_C_Flags += -ffunction-sections -fdata-sections -fstack-protector-strong
endif

Enclave_Cpp_Flags += -std=c++11  $(Enclave_C_Flags) -nostdinc++
Enclave_Link_Flags += $(SGX_COMMON_CFLAGS) \
	-Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_LIBRARY_PATH)  \
	-Wl,--whole-archive -l$(Trts_Library_Name) -Wl,--no-whole-archive \
	-Wl,--start-group -lsgx_tstdc -lsgx_tcxx -lsgx_tkey_exchange -l$(Crypto_Library_Name) -l$(Service_Library_Name) -Wl,--end-group \
	-Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
	-Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
	-Wl,--defsym,__ImageBase=0 \
	-Wl,--version-script=enclave/enclave.lds


#linker thinks that .a files are intermediate
.PRECIOUS: %.a

Enclave_Name ?= regular_enclave.so
Signed_Enclave_Name ?= regular_enclave.signed.so

Enclave_Config_File := enclave/enclave.config.xml

ifeq ($(SGX_MODE), HW)
ifneq ($(SGX_DEBUG), 1)
ifneq ($(SGX_PRERELEASE), 1)
Build_Mode = HW_RELEASE
endif
endif
endif


.PHONY: all run

ifeq ($(Build_Mode), HW_RELEASE)
all: $(App_Name) $(Enclave_Name)
	@echo "The project has been built in release hardware mode."
	@echo "Please sign the $(Enclave_Name) first with your signing key before you run the $(App_Name) to launch and access the enclave."
	@echo "To sign the enclave use the command:"
	@echo "   $(SGX_ENCLAVE_SIGNER) sign -key <your key> -enclave $(Enclave_Name) -out <$(Signed_Enclave_Name)> -config $(Enclave_Config_File)"
	@echo "You can also sign the enclave using an external signing tool. See User's Guide for more details."
	@echo "To build the project in simulation mode set SGX_MODE=SIM. To build the project in prerelease mode set SGX_PRERELEASE=1 and SGX_MODE=HW."
else
all: $(App_Name) $(Signed_Enclave_Name)
endif

run: all
ifneq ($(Build_Mode), HW_RELEASE)
	@$(CURDIR)/$(App_Name)
	@echo "RUN  =>  $(App_Name) [$(SGX_MODE)|$(SGX_ARCH), OK]"
endif

######## Enclave Objects ########

enclave/enclave_t.c: $(SGX_EDGER8R) enclave/enclave.edl
	@cd enclave && $(SGX_EDGER8R) --trusted ../enclave/enclave.edl --search-path ../enclave --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

enclave/enclave_t.o: enclave/enclave_t.c
	@$(CC) $(Enclave_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"



$(Enclave_Name): enclave/enclave_t.o $(Enclave_Lib_Objects) $(CEnclave_Lib_Objects) $(SEnclave_Lib_Objects) $(FUNCLIBOBJECTS) $(CFUNCLIBOBJECTS) $(EXTERNALOBJECTS)
	@$(CXX) $^ -o $@ $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

$(Signed_Enclave_Name): $(Enclave_Name)
	@$(SGX_ENCLAVE_SIGNER) sign -key enclave/enclave_private.pem -enclave $(Enclave_Name) -out $@ -config $(Enclave_Config_File)
	@echo "SIGN =>  $@"

######## Library Objects ########

enclave/lib/%.o: src/%.cpp
	@mkdir -p "$(@D)"
	@$(CXX) $(Enclave_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"
enclave/lib/%.o: src/%.c
	@mkdir -p "$(@D)"
	@$(CC) $(Enclave_C_Flags) -c $< -o $@
	@echo "CC  <=  $<"
enclave/lib/%.o: src/%.s
	@mkdir -p "$(@D)"
	@$(CC) $(Enclave_C_Flags) -c $< -o $@
	@echo "CC  <=  $<"

######## func Objects ########

enclave/lib/$(FUNC_NAME)/%.o: funcs/$(FUNC_NAME)/%.cpp
	@mkdir -p "$(@D)"
	@$(CXX) $(Enclave_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

enclave/lib/$(FUNC_NAME)/%.o: funcs/$(FUNC_NAME)/%.c
	@mkdir -p "$(@D)"
	@$(CC) $(Enclave_C_Flags) -c $< -o $@
	@echo "CC  <=  $<"
.PHONY: clean

print-%  : ; @echo $* = $($*)

rundebug:
	bash -c 'make SGX_MODE=HW SGX_DEBUG=1 && gdb --eval-command="catch throw" --eval-command=run --eval-command=quit ./regular_executable'
clean:
	@rm -f *$(Signed_Enclave_Name) $(Enclave_Name) *enclave.so *.signed.so harness/*.o harness/enclave_u.*  enclave/*.o enclave/enclave_t.*