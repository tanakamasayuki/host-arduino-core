// Tests for the host FS facades (LittleFS / SPIFFS / FFat / SD).
//
// All four facades map to a directory tree next to the executable. The
// test writes a small payload through each facade and reads it back.

#include <Arduino.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include <FFat.h>
#include <SD.h>

static int g_pass = 0;
static int g_total = 0;

#define EXPECT_TRUE(name, cond)    \
    do                             \
    {                              \
        ++g_total;                 \
        if (cond)                  \
        {                          \
            ++g_pass;              \
            Serial.print("PASS "); \
            Serial.println(name);  \
        }                          \
        else                       \
        {                          \
            Serial.print("FAIL "); \
            Serial.println(name);  \
        }                          \
    } while (0)

static void exerciseFS(fs::FS &filesystem, const char *label)
{
    String tag = String(label);
    EXPECT_TRUE((tag + ":begin").c_str(), filesystem.begin());

    const char *path = "/host_arduino_test.txt";
    if (filesystem.exists(path))
    {
        filesystem.remove(path);
    }

    File wf = filesystem.open(path, FILE_WRITE);
    EXPECT_TRUE((tag + ":open_write").c_str(), (bool)wf);
    if (wf)
    {
        const size_t n = wf.print("payload-");
        wf.print(label);
        wf.close();
        EXPECT_TRUE((tag + ":wrote_bytes").c_str(), n > 0);
    }

    EXPECT_TRUE((tag + ":exists").c_str(), filesystem.exists(path));

    File rf = filesystem.open(path, FILE_READ);
    EXPECT_TRUE((tag + ":open_read").c_str(), (bool)rf);
    if (rf)
    {
        String content;
        while (rf.available())
        {
            content += (char)rf.read();
        }
        rf.close();
        const String expected = String("payload-") + label;
        EXPECT_TRUE((tag + ":content").c_str(), content == expected);
    }

    EXPECT_TRUE((tag + ":remove").c_str(), filesystem.remove(path));
    EXPECT_TRUE((tag + ":gone").c_str(), !filesystem.exists(path));
}

void setup()
{
    Serial.begin(115200);
    Serial.println("TEST start fs");

    exerciseFS(LittleFS, "LittleFS");
    exerciseFS(SPIFFS, "SPIFFS");
    exerciseFS(FFat, "FFat");
    exerciseFS(SD, "SD");

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop()
{
    delay(10);
}
