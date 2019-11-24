
/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef __KOGSWRAPPER_H__
#define __KOGSWRAPPER_H__

#include <btc/btc.h>
#include <wchar.h>

#define WR_MAXCHAINNAMELEN 64
#define WR_MAXERRORLEN 128
typedef int32_t unity_int32_t;


#ifndef LIBNSPV_API
#if defined(_WIN32)
#ifdef LIBNSPV_BUILD
#define LIBNSPV_API __declspec(dllexport)
#else
#define LIBNSPV_API
#endif
#elif defined(__GNUC__) && defined(LIBNSPV_BUILD)
#define LIBNSPV_API __attribute__((visibility("default")))
#else
#define LIBNSPV_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

    // kogs wrapper functions:
    unity_int32_t LIBNSPV_API uplugin_InitNSPV(char *chainName, char *errorStr);

    unity_int32_t LIBNSPV_API uplugin_KogsList(void **inPtrPtr, char *errorStr);

    void LIBNSPV_API uplugin_free(void *inPtr);
    void LIBNSPV_API uplugin_FinishNSPV();

#ifdef __cplusplus
}
#endif

#endif // #ifndef __KOGSWRAPPER_H__