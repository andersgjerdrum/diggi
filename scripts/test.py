#!/usr/bin/env python

import os, json
from diggi_build import tests_makefile

if os.path.isfile("Makefile.test"):
    os.system("make clean -f Makefile.test")
    os.system("rm Makefile.test")

os.system("cp " + tests_makefile + " Makefile.test")
os.system("make -f Makefile.test -j 16")

