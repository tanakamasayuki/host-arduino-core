// Exercises the host Preferences (NVS) facade.
//
// In-memory only: data lives until the sketch exits. Verifies put/get
// roundtrip for the major types, default-value-on-missing-key behavior,
// type mismatch returning default, remove/clear, and namespace isolation.

#include <Arduino.h>
#include <Preferences.h>

static int g_pass = 0;
static int g_total = 0;

#define EXPECT(name, cond)             \
    do {                               \
        ++g_total;                     \
        if (cond) { ++g_pass; Serial.print("PASS "); Serial.println(name); } \
        else      { Serial.print("FAIL "); Serial.println(name); }           \
    } while (0)

void setup() {
    Serial.begin(115200);
    Serial.println("TEST start preferences");

    Preferences p;
    EXPECT("begin", p.begin("app", false));

    p.putUInt("count", 42);
    p.putInt("delta", -7);
    p.putBool("on", true);
    p.putFloat("f", 1.5f);
    p.putDouble("d", 3.14159);
    p.putString("name", "host");
    p.putULong64("big", 0x1122334455667788ULL);

    EXPECT("getUInt",   p.getUInt("count", 0) == 42);
    EXPECT("getInt",    p.getInt("delta", 0) == -7);
    EXPECT("getBool",   p.getBool("on", false) == true);
    EXPECT("getFloat",  p.getFloat("f", 0.0f) == 1.5f);
    EXPECT("getDouble", p.getDouble("d", 0.0) == 3.14159);
    EXPECT("getString", p.getString("name", String("")) == String("host"));
    EXPECT("getULong64", p.getULong64("big", 0) == 0x1122334455667788ULL);

    // Missing key returns default.
    EXPECT("missing_uint",   p.getUInt("nope", 99) == 99);
    EXPECT("missing_string", p.getString("nope", String("def")) == String("def"));
    EXPECT("isKey_true",  p.isKey("count"));
    EXPECT("isKey_false", !p.isKey("nope"));

    // Type mismatch returns default (we wrote count as UInt; reading as String must fall back).
    EXPECT("type_mismatch", p.getString("count", String("fallback")) == String("fallback"));

    // getBytes roundtrip.
    const uint8_t blob[5] = {0xDE, 0xAD, 0xBE, 0xEF, 0x55};
    p.putBytes("blob", blob, sizeof(blob));
    EXPECT("blob_len", p.getBytesLength("blob") == sizeof(blob));
    uint8_t out[5] = {0};
    const size_t got = p.getBytes("blob", out, sizeof(out));
    EXPECT("blob_got", got == sizeof(blob));
    EXPECT("blob_eq", std::memcmp(out, blob, sizeof(blob)) == 0);

    // C-string getString overload.
    char buf[16] = {0};
    const size_t n = p.getString("name", buf, sizeof(buf));
    EXPECT("cstr_n", n == 4);
    EXPECT("cstr_eq", strcmp(buf, "host") == 0);

    // remove / clear.
    EXPECT("remove",      p.remove("count"));
    EXPECT("removed",     !p.isKey("count"));
    EXPECT("remove_miss", !p.remove("nope"));
    p.clear();
    EXPECT("clear_blob", !p.isKey("blob"));
    EXPECT("clear_name", !p.isKey("name"));

    p.end();

    // Namespace isolation.
    Preferences a, b;
    a.begin("a", false);
    b.begin("b", false);
    a.putUInt("x", 1);
    b.putUInt("x", 2);
    EXPECT("ns_a", a.getUInt("x", 0) == 1);
    EXPECT("ns_b", b.getUInt("x", 0) == 2);
    a.end();
    b.end();

    // Re-begin same namespace within same process => data still there.
    Preferences c;
    c.begin("a", true);  // readOnly
    EXPECT("readonly_get",     c.getUInt("x", 0) == 1);
    EXPECT("readonly_put_fail", c.putUInt("x", 99) == 0);
    EXPECT("readonly_unchanged", c.getUInt("x", 0) == 1);
    c.end();

    Serial.print("TEST done ");
    Serial.print(g_pass);
    Serial.print("/");
    Serial.println(g_total);
}

void loop() { delay(100); }
