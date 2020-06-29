#ifndef HARNESS_H
#define HARNESS_H
#ifdef UNTRUSTED_APP

#include "runtime/DiggiUntrustedRuntime.h"
#include "datatypes.h"
#include "threading/ThreadPool.h"
#include "runtime/DiggiAPI.h"
#include "network/Connection.h"
#include "network/HTTP.h"
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <fstream>
#include "telemetry.h"
#include "misc.h"

#endif
#endif /* !HARNESS_H */
