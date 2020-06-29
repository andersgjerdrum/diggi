#ifndef TELEMETRY_H
#define TELEMETRY_H
#include "sgx/ocalls.h"
#if !defined(DIGGI_ENCLAVE)
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#include "DiggiAssert.h"
#include <inttypes.h>
typedef struct observation_t {
	size_t correlation_id;
	double measurement;
	observation_t * next;
	char * tag;
}observation_t;

typedef struct telemetry_t {
	size_t global_correlation_id;
	struct timespec tsa;
	size_t observation_count;
	observation_t *next;
}telemetry_t;

uint64_t rdtsc();
void get_time_(struct timespec *ts);

unsigned long long timespec_diff_ns_(struct timespec *start, struct timespec *end);

static telemetry_t *telemetrysingleton[[gnu::unused]] = nullptr;

void telemetry_init();

void telemetry_init(telemetry_t *sngl);
void telemetry_start(telemetry_t * tel);
void telemetry_start();
void telemetry_write();
void telemetry_capture_(telemetry_t * tel, const char* tag);
#else
#include "enclave_t.h"
#endif
void telemetry_capture(const char* tag);

#endif