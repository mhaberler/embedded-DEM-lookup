#include "util.hpp"

using namespace std;

string string_format(const string fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
    string str;
    va_list ap;
    while (1) {     // Maximum two passes on a POSIX system...
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {  // Everything worked
            str.resize(n);
            return str;
        }
        if (n > -1)  // Needed size returned
            size = n + 1;   // For null char
        else
            size *= 2;      // Guess at a larger size (OS specific)
    }
    return str;
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {

    LOG_INFO("Listing directory: %s", dirname);

    File root = fs.open(dirname);
    if(!root) {
        LOG_INFO("Failed to open directory");
        return;
    }
    if(!root.isDirectory()) {
        LOG_ERROR("Not a directory: %s", dirname);
        return;
    }

    File file = root.openNextFile();
    while(file) {
        if(file.isDirectory()) {
            LOG_INFO("  DIR : %s", file.name());
            if(levels) {
                listDir(fs, file.path(), levels -1);
            }
        } else {
            LOG_INFO("  FILE: %s  size %u", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}
