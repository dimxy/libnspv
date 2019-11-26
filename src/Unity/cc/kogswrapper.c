
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

static int init_state = WR_NOT_INITED;

//void nspv_log_message(char *format, ...);
extern btc_pubkey NSPV_pubkey;  // test

btc_chainparams kmd_chainparams_template =
{
    "KMD",
    60,
    85,
    "bc", // const char bech32_hrp[5]
    188,
    0x0488ADE4, // uint32_t b58prefix_bip32_privkey
    0x0488B21E, // uint32_t b58prefix_bip32_pubkey
    { 0xf9, 0xee, 0xe4, 0x8d },
    { 0x02, 0x7e, 0x37, 0x58, 0xc3, 0xa6, 0x5b, 0x12, 0xaa, 0x10, 0x46, 0x46, 0x2b, 0x48, 0x6d, 0x0a, 0x63, 0xbf, 0xa1, 0xbe, 0xae, 0x32, 0x78, 0x97, 0xf5, 0x6c, 0x5c, 0xfb, 0x7d, 0xaa, 0xae, 0x71 }, //{0x6f, 0xe2, 0x8c, 0x0a, 0xb6, 0xf1, 0xb3, 0x72, 0xc1, 0xa6, 0xa2, 0x46, 0xae, 0x63, 0xf7, 0x4f, 0x93, 0x1e, 0x83, 0x65, 0xe1, 0x5a, 0x08, 0x9c, 0x68, 0xd6, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00},
    7770,7771,
    { { "5.9.102.210, 45.32.19.196, 5.9.253.195, 78.47.196.146, 23.254.165.16, 136.243.58.134, 5.9.253.196, 5.9.253.197, 5.9.253.198, 5.9.253.199, 5.9.253.200, 5.9.253.201, 5.9.253.202, 5.9.253.203" }, 0 },
    60,
    170007,
    MAX_TX_SIZE_AFTER_SAPLING,
    1,1,0,
};

extern char *coinsCached;

static int kogs_plugin_mutex_init = false;
static portable_mutex_t kogs_plugin_mutex;
static const btc_chainparams *kogschain = NULL;
static btc_spv_client* kogsclient = NULL;
static pthread_t libthread;

//const char dbfile[] = "nlibnspv.dat";
const char dbfile[] = "";  // no db in android

/*
#define HTXID_LEN 64 
const char HTXN_MAGIC[] = "HTXN";

typedef struct _HEXTX {
    char hextxid[HTXID_LEN + 1];
    char *hextx;
} HEXTX;

typedef struct _HEXTX_ARRAY {
    char magic[4 + 1];
    int count;
    HEXTX *txns;
} HEXTX_ARRAY;
*/

static void *run_spv_event_loop(btc_spv_client* client)
{
    btc_spv_client_runloop(client);
    return NULL;
}

// helpers:

// check cJSON result from libnspv
static btc_bool check_jresult(cJSON *jmsg, char *error)
{
    strcpy(error, "");
    if (jmsg)
    {
        // check if nspv level error exists
        if (cJSON_HasObjectItem(jmsg, "error") &&
            cJSON_GetObjectItem(jmsg, "error")->valuestring != NULL)
        {
            // add nspv error:
            snprintf(error, WR_MAXCCERRORLEN, "nspv-error: %s", cJSON_GetObjectItem(jmsg, "error")->valuestring);
            return false;
        }
        
        if (cJSON_HasObjectItem(jmsg, "result"))
        { 
            // get app data container
            cJSON *jccresult = cJSON_GetObjectItem(jmsg, "result");

            // check if cc level error exists
            if (cJSON_HasObjectItem(jccresult, "error") &&
                cJSON_GetObjectItem(jccresult, "error")->valuestring != NULL)
            {
                snprintf(error, WR_MAXCCERRORLEN, "cc-error: %s", cJSON_GetObjectItem(jccresult, "error")->valuestring);
                return false;
            }
            // application result exists and error is empty
            return true;
        }
        strcpy(error, "no result object");
    }
    else {
        strcpy(error, "null");
    }
    return false;
}

