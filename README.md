# Coding standard

For developers of diggi: adhere to the following coding standards adopted from CMU department of electrical engineering
https://users.ece.cmu.edu/~eno/coding/CCodingStandard.html
https://users.ece.cmu.edu/~eno/coding/CppCodingStandard.html

# Prerequisites
Download required packages from apt-get using the following command:
``` bash
sudo apt-get update 
sudo apt-get install cmake valgrind gdbserver build-essential ocaml ocamlbuild automake autoconf libtool wget python python-pip libssl-dev libcurl4-openssl-dev protobuf-compiler libprotobuf-dev gdb debhelper

```
Install python requests:
``` bash
pip install requests
```
### Google Performance tools
Diggi is dependant on google performance tools for memory allocation, specifically tmalloc.
Source of which can be found here: https://github.com/gperftools/
### Test
In order to run unittests you need to install  google c++ test framework as a shared library:
``` bash
wget https://github.com/google/googletest/archive/release-1.8.0.tar.gz
tar xf release-1.8.0.tar.gz
cd googletest-release-1.8.0
cmake -DBUILD_SHARED_LIBS=ON .
make
sudo cp -a googletest/include/gtest/ /usr/include/
sudo cp -a googlemock/gtest/libgtest_main.so googlemock/gtest/libgtest.so  /usr/lib/
``` 
Check that the linker knows the libraries:
``` bash
sudo ldconfig -v | grep gtest
``` 
### Intel Software Guard Extentions
Install intel-sgx driver and SDK here: https://github.com/01org/linux-sgx

### Dev Environment 
Requires Visual Studio 2017 (v15.4.5)
https://www.visualstudio.com/en-us/productinfo/installing-an-earlier-release-of-vs2017

NB: Build in WSL is broken in later versions of VS
#### WSL
Install Windows Subsystem for Linux (WSL) to enable windows developement
https://blogs.msdn.microsoft.com/vcblog/2017/04/11/linux-development-with-c-in-visual-studio/
##### Install WSL from command line (cmd.exe)
Enable Windows Subsystem for Linux in powershell:
``` powershell
Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux
``` 
Install using cmd.exe:
``` cmd
lxrun.exe /install
```


Uncomment or add 
``` bash
set bell-style none
```
to /etc/inputrc to remove anoying bell in bash editing
#### Create separate user in WSL
Create separate user in WSL before installing SGX SDK.\
Running in Simulation mode does not require sgx driver install.\
\
Create user in WSL (bash):
``` bash
adduser <username>
```

Add to sudoers list:
``` bash
sudo adduser <username> sudo
```
Run in Cmd.exe:
``` cmd
lxrun /setdefaultuser <username>
```
\
You may have to edit visual studio .proj file with correct ssh credentials.\
\
To allow visual studio to remote build 
modify /etc/ssh/sshd_config and add the following:
``` bash
PasswordAuthentication yes
AllowUsers <username>
``` 
Then restart the ssh daemon:
``` bash
sudo service ssh --full-restart
``` 
To change target build box in visual studio, make nessecary changes in  
Options-->Crossplatform-->Connection Manager settings.
# Components

General (simplified) archicture:
```ascii
|----------------------------------|
|  Library func  |  Enclave func |
|    (Untrusted)  |	   (Trusted)   |
|   STL 6 / glibc |    SGX-SDK     |
|----------------------------------|
|                                  |
|           func Runtime		   |
|			Base Proccess		   |
|		     (Untrusted)           |
|                                  |
|----------------------------------|
```

All source code under /src is compiled together, but IFDEF's (compiler directives) control what is "activated" to run where. 
Any code, including funcs, may be written to run in both modes just by these directives, thus reducing LoC.

funcs use the TrustedThreadPool and communicate with the SecureMessageManager. 

They deal with syscalls from funcs by intercepting and implementing as much as possible in userland/enclave-land. 
When a syscall has to be sent out of the func, this is implemented by sending messages to 
Special funcs (file_io_func, network_io_func, etc.) then receive the remaining syscalls as messages and perform the direct syscall interaction with the operating system.

The programming pattern for Special funcs is as follows (* is used to substitute for file/storage and for network):
```ascii
[[Enclave func] | *Stub -> *Manager [->Send MSG] ///RINGBUFFER\\\ [->Recv MSG] -> *Server | [Library func]]
```


## Configuration file
The diggi configuration is stored in configuration.json.\
An example configuration:
``` json
{
  "structure": {
    "process-1": {
      "network": "iad23.cs.uit.no:6002",
      "funcs": [ "http_server_func"],
      "enclave": {
        "funcs": []
      }
    },
    "process-2": {
      "network": "iad22.cs.uit.no:6000",
      "funcs": [ "file_io_func" ],
      "enclave": {
        "funcs": []
      }
    }
  }
}

```
## funcs
funcs are located under /funcs 

There are two types of funcs, and they are distinguished by the compiler directives "DIGGI_ENCLAVE" and "UNTRUSTED_APP". 
(See "Components" above.)


### Implementing a new func
Create a subfolder under /funcs
``` bash
mkdir funcs/<func name>
``` 
Create a func.cpp file:
``` bash
cd funcs/<func name>
touch func.cpp
``` 
Implement the follwing interface:
``` C
void func_init(void * ctx, int status);

void func_start(void *com, int status);

void func_stop(void *ctx, int status);

```
Add "\<func name>" to configuration.json in the respective class of func. Either inside an enclave or regular func.
 ``` json
"structure": {
    "process-1": {
      "network": "iad23.cs.uit.no:6002",
      "funcs": [ "http_server_func"],
      "enclave": {
        "funcs": []
      }
    },

``` 

# Compilation
Builds diggi, run unit tests and store the resulting binaries in /output
``` bash
python scripts/build.py
``` 
Run unit tests in isolation:
``` bash
python scripts/test.py
``` 
Run acceptance tests:
``` bash
python scripts/acceptance_test.py
```
Run diggi:
``` bash
python scripts/start.py
```

Setup key exchange befor starting diggi to avoid password prompt for each copy
``` python
ssh-keygen -t rsa
ssh-copy-id myname@somehost
```
# Running the code
### Known Issues
- Tests will fail during first compilation after setup, because -lsqlite library is missing, just rerun compilation and it will be present. This issue is caused by sqlibrary only being compiled by diggi build, and not unit tests.
#### SGX SERVICE
If sgx_create_enclave fails with SGX_ERROR_SERVICE_TIMEOUT run the following command:
``` bash
sudo service aesmd restart
```
##### aesmd service refuses to start
May happen if updates to kernel flushes sgx driver, reinstall sgx
##### Missing python requests module
Install requests using pip
``` bash
pip install requests
```
##### Linux subsystem 4 windows 
If connection fails:
``` bash
sudo service ssh --full-restart
``` 
If restart of sshd service fails try generating new host keys:
``` bash
sudo /usr/bin/ssh-keygen -A
``` 
PS: remember that once you close the last WSL shell, all services and executables are removed.

Visual studio version 15.6.2 conflicts with previous versions, and cannot be used interchangebly with older versions.
Will cause copying of source over ssh to break.
##### SGX SDK
- If .so libraries are not located, copy from /opt/intel/sgxsdk/ to /usr/lib/:
``` bash
sudo cp /opt/intel/sgxsdk/lib64/libsgx_uae_service_sim.so /usr/lib/ 
``` 
- Remember to install sgx sdk in /opt/intel/
- Intellisense expects linu-sgx source to be located in parent folders

