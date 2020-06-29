/**
 * @file Pack.cpp
 * @author Lars Brenna (lars.brenna@uit.no)
 * @brief marshalling implementation for correctly packing typed variables into message struct.
 * @version 0.1
 * @date 2020-01-31
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "messaging/Pack.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"


void Pack::packBuffer(uint8_t **buffer, uint8_t *payload, unsigned int length){
	memcpy(*buffer, payload, length);
    //auto ptr = static_cast<unsigned int*>(*buffer);
    *buffer += length;
}

void Pack::unpackBuffer(uint8_t **buffer, uint8_t *payload, unsigned int length){
    memcpy(payload, *buffer, length);
    //auto ptr = static_cast<unsigned int*>(*buffer);
    *buffer += length;
}

#pragma GCC diagnostic pop