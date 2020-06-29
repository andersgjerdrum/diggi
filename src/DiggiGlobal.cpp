/**
 * @file diggi_global.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief implement static funcitons for requesting diggi api reference.
 * Static reference to diggi api, usable within each diggi instance.
 * Symbol space is different for each diggi instance( untrusted and enclave)
 * @warning if set twice within the same symbol space, will overwrite the static variable, particularly prone in unit tests, where all execution ocurs within the same symbol space.
 * 
 * @version 0.1
 * @date 2020-02-04
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "DiggiGlobal.h"
static IDiggiAPI *global_context = nullptr;

void SET_DIGGI_GLOBAL_CONTEXT(IDiggiAPI * ctx) {

	//DIGGI_ASSERT(ctx == nullptr);
	global_context = ctx;
}

IDiggiAPI *GET_DIGGI_GLOBAL_CONTEXT()
{
	//DIGGI_ASSERT(global_context);
	return global_context;
}