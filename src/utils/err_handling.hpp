#ifndef __ERR_HANDLING_HPP__
#define __ERR_HANDLING_HPP__

#include <string>

#include "err_handling.hpp"

void ExitOnErr(const char* msg, int err_no, int exit_code) __attribute__((noreturn));
static inline void __attribute__((noreturn))
ExitOnErr(const std::string& msg, int err_no, int exit_code) {
    ExitOnErr(msg.c_str(), err_no, exit_code);
}

void assert_func(const char* file_name, int line_num,
                 const char* cond, const char* fmt, ...);

std::string FmtErrno(const char* msg, int err_no);

#define ASSERT(cond, msg_fmt, ...) if (!(cond)) { \
    assert_func(__FILE__, __LINE__, #cond, msg_fmt, ##__VA_ARGS__); }

#define ASSERT0(cond) if (!(cond)) { \
    assert_func(__FILE__, __LINE__, #cond, 0); }

#endif
