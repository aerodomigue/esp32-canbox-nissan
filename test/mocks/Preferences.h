#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

// Persistent backend separate from ConfigManager's RAM. configInit() simulates
// a reboot without clearing this state. Writes remain observable per key.
struct FakeNvs {
    static inline bool exists = false;
    static inline std::map<std::string, int64_t> numbers;
    static inline std::map<std::string, std::string> strings;
    static inline std::map<std::string, unsigned> writes;

    static void reset() {
        exists = false;
        numbers.clear();
        strings.clear();
        writes.clear();
    }
};

class Preferences {
    bool opened = false;
    bool readOnly = false;

    template <typename T> T get(const char* key, T fallback) {
        assert(opened);
        const auto it = FakeNvs::numbers.find(key);
        return it == FakeNvs::numbers.end() ? fallback : static_cast<T>(it->second);
    }

    template <typename T> size_t put(const char* key, T value) {
        assert(opened && !readOnly);
        FakeNvs::numbers[key] = value;
        ++FakeNvs::writes[key];
        return sizeof(T);
    }

public:
    bool begin(const char* name, bool ro = false) {
        assert(!opened && std::string(name) == "canbox");
        readOnly = ro;
        opened = !ro || FakeNvs::exists;
        if (opened) FakeNvs::exists = true;
        return opened;
    }
    void end() { opened = false; }
    bool clear() {
        assert(opened && !readOnly);
        FakeNvs::numbers.clear();
        FakeNvs::strings.clear();
        return true;
    }
    bool isKey(const char* key) {
        assert(opened);
        return FakeNvs::numbers.count(key) || FakeNvs::strings.count(key);
    }

    int16_t getShort(const char* k, int16_t d = 0) { return get(k, d); }
    uint16_t getUShort(const char* k, uint16_t d = 0) { return get(k, d); }
    uint8_t getUChar(const char* k, uint8_t d = 0) { return get(k, d); }
    bool getBool(const char* k, bool d = false) { return get(k, d); }
    size_t putShort(const char* k, int16_t v) { return put(k, v); }
    size_t putUShort(const char* k, uint16_t v) { return put(k, v); }
    size_t putUChar(const char* k, uint8_t v) { return put(k, v); }
    size_t putBool(const char* k, bool v) { return put(k, v); }

    size_t putString(const char* key, const char* value) {
        assert(opened && !readOnly);
        FakeNvs::strings[key] = value;
        ++FakeNvs::writes[key];
        return strlen(value);
    }
    size_t getString(const char* key, char* dst, size_t size) {
        assert(opened);
        const auto it = FakeNvs::strings.find(key);
        if (it == FakeNvs::strings.end() || it->second.size() + 1 > size) return 0;
        memcpy(dst, it->second.c_str(), it->second.size() + 1);
        return it->second.size() + 1;
    }
};
