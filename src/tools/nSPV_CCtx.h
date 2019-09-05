
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


CC* CCNewEval(char *code,int32_t size)
{
    CC *cond = cc_new(CC_Eval);
    cond->code = (uint8_t*) malloc(size);
    memcpy(cond->code, code, size);
    cond->codeLength = size;
    return cond;
}

CC* CCNewThreshold(int t, CC** v, int size)
{
    CC *cond = cc_new(CC_Threshold);
    cond->threshold = t;
    cond->size = size;
    cond->subconditions = (CC**) calloc(size, sizeof(CC*));
    for (int i=0;i<size;i++) memcpy(cond->subconditions+i, *(v+i), sizeof(CC*));
    return cond;
}

static unsigned char* CopyPubKey(uint8_t *pkIn)
{
    unsigned char* pk = (unsigned char*) malloc(33);
    memcpy(pk, pkIn, 33);
    return pk;
}

CC* CCNewSecp256k1(uint8_t* k)
{
    CC *cond = cc_new(CC_Secp256k1);
    cond->publicKey = CopyPubKey(k);
    return cond;
}

CC *MakeCCcond1(uint8_t evalcode,uint8_t *pk)
{
    cstring *ss;

    CC *pks[1]={CCNewSecp256k1(pk)};    
    ss=cstr_new_sz(1);
    ser_varlen(ss,evalcode);
    CC *condCC = CCNewEval(ss->str,ss->len);
    CC *Sig = CCNewThreshold(1, pks, 1);
    CC *v[2]= {condCC, Sig};
    cstr_free(ss, true);
    return CCNewThreshold(2, v ,2);
}

cstring* CCPubKey(const CC *cond)
{
    unsigned char buf[1000],ss[1024]; int32_t n=0;

    size_t len = cc_conditionBinary(cond, buf);
    cstring* ccpk = cstr_new_sz(len+24);
    if (len < OP_PUSHDATA1) ser_varlen(ccpk,len);
    else if (len < 0xFF)
    {
        ser_varlen(ccpk,OP_PUSHDATA1);
        ser_varlen(ccpk,len);
    }
    else if (len < 0xFFFF)
    {
        ser_varlen(ccpk,OP_PUSHDATA2);
        ser_varlen(ccpk,len);
    }
    ser_bytes(ccpk,buf,len);
    ser_varlen(ccpk,OP_CHECKCRYPTOCONDITION);
    return ccpk;
}

void MakeCC1vout(btc_tx *mtx,uint8_t evalcode, uint64_t satoshis,uint8_t *pk)
{
    CC *payoutCond = MakeCCcond1(evalcode,pk);
    btc_tx_out *vout = btc_tx_out_new();    
    vout->script_pubkey = CCPubKey(payoutCond);
    vout->value = satoshis;
    vector_add(mtx->vout,vout);
}


cJSON *NSPV_CC_faucetget(btc_spv_client *client)
{
    cJSON *result = cJSON_CreateObject(); btc_tx *mtx=0; int32_t isKMD = 0;

    mtx = btc_tx_new(client->chainparams->komodo != 0 ? SAPLING_TX_VERSION : 1);
    isKMD = (strcmp(client->chainparams->name,"KMD") == 0);
    if ( isKMD != 0 )
        mtx->locktime = (uint32_t)time(NULL) - 777;
    
    MakeCC1vout(mtx,EVAL_FAUCET,10000,NSPV_pubkey.pubkey);
    jaddstr(result,"result","error");
    jaddstr(result,"error","not implemented yet");
    jaddstr(result,"hex","deadbeef");
    jaddstr(result,"lastpeer",NSPV_lastpeer);
    btc_tx_free(mtx);
    return(result);
}
#endif // NSPV_CCTX_H
