
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

extern btc_pubkey NSPV_pubkey;  // test

enum WR_INIT_STATE {
    WR_NOT_INITED = 0,
    WR_INITED,
    WR_FINISHING
};

static int init_state = WR_NOT_INITED;

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

//const char dbfile[] = "nlibnspv.dat";
const char dbfile[] = "";  // no db in android

typedef struct _HEXTX {
    char magic[4 + 1];
    char hextxid[64 + 1];
    char *hextx;
} HEXTX;

typedef struct _HEXTX_ARRAY {
    int count;
    HEXTX *txns;
} HEXTX_ARRAY;


// wrapper for NSPV library init
unity_int32_t LIBNSPV_API uplugin_InitNSPV(char *chainName, char *errorStr)
{
    //char chainName[WR_MAXCHAINNAMELEN+1];
    unity_int32_t retcode = 0;

    strcpy(errorStr, "");
    nspv_log_message("%s entering, chainName=%s kogschain ptr=%p", __func__, chainName, kogschain);

    if (init_state != WR_NOT_INITED) {
        strncpy(errorStr, "NSPV already initialized", WR_MAXERRORLEN);
        nspv_log_message("%s exiting error %s", __func__, errorStr);
        return -1;
    }

    if (!kogs_plugin_mutex_init) {
        kogs_plugin_mutex_init = true;
        portable_mutex_init(&kogs_plugin_mutex);
    }
    portable_mutex_lock(&kogs_plugin_mutex);

    //wcscpy(wErrorStr, L"");
    //wcstombs(chainName, wChainName, sizeof(chainName)/sizeof(chainName[0]));

    if (kogschain == NULL) 
    {
        nspv_log_message("%s before NSPV_coinlist_scan, searching chain=%s", __func__, chainName);
        kogschain = NSPV_coinlist_scan(chainName, &kmd_chainparams_main);
        nspv_log_message("%s after NSPV_coinlist_scan, kogschain ptr=%p", __func__, kogschain);

        if (kogschain != NULL && kogschain != (void*)0xFFFFFFFFFFFFFFFFLL)   // TODO: avoid 0xFFFFFFFFFFFFFFFFLL use
        {
            btc_ecc_start();
            kogsclient = btc_spv_client_new(kogschain, true, /*(dbfile && (dbfile[0] == '0' || (strlen(dbfile) > 1 && dbfile[0] == 'n' && dbfile[0] == 'o'))) ? true : false*/true);
            nspv_log_message("%s after btc_spv_client_new, kogsclient ptr=%p", __func__, kogsclient);
            if (kogsclient != NULL)
            {
                if (OS_thread_create(&libthread, NULL, NSPV_rpcloop, (void *)&kogschain->rpcport) != 0)
                {
                    strncpy(errorStr, "error launching NSPV_rpcloop for port", WR_MAXERRORLEN);
                    retcode = -1;
                }
                else
                {
                    //snprintf(errorStr, WR_MAXERRORLEN, "no error, kogschain ptr=%p", kogschain);
                    // wcsncpy(wErrorStr, L"no error", WR_MAXERRORLEN);
                    btc_spv_client_discover_peers(kogsclient, NULL);
                    nspv_log_message("%s discovered nodes &d", __func__, kogsclient->nodegroup->nodes->len);
                    if (kogsclient->nodegroup->nodes->len == 0)
                    {
                        strncpy(errorStr, "no nodes discovered", WR_MAXERRORLEN);
                        retcode = -1;
                    }
                }
            }
            else
            {
                strncpy(errorStr, "cannot create kogsclient", WR_MAXERRORLEN);
                retcode = -1;
            }
        }
        else {
            if (kogschain == (void*)0xFFFFFFFFFFFFFFFFLL)
            {
                strncpy(errorStr, "could not find coins file", WR_MAXERRORLEN);
                kogschain = NULL;
            }
            else
                strncpy(errorStr, "could not find chain in coins file", WR_MAXERRORLEN);
            retcode = -1;
        }
    }
    else 
    {
        strncpy(errorStr, "NSPV already initialized", WR_MAXERRORLEN);
        retcode = -1;
    }

    if (retcode == 0)
        init_state = WR_INITED;
    portable_mutex_unlock(&kogs_plugin_mutex);

    // set test pubkey
    char testpk[] = "034777b18effce6f7a849b72de8e6810bf7a7e050274b3782e1b5a13d0263a44dc";
    int outl;
    utils_hex_to_bin(testpk, NSPV_pubkey.pubkey, strlen(testpk), &outl);
    NSPV_pubkey.compressed = 1;

    nspv_log_message("%s exiting retcode=%d errorStr=%s", __func__, retcode, errorStr);
    return retcode;
}


