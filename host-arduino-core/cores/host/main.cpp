#include "Arduino.h"

#ifdef _WIN32
int main(int argc, char **argv);
#else
int main(int argc, char **argv) __attribute__((weak));
#endif
int main(int argc, char **argv)
{
    if (!HostArduino::runtimeStart(argc, argv)) {
        return 1;
    }

    setup();
    while (!HostArduino::runtimeShouldStop()) {
        loop();
        HostArduino::runtimePoll();
    }

    HostArduino::runtimeStop();
    return 0;
}
