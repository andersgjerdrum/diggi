import requests, os


def RunNodeSGXDebug(cwd, port, identifier):
    if not os.path.isdir(cwd):
        raise Exception("No Output directory found")
    os.system("cd " + cwd + """ && /opt/intel/sgxsdk/bin/sgx-gdb --eval-command=run --eval-command=quit --args diggi_base_executable --port """ + port + " --id " + str(identifier))


# add to watch memory location
# --eval-command="watch *0x7ffff00008f0"
# add to test signal handling
# --eval-command="handle SIGINT pass"
def RunNodeDebug(cwd, port, identifier):
    if not os.path.isdir(cwd):
        raise Exception("No Output directory found")
    os.system("cd " + cwd + """ && gdb --eval-command="catch throw" --eval-command="set print thread-events off" --eval-command=run --eval-command=quit --args diggi_base_executable --port """ + port + " --id " +  str(identifier))

def RunNode(cwd, port, identifier):
    if not os.path.isdir(cwd):
        raise Exception("No Output directory found")
    os.system("cd " + cwd + " && ./diggi_base_executable --port " + port + " --id " +  str(identifier))

#  add --leak-check=full  for complete memory leak test
# valgrind forces all resources on single thread, reduce number of concurrent funcs to account for this.

def RunNodeValgrind(cwd, port, identifier):
    if not os.path.isdir(cwd):
        raise Exception("No Output directory found")
    os.system("cd " + cwd + " && valgrind --leak-check=full --log-file=valgrind.output.log --track-origins=yes --fair-sched=yes  ./diggi_base_executable --port " + port + " --id " +  str(identifier))
def RunNodeCallgrind(cwd, port, identifier):
    if not os.path.isdir(cwd):
        raise Exception("No Output directory found")
    os.system("cd " + cwd + " && valgrind --tool=callgrind --log-file=valgrind.output.log --fair-sched=yes  ./diggi_base_executable --port " + port + " --id " +  str(identifier))

def UpdateConfiguration(url, configfile, identifier):

    def generateConfig(cfg): 
        config = open(cfg, 'r').read()
        yield config

    headers = {
        'content-type': "application/json",
        'cache-control': "no-cache",
        'postman-token': "d0cf25ed-2d3f-2ea0-ff19-ee2e9c5e13ef",
        'self-identity': identifier,
        'diggi-message-type':'configuration'
        }
    response = requests.request("POST", url, data=generateConfig(configfile), headers=headers)
