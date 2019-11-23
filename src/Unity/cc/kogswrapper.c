
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

#include <stdlib.h> 
#include <btc/net.h>
#include <btc/netspv.h>
#include <btc/utils.h>
#include <btc/ecc.h>

#include "nSPV_defs.h"
#include "kogswrapper.h"



//void nspv_log_message(char *format, ...);

btc_chainparams kmd_chainparams_main;
extern char *coinsCached;

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

static int kogs_plugin_mutex_init = false;
static portable_mutex_t kogs_plugin_mutex;
static const btc_chainparams *kogschain = NULL;
static btc_spv_client* kogsclient = NULL;
static pthread_t libthread;

const char dbfile[] = "nlibnspv.dat";

// wrapper for NSPV library init
unity_int32_t LIBNSPV_API uplugin_InitNSPV(wchar_t *wChainName, wchar_t *wErrorStr)
{
    char chainName[WR_MAXCHAINNAMELEN+1];
    unity_int32_t rc = 0;

    if (!kogs_plugin_mutex_init) {
        kogs_plugin_mutex_init = true;
        portable_mutex_init(&kogs_plugin_mutex);
    }
    portable_mutex_lock(&kogs_plugin_mutex);

    wcscpy(wErrorStr, L"");
    wcstombs(chainName, wChainName, sizeof(chainName)/sizeof(chainName[0]));

    nspv_log_message("entering, kogschain ptr=%p wChainName=%S", kogschain, wChainName);

    if (kogschain == NULL) 
    {
        nspv_log_message("before NSPV_coinlist_scan, searching chain=%s", chainName);
        kogschain = NSPV_coinlist_scan(chainName, &kmd_chainparams_main);
        nspv_log_message("after NSPV_coinlist_scan, kogschain=%p", kogschain);
        if (kogschain != NULL && kogschain != 0xFFFFFFFFFFFFFFFFLL)   // TODO: avoid 0xFFFFFFFFFFFFFFFFLL use
        {
            btc_ecc_start();
            btc_spv_client* client = btc_spv_client_new(kogschain, true, (dbfile && (dbfile[0] == '0' || (strlen(dbfile) > 1 && dbfile[0] == 'n' && dbfile[0] == 'o'))) ? true : false);
            NSPV_client = client;

            if (OS_thread_create(&libthread, NULL, NSPV_rpcloop, (void *)&kogschain->rpcport) != 0)
            {
                wcsncpy(wErrorStr, L"error launching NSPV_rpcloop for port", WR_MAXERRORLEN);
                rc = -1;
            } 
            else 
            {
                swprintf(wErrorStr, WR_MAXERRORLEN, L"no error, kogschain=%p", kogschain);
                // wcsncpy(wErrorStr, L"no error", WR_MAXERRORLEN);
            }
        }
        else {
            if (kogschain == 0xFFFFFFFFFFFFFFFFLL)
            {
                wcsncpy(wErrorStr, L"could not find coins file", WR_MAXERRORLEN);
                kogschain = NULL;
            }
            else
                wcsncpy(wErrorStr, L"could not find chain in coins file", WR_MAXERRORLEN);
            rc = -1;
        }
    }
    else 
    {
        wcsncpy(wErrorStr, L"NSPV already initialized", WR_MAXERRORLEN);
        rc = -1;
    }

    portable_mutex_unlock(&kogs_plugin_mutex);
    return rc;
}

// kogslist rpc wrapper
unity_int32_t LIBNSPV_API uplugin_KogsList(uint256 **plist, int32_t *pcount, wchar_t *wErrorStr)
{
    cJSON *request = cJSON_CreateNull();
    cJSON *result = NSPV_remoterpccall(kogsclient, "kogslist", request);
    unity_int32_t retcode = 0;

    if (cJSON_HasObjectItem(result, "kogids"))
    {
        wcscpy(wErrorStr, L"");
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
        wcscpy(wErrorStr, L"no kogids array returned");
        retcode = -1;
    }
    cJSON_Delete(request);
    cJSON_Delete(result);
    return retcode;
}

// free mem allocated by wrapper
void LIBNSPV_API uplugin_free(void *ptr)
{
    free(ptr);
}

// finish NSPV library
void LIBNSPV_API uplugin_FinishNSPV()
{
    if (!kogs_plugin_mutex_init)
        return;

    portable_mutex_lock(&kogs_plugin_mutex);
    btc_spv_client_free(kogsclient);
    
    // no pthread_cancel on android:
	// pthread_cancel(libthread);
    NSPV_STOP_RECEIVED = time(NULL);  // flag to stop thread
    pthread_join(libthread, NULL);

    btc_ecc_stop();
    if (kogschain)
        free(kogschain);
    kogschain = NULL;
    if (coinsCached)
        free(coinsCached);
    portable_mutex_unlock(&kogs_plugin_mutex);

}
