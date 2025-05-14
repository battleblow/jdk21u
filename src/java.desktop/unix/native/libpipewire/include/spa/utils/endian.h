/* Spa */
/* SPDX-FileCopyrightText: Copyright © 2019 Wim Taymans */
/* SPDX-License-Identifier: MIT */

#ifndef SPA_ENDIAN_H
#define SPA_ENDIAN_H

#if defined(_BSDONLY_SOURCE)
#include <sys/endian.h>
#define bswap_16 bswap16
#define bswap_32 bswap32
#define bswap_64 bswap64
#ifndef __BYTE_ORDER
#define __BYTE_ORDER _BYTE_ORDER
#endif
#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#endif
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#endif
#elif defined(_MSC_VER) && defined(_WIN32)
#include <stdlib.h>
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#define bswap_16 _byteswap_ushort
#define bswap_32 _byteswap_ulong
#define bswap_64 _byteswap_uint64
#elif defined(AIX)
#include <sys/machine.h>
#define __BIG_ENDIAN      BIG_ENDIAN
#define __BYTE_ORDER      BIG_ENDIAN
#else
#include <endian.h>
#include <byteswap.h>
#endif

#endif /* SPA_ENDIAN_H */
