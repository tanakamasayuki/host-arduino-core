#include "Arduino.h"

extern void setup();
extern void loop();

// Provide a weak default main so user sketches can override main() if desired.
// The linker will prefer a user-defined strong main over this weak symbol.
int main(int argc, char **argv) __attribute__((weak));
int main(int argc, char **argv)
{
    // Arduino-style entrypoint: call setup() once, then loop() forever.
    setup();
    for (;;)
    {
        loop();
    }
    return 0;
}
