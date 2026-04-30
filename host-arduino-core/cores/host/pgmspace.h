#ifndef HOST_ARDUINO_PGMSPACE_H
#define HOST_ARDUINO_PGMSPACE_H

#include <string.h>

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PGM_P
#define PGM_P const char *
#endif
#ifndef PSTR
#define PSTR(str) (str)
#endif
#ifndef FPSTR
#define FPSTR(pstr_pointer) (pstr_pointer)
#endif
#ifndef F
#define F(str) (str)
#endif

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_ptr(addr) (*(const void * const *)(addr))
#define strlen_P strlen
#define strcpy_P strcpy
#define strncpy_P strncpy
#define strcmp_P strcmp
#define strncmp_P strncmp

#endif