unity_int32_t LIBNSPV_API uplugin_TxnsCount(void *inPtr, unity_int32_t *pcount, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");

    if (inPtr == NULL) {
        strcpy(errorStr, "inPtr null");
        retcode = -1;
    }
    else {
        HEXTX_ARRAY *phextxns = (HEXTX_ARRAY *)inPtr;
        *pcount = phextxns->count;
    }
    nspv_log_message("%s exiting retcode=%d", __func__, retcode);
    return retcode;
}

// kogslist rpc wrapper
unity_int32_t LIBNSPV_API uplugin_KogsList(void **inPtrPtr, char *errorStr)
{
    cJSON *rpcrequest = cJSON_CreateNull();
    cJSON *rpcresult = NULL;
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    nspv_log_message("%s inPtrPtr=%p", __func__, inPtrPtr);

    rpcresult = NSPV_remoterpccall(kogsclient, "kogskoglist", rpcrequest);
    nspv_log_message("%s rpcresult=%p", __func__, rpcresult);

    strcpy(errorStr, "");
    *inPtrPtr = NULL;

    if (rpcresult == NULL) {
        strcpy(errorStr, "rpc result is null");
        return -1;
    }

    if (cJSON_HasObjectItem(rpcresult, "kogids") &&
        cJSON_IsArray(cJSON_GetObjectItem(rpcresult, "kogids")))  
    {
        HEXTX_ARRAY *phextxns = calloc(1, sizeof(HEXTX_ARRAY));
        phextxns->count = cJSON_GetArraySize(rpcresult);
        phextxns->txns = calloc(phextxns->count, sizeof(HEXTX));
        for (int32_t i = 0; i < phextxns->count; i++) 
        {
            cJSON *item = cJSON_GetArrayItem(rpcresult, i);
            if (cJSON_IsString(item)) {
                // utils_uint256_sethex(item->valuestring, hextxns.txns[i].hextxid);
                strncpy(phextxns->txns[i].hextxid, item->valuestring, sizeof(phextxns->txns[i].hextxid));
                nspv_log_message("%s item->valuestring=%s", __func__, item->valuestring);
            }
            else
            {
                nspv_log_message("uplugin_KogsList json item not string");
            }
        }
        *inPtrPtr = phextxns;
    }
    else
    {
        strcpy(errorStr, "no kogids array returned in rpc result");
        retcode = -1;
    }
    cJSON_Delete(rpcrequest);
    cJSON_Delete(rpcresult);
       
    nspv_log_message("%s exiting retcode=%d %s", __func__, retcode, errorStr);
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
    nspv_log_message("%s enterred", __func__);
    if (init_state != WR_INITED) {
        nspv_log_message("%s: exiting, state not inited", __func__);
        return;
    }

    init_state = WR_FINISHING;

    //if (!kogs_plugin_mutex_init)
    //    return;

    //portable_mutex_lock(&kogs_plugin_mutex);  // seems deadlock if pthread_join
    btc_spv_client_free(kogsclient);
    kogsclient = NULL;
    
    // no pthread_cancel on android:
	// pthread_cancel(libthread);
    NSPV_STOP_RECEIVED = (uint32_t)time(NULL);  // flag to stop thread
    pthread_join(libthread, NULL);

    btc_ecc_stop();
    if (kogschain)
        free((void*)kogschain);
    kogschain = NULL;

//  TODO: don't do this here, use OnDestroy() java callback
//    if (coinsCached)
//        free(coinsCached);  

    //portable_mutex_unlock(&kogs_plugin_mutex);
    init_state = WR_NOT_INITED;
    nspv_log_message("%s exiting", __func__);
}
