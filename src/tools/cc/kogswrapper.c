
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

#include <btc/utils.h>
#include "kogswrapper.h"
#include "nSPV_defs.h"

btc_chainparams kmd_chainparams_main;

/*
static btc_chainparams kogs_chainparams =
{
    "DIMXY11",
    60,
    85,
    "bc", // const char bech32_hrp[5]
    188,
    0x0488ADE4, // uint32_t b58prefix_bip32_privkey
    0x0488B21E, // uint32_t b58prefix_bip32_pubkey
    { 0xf9, 0xee, 0xe4, 0x8d },
    { 0x02, 0x7e, 0x37, 0x58, 0xc3, 0xa6, 0x5b, 0x12, 0xaa, 0x10, 0x46, 0x46, 0x2b, 0x48, 0x6d, 0x0a, 0x63, 0xbf, 0xa1, 0xbe, 0xae, 0x32, 0x78, 0x97, 0xf5, 0x6c, 0x5c, 0xfb, 0x7d, 0xaa, 0xae, 0x71 }, //{0x6f, 0xe2, 0x8c, 0x0a, 0xb6, 0xf1, 0xb3, 0x72, 0xc1, 0xa6, 0xa2, 0x46, 0xae, 0x63, 0xf7, 0x4f, 0x93, 0x1e, 0x83, 0x65, 0xe1, 0x5a, 0x08, 0x9c, 0x68, 0xd6, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00},
    14722,14723,
    { { "172.31.5.51" }, 0 },
    60,
    170007,
    MAX_TX_SIZE_AFTER_SAPLING,
    1,1,0,
};
*/

static const btc_chainparams *kogschain = NULL;
pthread_t libthread;

int32_t __stdcall LIBNSPV_API LibNSPVSetup(char *chainname, char *errorstr)
{
    strcpy(errorstr, "");
    kogschain = NSPV_coinlist_scan(chainname, &kmd_chainparams_main);
    if (kogschain == NULL) {
        strncpy(errorstr, "could not find chain", 128);
        return -1;
    }
    
    if (OS_thread_create(&libthread, NULL, NSPV_rpcloop, (void *)&kogschain->rpcport) != 0)
    {
        strncpy(errorstr, "error launching NSPV_rpcloop for port", 128);
        return -1;
    }
    return 0;
}


int32_t __stdcall LIBNSPV_API CCKogsList(uint256 **plist, int32_t *pcount, char *errorstr)
{
    cJSON *request = cJSON_CreateNull();
    cJSON *result = NSPV_remoterpccall(NULL, "kogslist", request);
    int32_t retcode = 0;

    if (cJSON_HasObjectItem(result, "kogids"))
    {
        strcpy(errorstr, "");
        if (cJSON_IsArray(cJSON_GetObjectItem(result, "kogids")))  {
            *pcount = cJSON_GetArraySize(result);
            *plist = malloc(sizeof(uint256) * (*pcount));
            for (int32_t i = 0; i < *pcount; i++) {
                cJSON *item = cJSON_GetArrayItem(result, i);
                if (cJSON_IsString(item))
                    utils_uint256_sethex(item->valuestring, (*plist)[i]);
            }
        }
    }
    else
    {
        strcpy(errorstr, "no kogids array returned");
        retcode = -1;
    }
    cJSON_Delete(request);
    cJSON_Delete(result);
    return retcode;
}

void __stdcall LIBNSPV_API CCWrapperFree(void *ptr)
{
    free(ptr);
}


void __stdcall LIBNSPV_API LibNSPVFinish()
{
    pthread_join(libthread, NULL);
}
