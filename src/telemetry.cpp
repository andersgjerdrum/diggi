/**
 * @file telemetry.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief Telemetry implementation for capturing high resolution timings of runtime interaction.
 * @version 0.1
 * @date 2020-02-05
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "telemetry.h"


#if !defined(DIGGI_ENCLAVE)
/**
 * @brief retrieves timing from OS
 * Only usable outside enclave.
 */
void get_time_(struct timespec *ts)
{
	if (clock_gettime(CLOCK_MONOTONIC, ts) < 0) {
		DIGGI_ASSERT(false);
	}
}
/**
 * @brief calculates diff off timespec
 * 
 * @param start 
 * @param end 
 * @return unsigned long long 
 */
unsigned long long timespec_diff_ns_(struct timespec *start, struct timespec *end)
{
	unsigned long long ns = 0;

	if (end->tv_nsec < start->tv_nsec) {
		ns += 1000000000 * (end->tv_sec - start->tv_sec - 1);
		ns += 1000000000 - start->tv_nsec + end->tv_nsec;
	}
	else {
		ns += 1000000000 * (end->tv_sec - start->tv_sec);
		ns += end->tv_nsec - start->tv_nsec;
	}
	return ns;
}
/**
 * @brief invoke read timestamp counter instruction
 * 
 * @return uint64_t 
 */
uint64_t rdtsc() {
	unsigned int lo, hi;
	__asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)hi << 32) | lo;

}
/**
 * @brief initialize telemetry object, sets a signleton object usable once per symbol space (diggi instance)
 * 
 */
void telemetry_init()
{
	telemetrysingleton = (telemetry_t*)calloc(sizeof(telemetry_t), 1);
}
/**
 * @brief set singleton specifically
 * 
 * @param sngl 
 */

void telemetry_init(telemetry_t *sngl)
{
	telemetrysingleton = sngl;
}
/**
 * @brief start telemetry capturing using specific telemetry object
 * 
 * @param tel 
 */
void telemetry_start(telemetry_t * tel)
{
	get_time_(&(tel->tsa));
	tel->global_correlation_id++;
}
/**
 * @brief start telemetry caputuring
 * 
 */
void telemetry_start()
{
	telemetry_start(telemetrysingleton);
}

/**
 * @brief write telemetry data to disk.
 * 
 */
void telemetry_write() {

	observation_t *head = telemetrysingleton->next;
	FILE * pFile = fopen("telemetry.log", "a+");
	while (head != NULL) {

		fprintf(pFile, "%s,%lf,%lu\n", head->tag, head->measurement, head->correlation_id);
		head = head->next;
	}
    telemetrysingleton->next = nullptr;
	fflush(pFile);
	fclose(pFile);
}
/**
 * @brief capture telemetry event with a given tagg using specified telemetry object
 * tag is voulentary string used to describe event.
 * @param tel 
 * @param tag 
 */
void telemetry_capture_(telemetry_t * tel, const char* tag)
{
	DIGGI_ASSERT(tel != nullptr);
	struct timespec tsb;
	get_time_(&tsb);
	double time_spent = timespec_diff_ns_(&(tel->tsa), &tsb);
	// double secs = (double)time_spent / 1000000000;
	observation_t *obs = (observation_t*)calloc(sizeof(observation_t), 1);
	obs->correlation_id = tel->global_correlation_id;
	obs->measurement = time_spent;
	obs->tag = (char*)malloc(strlen(tag) + 1);
	memcpy(obs->tag, tag, strlen(tag) + 1);
	obs->next = tel->next;
	tel->next = obs;
	tel->observation_count++;
	get_time_(&(tel->tsa));
}
#endif
/**
 * @brief wrapper telemetry capture method for given tag
 * uses singleton telemetry object
 * depending on if inside enclave, the telemetry object will perform an ocall before capture.
 * @warning if executing inside enclave, the ocall operation will incurr an added noise overhead on the measurement.
 * @param tag 
 */
void telemetry_capture(const char* tag)
{
#ifdef DIGGI_ENCLAVE
	ocall_telemetry_capture(tag);
#else
	telemetry_capture_(telemetrysingleton, tag);
#endif
}









