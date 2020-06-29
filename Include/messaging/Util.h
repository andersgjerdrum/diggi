#ifndef COMUTILS_H
#define COMUTILS_H

#include <vector>
#include <string>
#include "Logging.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif // __cplusplus

#include "datatypes.h"
using namespace std;

#define HEADERSIZE sizeof(unsigned int)

static const char base64index[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'};

const char paddingCharacter = '=';
const uint32_t MaskLowLow = 0x3F;
const uint32_t MaskLowHigh = 0xFC0;
const uint32_t MaskHighLow = 0x3F000;
const uint32_t MaskHighHigh = 0xFC0000;

class Utils
{
public:
    static string itoa(int i)
    {
        char str[30];
        snprintf(str, 30, "%d", i);
        string numstring(str);
        return numstring;
    }
    static string aid_to_str(aid_t a)
    {
        char str[30];
        snprintf(str, 30, "%lu", a.raw);
        string numstring(str);
        return numstring;
    }
    static string ftoa(float num)
    {
        char output[50];
        snprintf(output, 50, "%f", num);
        string numstring(output);
        return numstring;
    }
    static string utohex(unsigned int num)
    {
        char output[64];
        snprintf(output, 64, "%x", num);
        string numstring(output);
        return numstring;
    }
    static aid_t str_to_aid(string str)
    {
        unsigned long long dst = strtoll(str.c_str(), (char **)NULL, 10);
        aid_t dst_n;
        dst_n.raw = dst;
        return dst_n;
    }
    static unsigned long str_to_id(string str)
    {
        return strtol(str.c_str(), (char **)NULL, 10);
    }
    static string id_to_string(unsigned long id)
    {
        char str[30];
        snprintf(str, 30, "%lu", id);
        string numstring(str);
        return numstring;
    }
    static void print_byte_array(void *mem, uint32_t len, ILog *log, LogLevel lvl)
    {
        char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                         '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

        char *data = (char *)mem;
        log->Log(lvl, "============== BYTES ( SIZE=%u ) ===============\n", len);

        std::string s(len * 2, ' ');
        for (unsigned i = 0; i < len; ++i)
        {
            s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
            s[2 * i + 1] = hexmap[data[i] & 0x0F];
        }

        log->Log(lvl, "%s\n", s.c_str());
        log->Log(lvl, "============== END =============================\n");
    }
    static void print_byte_array(void *mem, uint32_t len)
    {
        char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                         '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

        char *data = (char *)mem;
        printf("============== BYTES ( SIZE=%u ) ===============\n", len);

        std::string s(len * 2, ' ');
        for (unsigned i = 0; i < len; ++i)
        {
            s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
            s[2 * i + 1] = hexmap[data[i] & 0x0F];
        }

        printf("%s\n", s.c_str());
        printf("============== END =============================\n");
    }
};

#endif