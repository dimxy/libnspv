
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

#include "nSPV_CCUtils.h"
// @blackjok3r and @mihailo implement the CC tx creation functions here
// instead of a swissarmy knife finalizeCCtx, i think it is better to pass in the specific info needed into a CC signing function. this would eliminate the comparing to all the different possibilities
// since the CC rpc that creates the tx will know which vins are normal and which ones are CC, and most importantly what type of CC vin it is, it will be much simpler finalize function, though it will mean all the CC rpc calls will have to do more work. that was the rationale behind FinalizeCCtx, but i hear a lot of complaints about the complexity it has become.
// please make a new way of doing CC tx that wont lead to complaints later. let us start with faucetget

extern bool NSPV_SignTx(btc_tx *mtx,int32_t vini,int64_t utxovalue,cstring *scriptPubKey,uint32_t nTime);

#define FAUCETSIZE (COIN / 10)

cstring *FinalizeCCtx(btc_spv_client *client, cstring *hex, vector *sigData )
{
    int32_t i,n; cstring *finalHex;
    btc_tx *mtx=btc_tx_decodehex(hex->str);
    n=sigData->len;
    for (i=0; i<n; i++)
    {
        CCSigData *sData=vector_idx(sigData,1);
        btc_tx_in *vin=btc_tx_vin(mtx,sData->vini);
        bits256 sigHash;
        
        if (sData->isCC)
        {
            cstring *script=CCPubKey(sData->cond);
            sigHash=NSPV_sapling_sighash(mtx,sData->vini,sData->voutValue,(unsigned char *)script->str,script->len);
            if (cc_signTreeSecp256k1Msg32(sData->cond,NSPV_key.privkey,sigHash.bytes)!=0)
                CCSig(sData->cond,vin->script_sig);
            cstr_free(script,1);
        }
        else
        {
            NSPV_SignTx(mtx,sData->vini,sData->voutValue,sData->voutScriptPubkey,0);
        }        
    }
    finalHex=btc_tx_to_cstr(mtx);
    vector_free(sigData,true);
    btc_tx_free(mtx);
    return (finalHex);
}
#endif // NSPV_CCTX_H
