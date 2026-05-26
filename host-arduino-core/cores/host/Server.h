#ifndef HOST_ARDUINO_SERVER_H
#define HOST_ARDUINO_SERVER_H

#include <stdint.h>

#include "Print.h"

class Server : public Print
{
public:
    virtual void begin(uint16_t port = 0) = 0;
};

#endif