/*
// create new txns struct
static HEXTX_ARRAY *create_new_hextxns()
{
    HEXTX_ARRAY *phextxns = calloc(1, sizeof(HEXTX_ARRAY));
    strcpy(phextxns->magic, HTXN_MAGIC);
    return phextxns;
}

// check if pointer to valid txns struct
static int check_hextxns(const HEXTX_ARRAY *phextxns)
{
    if (phextxns && strcmp(phextxns->magic, HTXN_MAGIC) == 0)
        return true;
    else
        return false;
}*/

// helpers end
// helpers end


// wrapper for NSPV library init
unity_int32_t LIBNSPV_API uplugin_InitNSPV(char *chainName, char *errorStr)
{
    //char chainName[WR_MAXCHAINNAMELEN+1];
    unity_int32_t retcode = 0;

    strcpy(errorStr, "");
    nspv_log_message("%s entering, chainName=%s kogschain ptr=%p", __func__, chainName, kogschain);

    if (init_state != WR_NOT_INITED) {
        strncpy(errorStr, "NSPV already initialized", WR_MAXERRORLEN);
        nspv_log_message("%s exiting with error %s", __func__, errorStr);
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
        kogschain = NSPV_coinlist_scan(chainName, &kmd_chainparams_template);
        nspv_log_message("%s after NSPV_coinlist_scan, kogschain ptr=%p", __func__, kogschain);

        if (kogschain != NULL && kogschain != (void*)0xFFFFFFFFFFFFFFFFLL)   // TODO: avoid 0xFFFFFFFFFFFFFFFFLL use
        {
            btc_ecc_start();
            kogsclient = btc_spv_client_new(kogschain, true, /*(dbfile && (dbfile[0] == '0' || (strlen(dbfile) > 1 && dbfile[0] == 'n' && dbfile[0] == 'o'))) ? true : false*/true);
            nspv_log_message("%s after btc_spv_client_new, kogsclient ptr=%p", __func__, kogsclient);
            if (kogsclient != NULL)
            {

                btc_spv_client_discover_peers(kogsclient, NULL);
                nspv_log_message("%s discovered nodes %d", __func__, kogsclient->nodegroup->nodes->len);
                if (kogsclient->nodegroup->nodes->len == 0)
                {
                    strncpy(errorStr, "no nodes discovered", WR_MAXERRORLEN);
                    retcode = -1;
                }
                else
                {
                    //if (OS_thread_create(&libthread, NULL, NSPV_rpcloop, (void *)&kogschain->rpcport) != 0)
                    if (OS_thread_create(&libthread, NULL, run_spv_event_loop, (void *)kogsclient) != 0)  // periodically connect nodes and process responses
                    {
                        strncpy(errorStr, "error launching NSPV_rpcloop for port", WR_MAXERRORLEN);
                        retcode = -1;
                    }
                    else
                    {
                        //snprintf(errorStr, WR_MAXERRORLEN, "no error, kogschain ptr=%p", kogschain);
                        // wcsncpy(wErrorStr, L"no error", WR_MAXERRORLEN);
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
    //char testpk[] = "034777b18effce6f7a849b72de8e6810bf7a7e050274b3782e1b5a13d0263a44dc";
    //int outl;
    //utils_hex_to_bin(testpk, NSPV_pubkey.pubkey, strlen(testpk), &outl);
    //NSPV_pubkey.compressed = 1;

    nspv_log_message("%s exiting retcode=%d errorStr=%s", __func__, retcode, errorStr);
    return retcode;
}


// kogslist rpc wrapper
unity_int32_t LIBNSPV_API uplugin_LoginNSPV(char *wifStr, char *errorStr)
{
    unity_int32_t retcode = 0;
    char cc_error[WR_MAXCCERRORLEN];

    if (!kogs_plugin_mutex_init) {
        strcpy(errorStr, "not inited");
        return -1;
    }
    if (init_state != WR_INITED) {
        strncpy(errorStr, "LibNSPV not initialized", WR_MAXERRORLEN);
        nspv_log_message("%s %s", __func__, errorStr);
        return -1;
    }

    portable_mutex_lock(&kogs_plugin_mutex);
    strcpy(errorStr, "");

    cJSON *jresult = NSPV_login(kogschain, wifStr);
    if (!check_jresult(jresult, cc_error)) {
        snprintf(errorStr, WR_MAXCCERRORLEN, "could not login to LibNSPV %s", cc_error);
        retcode = -1;
    }

    if (jresult)
        cJSON_Delete(jresult);

    portable_mutex_unlock(&kogs_plugin_mutex);
    return retcode;
}


// return string length in string object
unity_int32_t LIBNSPV_API uplugin_StringLength(void *inPtr, unity_int32_t *plen, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");

    if (inPtr != NULL)
    {
        *plen = strlen((char*)inPtr);
    }
    else
    {
        strncpy(errorStr, "inPtr invalid", WR_MAXERRORLEN);
        retcode = -1;
    }
    nspv_log_message("%s exiting retcode=%d", __func__, retcode);
    return retcode;
}

// return string length in string object
unity_int32_t LIBNSPV_API uplugin_GetString(void *inPtr, char *pStr, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");

    if (inPtr != NULL)
    {
        strcpy(pStr, (char*)inPtr);
    }
    else
    {
        strncpy(errorStr, "inPtr invalid", WR_MAXERRORLEN);
        retcode = -1;
    }
    nspv_log_message("%s exiting retcode=%d", __func__, retcode);
    return retcode;
}


/*
// access element count in HEXTX object
unity_int32_t LIBNSPV_API uplugin_TxnsCount(void *inPtr, unity_int32_t *pcount, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");

    if (check_hextxns(inPtr)) 
    {
        HEXTX_ARRAY *phextxns = (HEXTX_ARRAY *)inPtr;
        *pcount = phextxns->count;
    }
    else
    {
        strncpy(errorStr, "inPtr invalid", WR_MAXERRORLEN);
        retcode = -1;
    }
    nspv_log_message("%s exiting retcode=%d", __func__, retcode);
    return retcode;
}

// get a txid from hextxns struct
unity_int32_t LIBNSPV_API uplugin_GetTxid(void *inPtr, unity_int32_t index, char *txidout, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");

    if (check_hextxns(inPtr)) 
    {
        HEXTX_ARRAY *phextxns = (HEXTX_ARRAY *)inPtr;
        if (index >= 0 && index < phextxns->count)  {
            strncpy(txidout, phextxns->txns[index].hextxid, HTXID_LEN);
            txidout[HTXID_LEN] = '\0';
        }
        else {
            strncpy(errorStr, "index out of range", WR_MAXERRORLEN);
            retcode = -1;
        }
    }
    else    {
        strncpy(errorStr, "inPtr invalid", WR_MAXERRORLEN);
        retcode = -1;
    }
    
    nspv_log_message("%s exiting retcode=%d", __func__, retcode);
    return retcode;
}

// get a hex tx len from hextxns struct
unity_int32_t LIBNSPV_API uplugin_GetTxSize(void *inPtr, unity_int32_t index, unity_int32_t *txSize, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");

    if (check_hextxns(inPtr))
    {
        HEXTX_ARRAY *phextxns = (HEXTX_ARRAY *)inPtr;
        if (index >= 0 && index < phextxns->count)
        {
            if (phextxns->txns[index].hextx)   {
                *txSize = (unity_int32_t)strlen(phextxns->txns[index].hextx);
            }
            else   {
                strncpy(errorStr, "tx is null", WR_MAXERRORLEN);
                retcode = -1;
            }
        }
        else {
            strncpy(errorStr, "index out of range", WR_MAXERRORLEN);
            retcode = -1;
        }
    }
    else {
        strncpy(errorStr, "inPtr invalid", WR_MAXERRORLEN);
        retcode = -1;
    }

    nspv_log_message("%s exiting retcode=%d", __func__, retcode);
    return retcode;
}

// get a hex tx into char buf from hextxns struct
unity_int32_t LIBNSPV_API uplugin_GetTx(void *inPtr, unity_int32_t index, char *tx, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");

    if (check_hextxns(inPtr))
    {
        HEXTX_ARRAY *phextxns = (HEXTX_ARRAY *)inPtr;
        if (index >= 0 && index < phextxns->count)
        {
            if (phextxns->txns[index].hextx)   {
                strcpy(tx, phextxns->txns[index].hextx);
            }
            else   {
                strncpy(errorStr, "tx is null", WR_MAXERRORLEN);
                retcode = -1;
            }
        }
        else {
            strncpy(errorStr, "index out of range", WR_MAXERRORLEN);
            retcode = -1;
        }
    }
    else {
        strncpy(errorStr, "inPtr invalid", WR_MAXERRORLEN);
        retcode = -1;
    }

    nspv_log_message("%s exiting retcode=%d", __func__, retcode);
    return retcode;
}
*/

// kogslist rpc wrapper
unity_int32_t LIBNSPV_API uplugin_CallMethod(char *method, char *params, void **resultPtrPtr, char *errorStr)
{
    char cc_error[WR_MAXCCERRORLEN];

    nspv_log_message("%s enterred", __func__);
    nspv_log_message("%s resultPtrPtr=%p", __func__, resultPtrPtr);

    if (!kogs_plugin_mutex_init) {
        strncpy(errorStr, "not inited", WR_MAXERRORLEN);
        return -1;
    }

    if (method == NULL) {
        strncpy(errorStr, "method is null", WR_MAXERRORLEN);
        return -1;
    }

    cJSON *jrpcrequest = cJSON_CreateObject();
    cJSON *jparams = NULL; 
    if (params && strlen(params) > 0) {  // if params set
        jparams = cJSON_Parse(params);
        if (jparams == NULL) {
            strncpy(errorStr, "could not parse params", WR_MAXERRORLEN);
            return -1;
        }

        if (jparams != NULL && !cJSON_IsArray(jparams)) {
            strncpy(errorStr, "params not an array", WR_MAXERRORLEN);
            return -1;
        }
    }

    cJSON *jrpcresult = NULL;
    unity_int32_t retcode = 0;

    jaddstr(jrpcrequest, "method", method);
    if (jparams)
        jadd(jrpcrequest, params, jparams); // add params if it is parsed array

    portable_mutex_lock(&kogs_plugin_mutex);
    jrpcresult = NSPV_remoterpccall(kogsclient, method, jrpcrequest);
    nspv_log_message("%s rpcresult ptr=%p", __func__, jrpcresult);
    portable_mutex_unlock(&kogs_plugin_mutex);

    strcpy(errorStr, "");
    *resultPtrPtr = NULL;

    if (check_jresult(jrpcresult, cc_error))
    {
        cJSON *jresult = cJSON_GetObjectItem(jrpcresult, "result");
        if (jresult != NULL)
        {
/*          cJSON *jkogids = cJSON_GetObjectItem(jresult, "kogids");

            if (jkogids && cJSON_IsArray(jkogids))
            {
                HEXTX_ARRAY *phextxns = create_new_hextxns();
                phextxns->count = cJSON_GetArraySize(jkogids);
                phextxns->txns = calloc(phextxns->count, sizeof(HEXTX));
                for (int32_t i = 0; i < phextxns->count; i++)
                {
                    cJSON *item = cJSON_GetArrayItem(jkogids, i);
                    if (cJSON_IsString(item)) 
                    {
                        // utils_uint256_sethex(item->valuestring, hextxns.txns[i].hextxid);
                        strncpy(phextxns->txns[i].hextxid, item->valuestring, sizeof(phextxns->txns[i].hextxid));
                        nspv_log_message("%s item->valuestring=%s", __func__, item->valuestring);
                    }
                    else
                    {
                        nspv_log_message("%s json array item not string", __func__);
                    }
                }
                *inPtrPtr = phextxns;
                
            }
            else
            {
                strncpy(errorStr, "no kogids array returned in rpc result", WR_MAXERRORLEN);
                retcode = -1;
            } */
            *resultPtrPtr = cJSON_Print(jresult);
            nspv_log_message("%s json result=%s", __func__, (char*)*resultPtrPtr);
        }
        else
        {
            strncpy(errorStr, "no 'result' item in rpc result", WR_MAXERRORLEN);
            retcode = -1;
        }
        cJSON_Delete(jrpcresult);
    }
    else {
        strncpy(errorStr, "rpc result is null", WR_MAXERRORLEN);
        retcode = -1;
    }
    cJSON_Delete(jrpcrequest);
   
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
    // NSPV_STOP_RECEIVED = (uint32_t)time(NULL);  // flag to stop thread
    // pthread_join(libthread, NULL);
    pthread_kill(libthread, 0);

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
