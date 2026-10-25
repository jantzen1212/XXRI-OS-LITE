/*
  Copyright Ⓒ 1997, 1998, 1999, 2000, 2001  joost witteveen
  Copyright Ⓒ 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009  Clint Adams

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#ifndef FAKEROOT_MESSAGE_H
#define FAKEROOT_MESSAGE_H

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
# ifdef HAVE_INTTYPES_H
# include <inttypes.h>
# else
#  error Problem
# endif
#endif

/* On Solaris, use the native htonll(n)/ntohll(n) */
#if !defined(sun) && !defined(_NETINET_IN_H)
#if __BYTE_ORDER == __BIG_ENDIAN
# define htonll(n)  (n)
# define ntohll(n)  (n)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define htonll(n)  ((((uint64_t) htonl(n)) << 32LL) | htonl((n) >> 32LL))
# define ntohll(n)  ((((uint64_t) ntohl(n)) << 32LL) | ntohl((n) >> 32LL))
#endif
#endif /* !defined(sun) && !defined(_NETINET_IN_H) */

/* Endianness-agnostic swappers from byteswap.h */
#define bswaps(x) \
  ((uint16_t) ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8)))
#define bswapl(x) \
  ((((x) & 0xff000000u) >> 24) | (((x) & 0x00ff0000u) >> 8) \
   | (((x) & 0x0000ff00u) << 8) | (((x) & 0x000000ffu) << 24))
#define bswapll(x) \
  ((((x) & 0xff00000000000000ull) >> 56) \
   | (((x) & 0x00ff000000000000ull) >> 40) \
   | (((x) & 0x0000ff0000000000ull) >> 24) \
   | (((x) & 0x000000ff00000000ull) >> 8) \
   | (((x) & 0x00000000ff000000ull) << 8) \
   | (((x) & 0x0000000000ff0000ull) << 24) \
   | (((x) & 0x000000000000ff00ull) << 40) \
   | (((x) & 0x00000000000000ffull) << 56))

#define FAKEROOTKEY_ENV "FAKEROOTKEY"

/* The magic value must be positive itself and be still positive if
 * it becomes a high-dword of an uint64_t. Otherwise msgsnd(2) on x32
 * platform thinks mtype is 64-bit negative number and returns -EINVAL.
 * It should be also asymmetric it is possible to detect the endianness
 * of message
 */
#define FAKEROOT_MAGIC_LE 0x78787878
#define FAKEROOT_MAGIC_BE 0x75757575

typedef uint32_t func_id_t;

typedef uint64_t fake_ino_t;
typedef uint64_t fake_dev_t;
typedef uint32_t fake_uid_t;
typedef uint32_t fake_gid_t;
typedef uint32_t fake_mode_t;
typedef uint32_t fake_nlink_t;

#if __SUNPRO_C
#pragma pack(4)
#endif
struct fakestat {
	fake_uid_t   uid;
	fake_gid_t   gid;
	fake_ino_t   ino;
	fake_dev_t   dev;
	fake_dev_t   rdev;
	fake_mode_t  mode;
	fake_nlink_t nlink;
} FAKEROOT_ATTR(packed);
#if __SUNPRO_C
#pragma pack()
#endif

#define MAX_IPC_BUFFER_SIZE 1024

#if __SUNPRO_C
#pragma pack(4)
#endif
struct fakexattr {
	uint32_t   buffersize;
	char       buf[MAX_IPC_BUFFER_SIZE];
	int32_t    flags_rc; /* flags from setxattr. Return code on round trip */
} FAKEROOT_ATTR(packed);
#if __SUNPRO_C
#pragma pack()
#endif

#if __SUNPRO_C
#pragma pack(4)
#endif
struct fake_msg {
#ifndef FAKEROOT_FAKENET
	uint32_t magic; /* marker to detect cross-architecture mtype fluctuations */
#endif
	func_id_t       id; /* the requested function */
#ifndef FAKEROOT_FAKENET
	pid_t pid;
	int32_t serial;
#endif
	struct fakestat st;
	struct fakexattr xattr;
	uint32_t        remote;
} FAKEROOT_ATTR(packed);
#if __SUNPRO_C
#pragma pack()
#endif

#if __SUNPRO_C
#pragma pack(4)
#endif
struct fake_msg_buf {
#ifndef FAKEROOT_FAKENET
	long mtype; /* message type in SYSV message sending */
#endif
	char msg[sizeof(struct fake_msg)];
} FAKEROOT_ATTR(packed);

#if __SUNPRO_C
#pragma pack()
#endif

#endif
