
from diggi_build import *
import os
import re

build_diggi("configuration.json", False)

regexppatern = 'metadata->enclave_css.body.enclave_hash.m:(.*?)metadata->enclave_css.body.isv_prod_id:'
print("\nMeasurements:")
os.system("rm measurements.txt >/dev/null 2>&1")
for file in os.listdir("output"):
    if "signed" in file:
        if not os.path.isfile("/opt/intel/sgxsdk/bin/x64/sgx_sign"):
            raise Exception("No Sign Tool availible in the intel SGX SDK at '/opt/intel/sgxsdk/bin/x64/sgx_sign'")

        cmd = "/opt/intel/sgxsdk/bin/x64/sgx_sign dump -enclave output/" + file +  " -dumpfile measurements.txt >/dev/null 2>&1"
        os.system(cmd)
        with open ("measurements.txt", "r") as myfile:
            data=myfile.read().replace('\n', '')
            result = re.search(regexppatern, str(data))
            unprocessed_measurement = result.group(1)
            array = unprocessed_measurement.replace('0x','',32).replace(' ', '', 32)

            print(file.split('.')[0] + " : " + array)
