#ifndef HOST_ARDUINO_PGMSPACE_H
#define HOST_ARDUINO_PGMSPACE_H

#include <string.h>

#ifdef __cplusplus
class __FlashStringHelper;
#endif

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
#ifdef __cplusplus
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#else
#define FPSTR(pstr_pointer) (pstr_pointer)
#endif
#endif
#ifndef F
#ifdef __cplusplus
#define F(str) (FPSTR(PSTR(str)))
#else
#define F(str) (PSTR(str))
#endif
#endif

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_ptr(addr) (*(const void *const *)(addr))

// memcpy_P / memcmp_P / str*_P are guarded on ARDUINO so they only get
// defined for the default Arduino-host build. When ARDUINO is not
// defined (e.g., mode=lgfx for LovyanGFX/M5GFX headless), the LovyanGFX
// utility/pgmspace.h provides matching `static inline` functions inside
// `#ifndef ARDUINO`; redefining them here as macros would mangle that
// declaration (e.g. `static inline void* memcpy_P(...)` would expand to
// `static inline void* memcpy(...)`) and cascade into unrelated template
// errors in result.hpp.
#ifdef ARDUINO
#define memcpy_P(dest, src, n) memcpy((dest), (src), (n))
#define memcmp_P(s1, s2, n) memcmp((s1), (s2), (n))
#define strlen_P strlen
#define strcpy_P strcpy
#define strncpy_P strncpy
#define strcat_P strcat
#define strncat_P strncat
#define strcmp_P strcmp
#define strncmp_P strncmp
#define strstr_P strstr
#endif

#endif
