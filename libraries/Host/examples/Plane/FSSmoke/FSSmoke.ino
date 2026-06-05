#include <Arduino.h>
#include <SPIFFS.h>
#include <LittleFS.h>
#include <FFat.h>
#include <SD.h>

void writeAndRead(fs::FS &filesystem, const char *label)
{
  if (!filesystem.begin())
  {
    Serial.print(label);
    Serial.println(" begin failed");
    return;
  }

  File file = filesystem.open("/dir/test.txt", FILE_WRITE);
  if (!file)
  {
    Serial.print(label);
    Serial.println(" open for write failed");
    return;
  }
  file.print(label);
  file.println(" hello");
  file.close();

  File readback;
  readback = filesystem.open("/dir/test.txt", FILE_READ);
  if (!readback)
  {
    Serial.print(label);
    Serial.println(" open for read failed");
    return;
  }

  Serial.print(label);
  Serial.print(" read: ");
  Serial.print(readback.readString());
  readback.close();
}

void setup()
{
  Serial.begin(115200);

  writeAndRead(SPIFFS, "SPIFFS");
  writeAndRead(LittleFS, "LittleFS");
  writeAndRead(FFat, "FFat");
  writeAndRead(SD, "SD");
}

void loop()
{
  delay(10);
}
