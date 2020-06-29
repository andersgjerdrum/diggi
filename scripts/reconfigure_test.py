import threading, time
from diggi_build import build_diggi, output_dir_name
from diggi_orchestration import RunNodeDebug, UpdateConfiguration

build_diggi("configuration.json")

def run_node():
    RunNodeDebug(output_dir_name,"4442",1)


url = "http://localhost:4442/"

t = threading.Thread(target=run_node)
t.start()
time.sleep(1) #increase when running valgrind


for x in range(0,100):
    print("Updating configuration!")
    time.sleep(0.5) # too fast updates causes distress
    UpdateConfiguration(url, 'configuration.json', 'PetNat') # larsb: inserted random identifier to make it run.


print("Test Completed Successfully!")