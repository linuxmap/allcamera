#ifndef _SVS_ADT_H
#define _SVS_ADT_H


#ifdef WIN32

/* basic types
 * */
typedef char               int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef __int64            int64_t;
typedef unsigned __int64   uint64_t;

#else // WIN32

#include <sys/types.h>

#ifndef uint32_t
typedef u_int32_t          uint32_t;
#endif

#ifndef uint64_t
typedef u_int64_t          uint64_t;
#endif

#ifndef uint16_t
typedef u_int16_t          uint16_t;
#endif

#ifndef uint8_t
typedef unsigned char      uint8_t;
#endif

#endif // WIN32

#ifndef WIN32

typedef uint32_t           DWORD;
typedef uint16_t           WORD;
typedef uint8_t            BYTE;
typedef bool               BOOL;

#ifndef TRUE
#define TRUE               true
#define FALSE              false
#endif

#endif

typedef uint32_t routerid_t, streamid_t, recordid_t;


#endif //_SVS_ADT_H


