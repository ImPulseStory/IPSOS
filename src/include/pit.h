#ifndef IPSOS_PIT_H
#define IPSOS_PIT_H

#include <stdint.h>

#define PIT_FREQUENCY 100u

void pit_init(uint32_t frequency);

#endif
