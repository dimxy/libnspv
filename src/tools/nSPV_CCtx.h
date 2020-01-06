
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

#ifndef NSPV_CCTX_H
#define NSPV_CCTX_H

#include "nSPV_defs.h"
#include "nSPV_CCUtils.h"
// @blackjok3r and @mihailo implement the CC tx creation functions here
// instead of a swissarmy knife finalizeCCtx, i think it is better to pass in the specific info needed into a CC signing function. this would eliminate the comparing to all the different possibilities
// since the CC rpc that creates the tx will know which vins are normal and which ones are CC, and most importantly what type of CC vin it is, it will be much simpler finalize function, though it will mean all the CC rpc calls will have to do more work. that was the rationale behind FinalizeCCtx, but i hear a lot of complaints about the complexity it has become.
// please make a new way of doing CC tx that wont lead to complaints later. let us start with faucetget

extern bool NSPV_SignTx(btc_tx *mtx,int32_t vini,int64_t utxovalue,cstring *scriptPubKey,uint32_t nTime);

cstring *FinalizeCCtx(btc_spv_client *client, cJSON *txdata, char *error)
{
    int32_t i,n,vini; 
    cstring *finalHex,*hex; 
    cJSON *sigData=NULL; 
    int64_t voutValue;
    char ccerror[256];
    
    if (!cJSON_HasObjectItem(txdata, "hex")) {
        // return(cstr_new("No field \"hex\" in JSON response from fullnode"));
        nspv_log_message("%s No field \"hex\" in JSON response from fullnode\n", __func__);
        return NULL;
    }

    hex=cstr_new(jstr(txdata,"hex"));
    cstr_append_c(hex,0);
    btc_tx *mtx=btc_tx_decodehex(hex->str);
    cstr_free(hex,1);
    if (!mtx) {
        nspv_log_message("%s Invalid hex tx in JSON response from fullnode (could not parse into mtx)\n", __func__);
        if (error) {
            snprintf(error, NSPV_MAXERRORLEN - 1, "Invalid hex tx in txdata parameter");
            error[NSPV_MAXERRORLEN - 1] = '\0';
        }
        //return(cstr_new("Invalid hex in JSON response from fullnode"));
        return NULL;
    }
    sigData=jarray(&n,txdata,"SigData");
    if (!sigData) {
        nspv_log_message("%s No field \"SigData\" in JSON response from fullnode\n", __func__);
        // return(cstr_new("No field \"SigData\" in JSON response from fullnode"));
        if (error) {
            snprintf(error, NSPV_MAXERRORLEN - 1, "No field \"SigData\" in txdata parameter");
            error[NSPV_MAXERRORLEN - 1] = '\0';
        }
        return NULL;
    }
    for (i=0; i < n; i++)
    {
        cJSON *item=jitem(sigData,i);
        vini=jint(item,"vin");
        voutValue= j64bits(item,"amount");
        if (cJSON_HasObjectItem(item,"cc")!=0)
        {
            CC *cond;
            btc_tx_in *vin=btc_tx_vin(mtx,vini);
            bits256 sigHash;
            memset(ccerror,0,sizeof(ccerror));
            cond=cc_conditionFromJSON(jobj(item,"cc"),ccerror);
            if (cond==NULL || ccerror[0])
            {
                btc_tx_free(mtx);
                if (cond) 
                    cc_free(cond);
                //return cstr_new(error);
                nspv_log_message("%s cc error from cc_conditionFromJSON %s\n", __func__, ccerror);
                if (error) {
                    snprintf(error, NSPV_MAXERRORLEN - 1, "error from parse \"cc\" field %s", ccerror);
                    error[NSPV_MAXERRORLEN - 1] = '\0';
                }
                return NULL;
            }
            cstring *script = CCPubKey(cond);
            sigHash = NSPV_sapling_sighash(mtx,vini,voutValue,(unsigned char *)script->str,script->len);
            sigHash = bits256_rev(sigHash);
            if ((cc_signTreeSecp256k1Msg32(cond, NSPV_key.privkey, sigHash.bytes)) != 0)
            {
                if (vin->script_sig)
                {
                    cstr_free(vin->script_sig,1);
                    vin->script_sig=cstr_new("");                    
                }
                CCSig(cond,vin->script_sig);
            }
            else
            {
                nspv_log_message("%s cc_signTreeSecp256k1Msg32 returned null\n", __func__);
            }
            cstr_free(script,1);
            cc_free(cond);
        }
        else
        {            
            cstring *voutScriptPubkey=cstr_new((char *)utils_hex_to_uint8(jstr(item,"scriptPubKey")));
            if (NSPV_SignTx(mtx,vini,voutValue,voutScriptPubkey,0)==0)
            {
                //fprintf(stderr,"signing error for vini.%d\n",vini);
                nspv_log_message("signing error for vini.%d\n", vini);
                if (error) {
                    snprintf(error, NSPV_MAXERRORLEN - 1, "signing error for vini.%d", vini);
                    error[NSPV_MAXERRORLEN - 1] = '\0';
                }
                cstr_free(voutScriptPubkey,1);
                btc_tx_free(mtx);
                snprintf(err, NSPV_MAXERRORLEN, "signing error for vini.%d", vini);
                return NULL;
            }
            cstr_free(voutScriptPubkey,1);
        }
    }
    finalHex = btc_tx_to_cstr(mtx);
    btc_tx_free(mtx);
    return (finalHex);
}
#endif // NSPV_CCTX_H
