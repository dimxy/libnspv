#ifndef NSPV_CCUTILS_H
#define NSPV_CCUTILS_H

#include "tools/cryptoconditions/include/cryptoconditions.h"
#include "../include/btc/script.h"

#define EVAL_FAUCET (0xe4)


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

#endif // NSPV_CCUTILS_H