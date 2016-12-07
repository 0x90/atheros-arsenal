/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Qualcomm Atheros nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __ADF_OS_UTIL_PVT_H
#define __ADF_OS_UTIL_PVT_H

#include <adf_os_types.h>

#define __adf_os_unlikely(_expr)
#define __adf_os_likely(_expr)

/**
 * @brief memory barriers.
 */
#define __adf_os_wmb()          oops no implementation...
#define __adf_os_rmb()          oops no implementation...
#define __adf_os_mb()           oops no implementation...

#define __adf_os_min(_a, _b)	((_a < _b) ? _a : _b)
#define __adf_os_max(_a, _b)    ((_a > _b) ? _a : _b)

#ifdef _DEBUG_BUILD_
#define __adf_os_assert(expr)  do {\
    if(!(expr)) {                                   	\
        adf_os_print("Assertion failed! %s:%s %s:%d\n", #expr, __FUNCTION__, __FILE__, __LINE__);	\
	while(1){}					\
        						\
	}\
}while(0);
#else
#if defined(PROJECT_MAGPIE)
#define __adf_os_assert(expr)  do {      \
    if(!(expr)) {                        \
        adf_os_print("Assertion failed! %s\n", __FUNCTION__);	\
        (*((volatile uint32_t *)(0x12345678)));            \
	}                                    \
}while(0);
#else
#define __adf_os_assert(expr)  do {      \
    if(!(expr)) {                        \
        while(1){}					     \
	}                                    \
}while(0);
#endif
#endif

#ifndef inline
#define inline
#endif

static void inline
__adf_os_get_rand(adf_os_handle_t  hdl,__a_uint8_t *ptr, __a_uint32_t len)
{
#if 0
	u_int8_t *dp = ptr;
	u_int32_t v;
	size_t nb;
	while (len > 0) {
		v = arc4random();
		nb = len > sizeof(u_int32_t) ? sizeof(u_int32_t) : len;
		bcopy(&v, dp, len > sizeof(u_int32_t) ? sizeof(u_int32_t) : len);
		dp += sizeof(u_int32_t);
		len -= nb;
	}
#endif
}


#endif /*_ADF_OS_UTIL_PVT_H*/
