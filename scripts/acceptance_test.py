import os, csv
from diggi_build import *



def deploy_configuration(configuration, gdb):
    os.system("")
    if gdb:
        return os.system("""cd output && gdb --eval-command="catch throw" --eval-command="set print thread-events off" --eval-command=run --eval-command=quit --args  diggi_base_executable --id 1 --port 6000 --exit_when_done --config """ + " ../" + configuration)
    else:
        return os.system("cd output && ./diggi_base_executable --id 1 --port 6000 --exit_when_done --config " + " ../" + configuration)

def execute_configuration(configuration, Preclean = True, gdb = False):
    print("BUILDING ACCEPTANCE TEST = !!!! " + configuration + " !!!!" )
    build_diggi(configuration, False, Preclean)
    print("EXECUTING ACCEPTANCE TEST = !!!! " + configuration + " !!!!" )
    retval = deploy_configuration(configuration, gdb)
    if retval != 0:
        print("!!!ACCEPTANCETEST FAILED!!!!")
        exit(-1)
    print("EXECUTION DONE")

    print("\n")
    print("\n")
    
def get_list_of_items(file):
    with open(file) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        retlist = list()
        for row in csv_reader:
            retlist.append(row[1])
    return retlist
def create_x_lables(lable, count):
    retlist = list()
    for i in range(0,count):
        retlist.append(lable)
    return retlist
    
def write_csv(filename, list_o_list, headers):
    f = open(filename,"w+")
    for x in range(0, len(headers)):
        f.write(headers[x] + ",")
    f.write("\n")
    for y in range(0, len(list_o_list[0])): #row
        for i in range(0, len(list_o_list)): #column
            if(i < len(list_o_list)):
                f.write(list_o_list[i][y] + ",")
            else:
                f.write(list_o_list[i][y])
        f.write("\n")
    f.close();
def generate_rw_type_list(listcount,repeats):
    temp = create_x_lables("read", listcount / 2)
    temp.extend(create_x_lables("write",listcount / 2))
    retlist = list()
    for i in range(0, repeats):
        retlist.extend(temp)
    return retlist

def rw_throughput_test():
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_4k_async.json")
    async_rw = get_list_of_items("output/telemetry.log")[:-1]
    
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_4k_sync.json")
    sync_rw = get_list_of_items("output/telemetry.log")[:-1]
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_4k_ocalls.json")
    ocalls_rw = get_list_of_items("output/telemetry.log")[:-1]
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_8k_async.json")
    async_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_8k_sync.json")
    sync_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
     
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_8k_ocalls.json")
    ocalls_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_16k_async.json")
    async_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_16k_sync.json")
    sync_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_16k_ocalls.json")
    ocalls_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_32k_async.json")
    async_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_32k_sync.json")
    sync_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_32k_ocalls.json")
    ocalls_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_64k_async.json")
    async_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_64k_sync.json")
    sync_rw.extend(get_list_of_items("output/telemetry.log")[:-1])
 
    os.system("rm output/test.file.test")
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_throughput_64k_ocalls.json")
    ocalls_rw.extend(get_list_of_items("output/telemetry.log")[:-1])


    typelength = 2
    rw_list = generate_rw_type_list(typelength, 5)
    
    size_list = create_x_lables("4", typelength)
    size_list.extend(create_x_lables("8", typelength))
    size_list.extend(create_x_lables("16", typelength))
    size_list.extend(create_x_lables("32", typelength))
    size_list.extend(create_x_lables("64", typelength))
    assert(len(rw_list) == len(size_list))
    assert(len(rw_list) == len(async_rw))
    assert(len(rw_list) == len(sync_rw))
    assert(len(rw_list) == len(ocalls_rw))

    columns = list()

    columns.append(rw_list)
    columns.append(size_list)
    columns.append(sync_rw)
    columns.append(async_rw)
    columns.append(ocalls_rw)


    write_csv("experiments/rw_throughput_total_results.csv", columns,["type","size","sync","async","ocalls"])

def rw_io_test():
    os.system("rm output/telemetry.log")
    execute_configuration("acceptancetests/rw_io_4k_baseline.json")
    baseline = get_list_of_items("output/telemetry.log")
    os.system("rm output/telemetry.log")
    typelength = len(baseline)

    execute_configuration("acceptancetests/rw_io_4k_full_enc.json")
    fullenc = get_list_of_items("output/telemetry.log")
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_4k_full_noenc.json")
    noenc = get_list_of_items("output/telemetry.log")
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_4k_ocall.json")
    ocall = get_list_of_items("output/telemetry.log")
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_8k_baseline.json")
    baseline.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_8k_full_enc.json")
    fullenc.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_8k_full_noenc.json")
    noenc.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_8k_ocall.json")
    ocall.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_16k_baseline.json")
    baseline.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_16k_full_enc.json")
    fullenc.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_16k_full_noenc.json")
    noenc.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_16k_ocall.json")
    ocall.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_32k_baseline.json")
    baseline.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_32k_full_enc.json")
    fullenc.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_32k_full_noenc.json")
    noenc.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_32k_ocall.json")
    ocall.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_64k_baseline.json")
    baseline.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_64k_full_enc.json")
    fullenc.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_64k_full_noenc.json")
    noenc.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    execute_configuration("acceptancetests/rw_io_64k_ocall.json")
    ocall.extend(get_list_of_items("output/telemetry.log"))
    os.system("rm output/telemetry.log")

    rw_list = generate_rw_type_list(typelength, 5)

    size_list = create_x_lables("4", typelength)
    size_list.extend(create_x_lables("8", typelength))
    size_list.extend(create_x_lables("16", typelength))
    size_list.extend(create_x_lables("32", typelength))
    size_list.extend(create_x_lables("64", typelength))

    assert(len(rw_list) == len(size_list))
    assert(len(rw_list) == len(ocall))
    assert(len(rw_list) == len(noenc))
    assert(len(rw_list) == len(fullenc))
    assert(len(rw_list) == len(baseline))

    columns = list()

    columns.append(rw_list)
    columns.append(size_list)
    columns.append(ocall)
    columns.append(noenc)
    columns.append(fullenc)
    columns.append(baseline)

    write_csv("experiments/rw_io_total_results.csv", columns,["type","size","ocall","noenc","fullenc","baseline"])

#################################################################################################################################
# 1. Add a line with your configuration file to execute as an acceptance test.                                                  #
# 2. Remember that all funcs executing as acceptance tests must volentarily exit using the SignalHandlers->Stopfunc() call.   #
#################################################################################################################################

execute_configuration("acceptancetests/clientserver_acceptance_test_configuration.json")
execute_configuration("acceptancetests/attestation_acceptance_test.json")
execute_configuration("acceptancetests/storageacceptance_testconfiguration.1.json")
execute_configuration("acceptancetests/storage_in_memory.json")
execute_configuration("acceptancetests/record_test.json")
# if these files exist, the replay will fail.
os.system("rm output/*boron.db")
os.system("rm output/*boron.db-journal")

execute_configuration("acceptancetests/replay_test.json", False)

rw_io_test()
rw_throughput_test()