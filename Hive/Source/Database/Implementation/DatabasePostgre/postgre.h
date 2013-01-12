#pragma once

#ifdef WIN32
#ifdef FD_SETSIZE
#undef FD_SETSIZE
#endif
#define FD_SETSIZE 1024
#include <winsock2.h>
#include <postgre/libpq-fe.h>
#else
#include <libpq-fe.h>
#endif