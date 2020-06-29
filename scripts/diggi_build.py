import os, subprocess, json, sys
import threading
output_dir_name = "output"

#diggi_base_executable_conf
proc_func_makefile = "scripts/template_makefiles/proc_runtime/Makefile"
lib_func_makefile = "scripts/template_makefiles/lib_runtime/Makefile"

enclave_func_makefile = "scripts/template_makefiles/enclave_runtime/Makefile"
tests_makefile =  "scripts/template_makefiles/tests/Makefile"

#INFO: 
# - Setting to SGX_PRERELEASE=1 causes assert to not be evaluated

# - change SGX_MODE to SIM if debugging is difficult
# - Want to support -j 16 in the future
# - Combine with valgrind by using SIM mode
# - If sim fails because of missing libsgx_urts_sim.so, 
#   add the directory where its located manualy to  /etc/ld.so.conf
SGX_DEBUG=" SGX_DEBUG=1 "
SGX_RELEASE=" SGX_PRERELEASE=1 "
SGX_HARDWARE= " SGX_MODE="
def create_output_dir(configfilename):
    os.system("mkdir "  + output_dir_name)
    if not os.path.isfile(configfilename):
        raise Exception("No "+ configfilename +" file found for func func")
    os.system("cp "+configfilename+" "+ output_dir_name + "/")

def clean_output_dir():
    os.system("rm -r "+ output_dir_name)

def setup_build_env(configfilename, preclean):
    clean_build_dir()
    if preclean:
        clean_output_dir()
    create_output_dir(configfilename)

def get_diggi_configuration(filename):
    with open (filename, "r") as myfile:
        return json.load(myfile)

def run_tests():
    
    os.system("cp " + tests_makefile + " Makefile.test")
    os.system("make clean -f Makefile.test")
    os.system("make -f Makefile.test -j 16")
    p = subprocess.Popen(["./runtest"], stdout=subprocess.PIPE)
    out = p.stdout.read()
    print(out)
    if "FAILED" in out:
        print("!!!!!!!!!!!!!!!!!Tests failed!!!!!!!!!!!!!!!!!")
        print("!!!!!!!!!!!!!!!!!Tests failed!!!!!!!!!!!!!!!!!")
        print("!!!!!!!!!!!!!!!!!Tests failed!!!!!!!!!!!!!!!!!")
        print("!!!!!!!!!!!!!!!!!Tests failed!!!!!!!!!!!!!!!!!")

    os.system("make clean -f Makefile.test")
    os.system("rm Makefile.test")

def clean_build_dir():
    if os.path.isfile("Makefile"):
        os.system("make clean")
        os.system("rm Makefile")
    os.system("rm src/runtime/attested_config.cpp")
    os.system("rm enclave/lib/runtime/attested_config.o")
    os.system("rm lib_harness/runtime/attested_config.o")

def build_enclave_func(name, binary_name, mode, hardware):
    if not os.path.isfile("funcs/" + name + "/Makefile.trusted"):
        os.system("cp " + enclave_func_makefile + " Makefile")
    else:
        os.system("cp " + "funcs/" + name + "/Makefile.trusted" + " Makefile")
    if not os.path.isfile("funcs/" + name + "/func.cpp"):
        raise Exception("No func.cpp file found for" + name)

    signed_enc_binary = binary_name + ".signed.so"
    enc_binary = binary_name + ".so"
    
    buildconf = SGX_HARDWARE + hardware + " " 
    if mode == "DEBUG":
        buildconf += SGX_DEBUG
    else:
        buildconf += SGX_RELEASE
    os.system("make" + buildconf + "Signed_Enclave_Name="+ signed_enc_binary + " Enclave_Name=" + enc_binary + " FUNC_NAME=" + name)

    if not os.path.isfile(signed_enc_binary):
        raise Exception("No file compiled for " + signed_enc_binary)

    os.system("mv " + signed_enc_binary +" "+ output_dir_name + "/")
    os.system("mv " + enc_binary + " "+ output_dir_name + "/")
    os.system("chmod 777 " + "./" + output_dir_name + "/" + enc_binary)
    build_move_auxillary_data(name)
    clean_build_dir()

