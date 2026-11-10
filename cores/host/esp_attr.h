#ifndef HOST_ARDUINO_ESP_ATTR_H
#define HOST_ARDUINO_ESP_ATTR_H

// ESP-IDF memory placement attributes. On the chip they put code in IRAM or
// data in DRAM / RTC memory; the host has one flat address space, so every
// one of them is empty. Defined only when absent so a sketch that supplies
// its own definitions keeps them.

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif
#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif
#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif
#ifndef RTC_IRAM_ATTR
#define RTC_IRAM_ATTR
#endif
#ifndef WORD_ALIGNED_ATTR
#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))
#endif
#ifndef ARDUINO_ISR_ATTR
#define ARDUINO_ISR_ATTR IRAM_ATTR
#endif

#endif
