
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
static cJSON *check_jresult(cJSON *json, char *error)
{
    strcpy(error, "");
    if (json)
    {
        // check if nspv level error exists
        if (cJSON_HasObjectItem(json, "error"))
        {
            cJSON * jerror = cJSON_GetObjectItem(json, "error");
            if (cJSON_IsObject(jerror)) {
                // if like this:  { "error" : { "code" : -30020, "message" : "some-error-message" } }
                cJSON * jmessage = cJSON_GetObjectItem(jerror, "message");

                // add cc error:
                snprintf(error, WR_MAXCCERRORLEN, "cc-error: %s\n", (jmessage ? jmessage->valuestring : "null"));
                error[WR_MAXCCERRORLEN - 1] = '\0';
                nspv_log_message("%s cc-error %s\n", __func__, error);

                return NULL;
            }
            else if (jerror->valuestring != NULL)   {
                // { "error" : "some-error-message" } }
                // add nspv error:
                snprintf(error, WR_MAXCCERRORLEN, "nspv-error: %s", (jerror->valuestring ? jerror->valuestring : "null"));
                error[WR_MAXCCERRORLEN - 1] = '\0';
                nspv_log_message("%s nspv-error %s\n", __func__, error);

                return NULL;
            }
            else {
                // no error, continue
            }
        }
        
        if (cJSON_HasObjectItem(json, "result"))
        { 
            // check result as object, like this:
            //  { "result" : { "result": "error", "error": "some-error..."} }

            // get app data container
            cJSON *jresult = cJSON_GetObjectItem(json, "result");

            // check if cc level error exists
            if (cJSON_IsObject(jresult) && cJSON_HasObjectItem(jresult, "error"))
            {
                cJSON * jccerror = cJSON_GetObjectItem(jresult, "error");
                if (cJSON_IsString(jccerror) && jccerror->valuestring != NULL) {
                    snprintf(error, WR_MAXCCERRORLEN, "cc-error: %s", cJSON_GetObjectItem(jresult, "error")->valuestring);
                    error[WR_MAXCCERRORLEN - 1] = '\0';
                    return NULL;
                }
            }

            // application result exists and error is empty
            if (cJSON_IsObject(jresult))    // { "result" : { "result": "success", ...} }
                return jresult;             
            else                            // { "result": "success", "hex": "A356143FD..." }
                return json;

        }
        strncpy(error, "no result object", WR_MAXCCERRORLEN);
    }
    else {
        strncpy(error, "null", WR_MAXCCERRORLEN);
    }
    return NULL;
}

// helpers end

