// Pre-standard <fstream.h> as used by MSVC 6 code.
// - ios::nocreate / filebuf::openprot / setmode(filebuf::binary) accepted
// - Windows-style relative paths ("Data\\x") mapped onto the virtual filesystem
// - Text-mode input behaves like the MSVC CRT: CRLF -> LF and Ctrl-Z ends the stream
#ifndef D2WEB_FSTREAM_H
#define D2WEB_FSTREAM_H
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <windows.h>

namespace d2io {
enum { nocreate = 0x1000, noreplace = 0x2000 };
std::string text_from_raw(const std::string &raw);
bool read_file(const char *winPath, std::string &out);
void mark_dirty();
}

class ios : public std::ios {
public:
    enum { nocreate = d2io::nocreate, noreplace = d2io::noreplace };
};
struct filebuf {
    enum { openprot = 0, text = 0x4000, binary = 0x8000 };
};

class ifstream : public std::istream {
    std::stringbuf sb;
    std::string raw;
    bool opened;
    bool consumedAny() { return tellg() > 0; }
public:
    ifstream() : std::istream(&sb), opened(false) {}
    explicit ifstream(const char *name, int m = std::ios::in, int prot = 0) : std::istream(&sb), opened(false) { open(name, m, prot); }
    void open(const char *name, int m = std::ios::in, int = 0) {
        clear();
        opened = d2io::read_file(name, raw);
        if (!opened) { raw.clear(); sb.str(""); setstate(std::ios::failbit); return; }
        sb.str((m & std::ios::binary) ? raw : d2io::text_from_raw(raw));
    }
    int setmode(int mode) {
        // Only used right after open in this code base; switch the view before reading.
        if (opened && !consumedAny()) sb.str(mode == filebuf::binary ? raw : d2io::text_from_raw(raw));
        return 0;
    }
    bool is_open() const { return opened; }
    void close() { opened = false; }
    using std::istream::read;
    ifstream &read(unsigned char *p, std::streamsize n) { std::istream::read((char *)p, n); return *this; }
    ifstream &read(signed char *p, std::streamsize n) { std::istream::read((char *)p, n); return *this; }
};

class ofstream : public std::ofstream {
public:
    ofstream() {}
    explicit ofstream(const char *name, int m = std::ios::out, int prot = 0) { open(name, m, prot); }
    void open(const char *name, int m = std::ios::out, int = 0) {
        char p[MAX_PATH];
        std::ofstream::open(d2_path(name, p, sizeof p), (std::ios::openmode)((m & ~(d2io::nocreate | d2io::noreplace)) & 0xFFF) | std::ios::out);
        d2io::mark_dirty();
    }
    int setmode(int) { return 0; }  // no newline translation on this platform; reads mirror it
    void close() { std::ofstream::close(); d2io::mark_dirty(); }
    ~ofstream() { d2io::mark_dirty(); }
    using std::ofstream::write;
    ofstream &write(const unsigned char *p, std::streamsize n) { std::ofstream::write((const char *)p, n); return *this; }
    ofstream &write(const signed char *p, std::streamsize n) { std::ofstream::write((const char *)p, n); return *this; }
};

using std::endl;
using std::ends;
using std::flush;
using std::ostream;
using std::istream;
#endif
