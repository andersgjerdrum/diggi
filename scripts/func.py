import threading, time, sys, os, socket
import getpass

from diggi_orchestration import *
from diggi_build import *

#Which process to debug
#setup key exchange beforehand

#ssh-keygen -t rsa # ENTER to every field
#ssh-copy-id myname@somehost



ownip = (([ip for ip in socket.gethostbyname_ex(socket.gethostname())[2] if not ip.startswith("127.")] or [[(s.connect(("8.8.8.8", 53)), s.getsockname()[0], s.close()) for s in [socket.socket(socket.AF_INET, socket.SOCK_DGRAM)]][0][1]]) + ["no IP found"])[0]
assert(len(ownip))
owndnsname = socket.gethostbyaddr(ownip)[0]
def run_node(port, identifier):
    RunNode("output", port, identifier)

def run_node_debug(port, identifier):
    #time.sleep(5) # debug late start
    RunNodeDebug("output", port, identifier)
def run_node_valgrind(port,identifier):
    RunNodeValgrind("output",port,identifier)
def run_node_sgx_debug(port,identifier):
    RunNodeSGXDebug("output", port,identifier)
def run_node_callgrind(port,identifier):
    RunNodeCallgrind("output",port,identifier)
    
def run_node_remote(host, port, identifier):
    cmd = '''ssh -tt '''+ str(getpass.getuser()) + '''@''' + str(host) +''' "cd ~/output && ./diggi_base_executable --port '''+ str(port) +''' --id ''' + str(identifier) + '''" '''
    print(cmd)
    os.system(cmd)

def func_start():

    json_configuration = get_diggi_configuration('configuration.json')
    identifier = 0
    debugtarget = int(json_configuration['DebugTargetProcess'])
    debugengine = json_configuration['DebugEngine']
    thread_arr = list()
    for attribute, value in json_configuration['structure'].iteritems():
        destination = ""
        identifier += 1
        for attribute1, value1 in value.iteritems():
            if "network" in attribute1:
                destination = value1
        host = destination.split(':')[0]
        port = destination.split(':')[1]
        print(host)
        print(owndnsname)
        if (host == ownip ) or (host == owndnsname) or (host == "127.0.0.1"):
            if debugtarget == identifier:
                if debugengine == "gdb":
                    t = threading.Thread(target=run_node_debug, args=(port, identifier))
                elif debugengine == "valgrind":
                    t = threading.Thread(target=run_node_valgrind, args=(port, identifier))
                elif debugengine == "sgx-gdb":
                    t = threading.Thread(target=run_node_sgx_debug, args=(port, identifier))
                elif debugengine == "callgrind":
                    t = threading.Thread(target=run_node_callgrind, args=(port, identifier))
                else:
                    t = threading.Thread(target=run_node, args=(port, identifier))
            else:
                t = threading.Thread(target=run_node, args=(port, identifier))
            t.start()
            thread_arr.append(t)
        else:
            #setup key exchange beforehand to avoid password for each copy
            print("PREREQUISITE BEFORE EXECUTING")
            print("Create ssh identity and copy ids to remote machines: ")
            print(" ssh-keygen -t rsa")
            print(" ssh-copy-id myname@somehos")
            os.system("ssh " +  str(getpass.getuser())+ "@" + host + " 'rm -r -f output/'") 
            os.system("ssh " +  str(getpass.getuser())+ "@" + host + " 'mkdir -p output'") 
            os.system("scp -r output/* "+ str(getpass.getuser())+ "@" + host + ":~/output/")
            #old rsync, not used anymore
            #os.system('''rsync --delete --ignore-times --ignore-existing -arvce "ssh -o StrictHostKeyChecking=no"  -r ~/output/ '''+ host +''':~/output/''')
            t = threading.Thread(target=run_node_remote,args=(host, port, identifier))
            t.start()
            thread_arr.append(t)
    while True:
        time.sleep(1)