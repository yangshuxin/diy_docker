#ifndef __CLONE_EXEC_HPP__
#define __CLONE_EXEC_HPP__

int clone_exec(int clone_flags, const char* argv, int (*child_fn)(void*));

#endif
