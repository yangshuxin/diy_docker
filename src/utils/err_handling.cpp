#include <string.h>

#include <cstdlib>
#include <iostream>
#include <stdarg.h>
#include <sstream>

#include "err_handling.hpp"

using namespace std;


string
FmtErrno(const char* msg, int err_no) {
    ostringstream ss;
    if (msg) { ss << msg; }
    ss << ": ";
    if (err_no) {
        char err_buf[256];
        const char* err_str = strerror_r(err_no, err_buf, sizeof(err_buf));
        ss << err_str;
    }

    return ss.str();
}

void __attribute__((noreturn))
ExitOnErr(const char* msg, int err_no, int exit_code) {
    auto err_msg = FmtErrno(msg, err_no);
    cerr << err_msg << endl;
    ::exit(exit_code);
}

void
assert_func(const char* file_name, int line_num, const char* cond,
            const char* fmt, ...) {

    cout << "# " << file_name << ":" << line_num << " " << cond << endl;
    cout << "# condition evaluated to false:" << cond << endl;

    if (!fmt) return;

    char buf[500];
    va_list vl;
    va_start(vl, fmt);
    vsnprintf(buf, sizeof(buf), fmt, vl);
    va_end(vl);

    cout << "# " << (char*)buf << endl;

    exit(-1);
}