// wrapper for NSPV library init
unity_int32_t LIBNSPV_API uplugin_InitNSPV(char *chainName, char *errorStr)
{
    unity_int32_t retcode = 0;

    strcpy(errorStr, "");
    nspv_log_message("%s entering, chainName=%s kogschain ptr=%p\n", __func__, chainName, kogschain);

    if (init_state != WR_NOT_INITED) {
        strncpy(errorStr, "NSPV already initialized", WR_MAXERRORLEN);
        nspv_log_message("%s exiting with error %s\n", __func__, errorStr);
        return -1;
    }

    if (!kogs_plugin_mutex_init) {
        kogs_plugin_mutex_init = true;
        portable_mutex_init(&kogs_plugin_mutex);
    }
    portable_mutex_lock(&kogs_plugin_mutex);

    if (kogschain == NULL) 
    {
        nspv_log_message("%s before NSPV_coinlist_scan, searching chain=%s\n", __func__, chainName);
        kogschain = NSPV_coinlist_scan(chainName, &kmd_chainparams_template);
        nspv_log_message("%s after NSPV_coinlist_scan, kogschain ptr=%p\n", __func__, kogschain);

        if (kogschain != NULL && kogschain != (void*)0xFFFFFFFFFFFFFFFFLL)   // TODO: avoid 0xFFFFFFFFFFFFFFFFLL use
        {
            btc_ecc_start();
            kogsclient = btc_spv_client_new(kogschain, true, /*(dbfile && (dbfile[0] == '0' || (strlen(dbfile) > 1 && dbfile[0] == 'n' && dbfile[0] == 'o'))) ? true : false*/true);
            nspv_log_message("%s after btc_spv_client_new, kogsclient ptr=%p\n", __func__, kogsclient);
            if (kogsclient != NULL)
            {

                btc_spv_client_discover_peers(kogsclient, NULL);
                nspv_log_message("%s discovered nodes %d\n", __func__, kogsclient->nodegroup->nodes->len);
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
                        // errorStr[WR_MAXERRORLEN - 1] = '\0';
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
                // find current working dir on Unity for c apps:
                // it is actually the project path
#if !defined(ANDROID) && !defined(__ANDROID__)
#include <unistd.h>
                char cwd[256];
                getcwd(cwd, sizeof(cwd));
                nspv_log_message("%s cwd=%s\n", __func__, cwd);
                snprintf(errorStr, WR_MAXERRORLEN, "could not find coins file, cwd=%s", cwd);
                errorStr[WR_MAXERRORLEN - 1] = '\0';
#else
                strncpy(errorStr, "could not find coins file", WR_MAXERRORLEN);
#endif
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

    nspv_log_message("%s exiting retcode=%d errorStr=%s\n", __func__, retcode, errorStr);
    return retcode;
}


// kogslist rpc wrapper
unity_int32_t LIBNSPV_API uplugin_LoginNSPV(char *wifStr, char *errorStr)
{
    unity_int32_t retcode = 0;
    char cc_error[WR_MAXCCERRORLEN];
    nspv_log_message("%s enterred\n", __func__);

    if (!kogs_plugin_mutex_init) {
        strcpy(errorStr, "not inited");
        return -1;
    }
    if (init_state != WR_INITED) {
        strncpy(errorStr, "LibNSPV not initialized", WR_MAXERRORLEN);
        nspv_log_message("%s LibNSPV not initialized\n", __func__);
        return -1;
    }
    strcpy(errorStr, "");

    portable_mutex_lock(&kogs_plugin_mutex);    
    cJSON *jresult = NSPV_login(kogschain, wifStr);
    portable_mutex_unlock(&kogs_plugin_mutex);

    if (!check_jresult(jresult, cc_error)) {
        snprintf(errorStr, WR_MAXCCERRORLEN, "could not login to LibNSPV %s", cc_error);
        errorStr[WR_MAXERRORLEN - 1] = '\0';
        retcode = -1;
    }

    if (jresult)
        cJSON_Delete(jresult);

    
    nspv_log_message("%s exiting retcode=%d\n", __func__, retcode);

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

// cc rpc caller 
unity_int32_t LIBNSPV_API uplugin_CallRpcWithJson(char *jsonStr, void **resultPtrPtr, char *errorStr)
{
    char cc_error[WR_MAXCCERRORLEN];

    nspv_log_message("%s enterred", __func__);
    nspv_log_message("%s resultPtrPtr=%p", __func__, resultPtrPtr);

    if (!kogs_plugin_mutex_init) {
        strncpy(errorStr, "not inited", WR_MAXERRORLEN);
        return -1;
    }
    if (init_state != WR_INITED) {
        nspv_log_message("%s: exiting, state not inited", __func__);
        strncpy(errorStr, "not inited", WR_MAXERRORLEN);
        return -1;
    }

    if (jsonStr == NULL || strlen(jsonStr) == 0) {
        strncpy(errorStr, "method is null or empty", WR_MAXERRORLEN);
        return -1;
    }

    cJSON *jrpcrequest = cJSON_Parse(jsonStr);
    if (jrpcrequest == NULL) {  
        strncpy(errorStr, "could not parse json request", WR_MAXERRORLEN);
        return -1;
    }

    cJSON *jmethod = cJSON_GetObjectItem(jrpcrequest, "method");
    if (jmethod == NULL || !cJSON_IsString(jmethod)) {
        strncpy(errorStr, "could not find method param in json request", WR_MAXERRORLEN);
        return -1;
    }

    cJSON *jrpcresult = NULL;
    unity_int32_t retcode = 0;

    portable_mutex_lock(&kogs_plugin_mutex);
    jrpcresult = NSPV_remoterpccall(kogsclient, jmethod->valuestring, jrpcrequest);
    portable_mutex_unlock(&kogs_plugin_mutex);

    nspv_log_message("%s rpcresult ptr=%p", __func__, jrpcresult);
    char *debStr = cJSON_Print(jrpcresult);
    nspv_log_message("%s rpcresult 1/2 str=%s", __func__, debStr ? debStr : "null-str");
    nspv_log_message("%s rpcresult 2/2 str=%s", __func__, debStr ? debStr + 980 : "null-str");
    if (debStr) cJSON_free(debStr);

    strcpy(errorStr, "");
    *resultPtrPtr = NULL;

    cJSON *jresult;
    if ((jresult = check_jresult(jrpcresult, cc_error)) != NULL)
    {
        char *jsonStr = cJSON_Print(jresult);
        if (jsonStr != NULL) {
            *resultPtrPtr = jsonStr;
        }
        else {
            retcode = -1;
            strncpy(errorStr, "cannot serialize 'result' object to string", WR_MAXERRORLEN);
        }
        nspv_log_message("%s json result=%s", __func__, jsonStr ? jsonStr : "null-ptr");

        cJSON_Delete(jrpcresult);
    }
    else {
        snprintf(errorStr, WR_MAXERRORLEN, "rpc result invalid %s", cc_error);
        errorStr[WR_MAXERRORLEN - 1] = '\0';
        retcode = -1;
    }
    cJSON_Delete(jrpcrequest);

    nspv_log_message("%s exiting retcode=%d %s", __func__, retcode, errorStr);
    return retcode;
}
// cc rpc caller with json
unity_int32_t LIBNSPV_API uplugin_CallRpcMethod(char *method, char *params, void **resultPtrPtr, char *errorStr)
{
    char cc_error[WR_MAXCCERRORLEN];

    nspv_log_message("%s enterred", __func__);
    nspv_log_message("%s resultPtrPtr=%p", __func__, resultPtrPtr);

    if (!kogs_plugin_mutex_init) {
        strncpy(errorStr, "not inited", WR_MAXERRORLEN);
        return -1;
    }
    if (init_state != WR_INITED) {
        nspv_log_message("%s: exiting, state not inited", __func__);
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
            cJSON_Delete(jparams);
            return -1;
        }
    }

    cJSON *jrpcresult = NULL;
    unity_int32_t retcode = 0;

    jaddstr(jrpcrequest, "method", method);
    if (jparams)
        jadd(jrpcrequest, "params", jparams); // add params if it is valid array

    portable_mutex_lock(&kogs_plugin_mutex);
    jrpcresult = NSPV_remoterpccall(kogsclient, method, jrpcrequest);
    portable_mutex_unlock(&kogs_plugin_mutex);

    nspv_log_message("%s rpcresult ptr=%p", __func__, jrpcresult);
    char *debStr = cJSON_Print(jrpcresult);
    nspv_log_message("%s rpcresult 1/2 str=%s", __func__, debStr ? debStr : "null-str");
    nspv_log_message("%s rpcresult 2/2 str=%s", __func__, debStr ? debStr+980 : "null-str");

    if (debStr) cJSON_free(debStr);

    strcpy(errorStr, "");
    *resultPtrPtr = NULL;

    cJSON *jresult;
    if ((jresult = check_jresult(jrpcresult, cc_error)) != NULL)
    {
        char *jsonStr = cJSON_Print(jresult);
        if (jsonStr != NULL) {
            *resultPtrPtr = jsonStr;
        }
        else {
            retcode = -1;
            strncpy(errorStr, "cannot serialize 'result' object", WR_MAXERRORLEN);
        }
        nspv_log_message("%s json result=%s", __func__, jsonStr ? jsonStr : "null-ptr");

        cJSON_Delete(jrpcresult);
    }
    else {
        snprintf(errorStr, WR_MAXERRORLEN, "rpc result invalid %s", cc_error);
        errorStr[WR_MAXERRORLEN - 1] = '\0';
        retcode = -1;
    }
    cJSON_Delete(jrpcrequest);

    // dont do delete as we added it to jrpcrequest!
    // if (jparams)
    //    cJSON_Delete(jparams);
   
    nspv_log_message("%s exiting retcode=%d %s", __func__, retcode, errorStr);
    return retcode;
}

// FinalizeCCTx wrapper
unity_int32_t LIBNSPV_API uplugin_FinalizeCCTx(char *txdataStr, void **resultPtrPtr, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");
    if (init_state != WR_INITED) {
        nspv_log_message("%s: exiting, state not inited", __func__);
        strcpy(errorStr, "not inited");
        return -1;
    }

    if (txdataStr == NULL) {
        strncpy(errorStr, "txdata is null", WR_MAXERRORLEN);
        return -1;
    }
    if (resultPtrPtr == NULL) {
        strncpy(errorStr, "resultPtrPtr is null", WR_MAXERRORLEN);
        return -1;
    }

    *resultPtrPtr = NULL;

    nspv_log_message("%s source tx 1/2=%s", __func__, txdataStr);
    nspv_log_message("%s source tx 2/2=%s", __func__, txdataStr + 982);

    cJSON *jtxdata = cJSON_Parse(txdataStr);
    if (jtxdata == NULL) {
        strncpy(errorStr, "could not parse txdata", WR_MAXERRORLEN);
        return -1;
    }

    cstring *cstrTx = FinalizeCCtx(kogsclient, jtxdata);
    if (cstrTx != NULL) {
        char *bufStr = malloc(cstrTx->len+1);
        strcpy(bufStr, cstrTx->str);
        *resultPtrPtr = bufStr;
        nspv_log_message("%s signed tx 1/2=%s", __func__, bufStr);
        nspv_log_message("%s signed tx 2/2=%s", __func__, bufStr+982);
    }
    else {
        strncpy(errorStr, "could not sign tx", WR_MAXERRORLEN);
        retcode = -1;
    }
    if (jtxdata)
        cJSON_Delete(jtxdata);
    if (cstrTx)
        cstr_free(cstrTx, true);

    nspv_log_message("%s exiting retcode=%d %s", __func__, retcode, errorStr);
    return retcode;
}


// BroadcastTx wrapper
unity_int32_t LIBNSPV_API uplugin_BroadcastTx(char *txdataStr, void **resultPtrPtr, char *errorStr)
{
    unity_int32_t retcode = 0;

    nspv_log_message("%s enterred", __func__);
    strcpy(errorStr, "");

    if (!kogs_plugin_mutex_init) {
        strncpy(errorStr, "not inited", WR_MAXERRORLEN);
        return -1;
    }
    if (init_state != WR_INITED) {
        nspv_log_message("%s: exiting, state not inited", __func__);
        strcpy(errorStr, "not inited");
        return -1;
    }

    if (txdataStr == NULL) {
        strncpy(errorStr, "txdata is null", WR_MAXERRORLEN);
        return -1;
    }
    if (resultPtrPtr == NULL) {
        strncpy(errorStr, "resultPtrPtr is null", WR_MAXERRORLEN);
        return -1;
    }

    *resultPtrPtr = NULL;

    portable_mutex_lock(&kogs_plugin_mutex);
    cJSON *jresult = NSPV_broadcast(kogsclient, txdataStr);
    portable_mutex_unlock(&kogs_plugin_mutex);
    if (jresult != NULL) {
        *resultPtrPtr = cJSON_Print(jresult);
    }
    else {
        strncpy(errorStr, "broadcast result null", WR_MAXERRORLEN);
        retcode = -1;
    }
    if (jresult)
        cJSON_Delete(jresult);
  
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