def build_lib_func(name, binary_name, mode, hardware):
    if not os.path.isfile("funcs/" + name + "/Makefile.untrusted"):
        os.system("cp " + lib_func_makefile + " Makefile")
    else:
        os.system("cp " + "funcs/" + name + "/Makefile.untrusted" + " Makefile")

    if not os.path.isfile("funcs/" + name + "/func.cpp"):
        raise Exception("No func.cpp file found for" + name)
    buildconf = SGX_HARDWARE + hardware + " " 
    if mode == "DEBUG":
        buildconf += SGX_DEBUG
    else:
        buildconf += SGX_RELEASE
    os.system("make"+ buildconf + "FUNC_LIB=" + binary_name + ".so " + "FUNC_NAME=" + name)
   
    if not os.path.isfile( binary_name + ".so"):
        raise Exception("No " +  binary_name + ".so" + " file found")
   
    os.system("mv "+  binary_name + ".so" + " "+ output_dir_name + "/")
    build_move_auxillary_data(name)
    clean_build_dir()

def build_inject_configuration(config):
    with open ("src/runtime/attested_config.cpp", "w+") as conf:
        conf.write('''\n /* WARNING!! This file is auto generated and will be overwritten on build!*/ \n const char* static_attested_diggi_configuration = "'''+ config.replace("u'","\\\"").replace('\'',"\\\"") +'''";\n\n''')
    
def build_move_auxillary_data(name):
    cmd =  "rsync -av funcs/" + name + "/ " + output_dir_name
    os.system(cmd + " >/dev/null 2>&1")
    os.system("rm -r -f "+output_dir_name+"/*.cpp") #removing unnecesary files
    os.system("rm -r -f "+output_dir_name+"/*.c") #removing unnecesary files
    os.system("rm -r -f "+output_dir_name+"/*.h") #removing unnecesary files


def build_diggi_base_executable(name, mode, hardware):
    os.system("cp " + proc_func_makefile + " Makefile")
    buildconf = SGX_HARDWARE + hardware + " " 

    if mode == "DEBUG":
        buildconf += SGX_DEBUG
    else:
        buildconf += SGX_RELEASE

    os.system("make" + buildconf + "App_Name=" + name)

    if not os.path.isfile(name):
        raise Exception("No " + name + " file found")
   
    os.system("mv "+ name + " "+ output_dir_name + "/")
    clean_build_dir()
def build_diggi(configuration, runtests = False, preclean = True):

    setup_build_env(configuration, preclean)
    
    t = threading.Thread(target=run_tests)

    if(runtests):
        t.start()

    json_configuration = get_diggi_configuration(configuration)
    count  = 0 
    builddict = {}
    print("!!!!!!!!!!!!!!!!building diggi_base_executable!!!!!!!!!!!!!!!!")
    buildmode = json_configuration['BuildMode']
    hardwaremode = json_configuration['TrustedHardwareMode']
    print("BuildMode = " + str(buildmode))
    print("TrustedHardwareMode = " + str(hardwaremode))

    build_diggi_base_executable("diggi_base_executable", buildmode, hardwaremode)
    
    for attribute, value in json_configuration['structure'].iteritems():
        if "process" not in attribute:
            raise Exception("Malformed configuration file")
        count += 1  
        for attribute1, value1 in value.iteritems():
            ##Process level funcs
            if "funcs" in attribute1:
                for value2 in value1:
                    name = value2.split('@')[0]
                    if not builddict.has_key(value2 + ".so"):
                        print("!!!!!!!!!!!!!!!!building lib func: " + value2 +"!!!!!!!!!!!!!!!!")
                        build_inject_configuration(str(value1[value2]))
                        build_lib_func(name, value2 , buildmode, hardwaremode)
                        builddict[value2 + ".so"] = 1
            ##Enclave level funcs
            elif "enclave" in attribute1:
                for attribute3, value3 in value1.iteritems():
                    if "funcs" in attribute3:
                        for value2 in value3:
                            name = value2.split('@')[0]
                            if not builddict.has_key(value2 + ".signed.so"):
                                print("!!!!!!!!!!!!!!!!building enclave func: " + value2 +"!!!!!!!!!!!!!!!!")
                                build_inject_configuration(str(value3[value2]))
                                build_enclave_func(name, value2, buildmode, hardwaremode)
                                builddict[value2 + ".signed.so"] = 1
                    else:
                        raise Exception("Malformed configuration file")
            else:
                continue

    clean_build_dir()
    if(runtests):
        t.join()
