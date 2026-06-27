#pragma once
#include <stdio.h>
#include <string.h>
#include <string>

// File — thin wrapper around POSIX FILE* for native test builds
class File {
public:
    File() : _fp(nullptr) {}
    explicit File(FILE* fp) : _fp(fp) {}

    operator bool() const { return _fp != nullptr; }

    size_t read(uint8_t* buf, size_t len) {
        return _fp ? fread(buf, 1, len, _fp) : 0;
    }

    int read() {
        if (!_fp) return -1;
        int c = fgetc(_fp);
        return (c == EOF) ? -1 : c;
    }

    bool available() {
        if (!_fp) return false;
        return !feof(_fp);
    }

    void close() {
        if (_fp) { fclose(_fp); _fp = nullptr; }
    }

    size_t size() {
        if (!_fp) return 0;
        long pos = ftell(_fp);
        fseek(_fp, 0, SEEK_END);
        long sz = ftell(_fp);
        fseek(_fp, pos, SEEK_SET);
        return (size_t)sz;
    }

private:
    FILE* _fp;
};

// Minimal FS stub backed by POSIX filesystem
// Fixture JSON files live in test/fixtures/ and are accessed by name (no leading /).
class FS {
public:
    bool begin(bool) { return true; }
    bool begin() { return true; }

    // Base path used by tests — set before calling loadFromJson()
    const char* basePath = "test/fixtures";

    bool exists(const char* path) {
        std::string full = _fullPath(path);
        FILE* fp = fopen(full.c_str(), "r");
        if (fp) { fclose(fp); return true; }
        return false;
    }

    File open(const char* path, const char* mode = "r") {
        std::string full = _fullPath(path);
        FILE* fp = fopen(full.c_str(), mode);
        return File(fp);
    }

private:
    std::string _fullPath(const char* path) const {
        // Strip leading '/' and prefix with basePath
        const char* name = (path && path[0] == '/') ? path + 1 : path;
        return std::string(basePath) + "/" + name;
    }
};

extern FS LittleFS;
