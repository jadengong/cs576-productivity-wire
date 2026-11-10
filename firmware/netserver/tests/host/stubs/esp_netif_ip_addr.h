#pragma once
#include <stdint.h>
typedef struct { uint32_t addr; } esp_ip4_addr_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(ip) (unsigned)((const uint8_t *)&((ip)->addr))[0], \
    (unsigned)((const uint8_t *)&((ip)->addr))[1], \
    (unsigned)((const uint8_t *)&((ip)->addr))[2], \
    (unsigned)((const uint8_t *)&((ip)->addr))[3]
