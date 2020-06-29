#ifndef PACK_H
#define PACK_H

#include "posix/io_types.h"

/*
Marshalling methods for various types that are copied and passed in Diggi messages.
Basically takes a pointer to a buffer and a variable (and sometimes size), and 
copies that variable to that buffer. Buffer must be allocated in advance.

@author: lars.brenna@uit.no
*/


class Pack {
    public:

        template <typename T>
        static void pack(uint8_t **buffer, T payload) {
            //TODO: include sanity check and memory bounds check here! 
            //STATIC_ASSERT(std::is_trivially_copyable_v<T>));
            memcpy(*buffer, &payload, sizeof(payload));
            //*buffer = (unsigned int*)*buffer + sizeof(T);
            *buffer += sizeof(T);
        }
        template <typename T>
        static T unpack(uint8_t **buffer){
            T retval;
            memcpy(&retval, *buffer, sizeof(T));
            //*buffer = (unsigned int*)*buffer + sizeof(T);
            *buffer += sizeof(T);
            return retval; 
        }

        static void packBuffer(uint8_t **buffer, uint8_t *payload, unsigned int length);
        static void unpackBuffer(uint8_t **buffer, uint8_t *payload, unsigned int length);
      
};

#endif
