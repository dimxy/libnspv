
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

extern btc_tx *NSPV_gettx(btc_spv_client *client,bits256 txid,int32_t v,int32_t height);

#define FAUCETSIZE (COIN / 10)

cstring *FinalizeCCtx(btc_spv_client *client, btc_tx *mtx, uint64_t txfee, cstring *opret, CCSigData *pData )
{
    int32_t i,n,retval=0,isKMD,skipvalidation=0; int64_t change,normalinputs=0,totaloutputs=0,normaloutputs=0,totalinputs=0,normalvins=0,ccvins=0;
    btc_tx *vintx=0; uint256 hashBlock; char str[65];

    for (i=0; i<(int32_t)mtx->vout->len; i++)
    {
        btc_tx_out *vout=(btc_tx_out *)vector_idx(mtx->vout,i);
        if (IsPayToCryptoCondition(vout->script_pubkey) == 0 )
            normaloutputs += vout->value;
        totaloutputs += vout->value;
    }
    if ( (n= (int32_t)mtx->vin->len) > CC_MAXVINS )
    {
        fprintf(stderr,"FinalizeCCTx: %d is too many vins\n",n);
        return(cstr_new("0"));
    }
    //Reorder vins so that for multiple normal vins all other except vin0 goes to the end
    //This is a must to avoid hardfork change of validation in every CC, because there could be maximum one normal vin at the begining with current validation.
    for (i=0; i<n; i++)
    {
        btc_tx_in *vin=(btc_tx_in *)vector_idx(mtx->vin,i);
        if (i==0 && vin->prevout.n==10e8)
            continue;
        if ( (vintx=NSPV_gettx(client,btc_uint256_to_bits256(vin->prevout.hash),0,0)) != NULL && vin->prevout.n < vintx->vout->len )
        {
            btc_tx_out *vout=vector_idx(vintx->vout,vin->prevout.n);
            if ( IsPayToCryptoCondition(vout->script_pubkey) == 0 && ccvins==0)
                normalvins++;            
            else ccvins++;
        }
        else
        {
            fprintf(stderr,"vin.%d vout.%d is bigger than vintx.%d\n",i,vin->prevout.n,(int32_t)vintx->vout->len);
            return(cstr_new(""));
        }
    }
    if (normalvins>1 && ccvins)
    {        
        for(i=1;i<normalvins;i++)
        {   
            vector_add(mtx->vin,vector_idx(mtx->vin,1));
            vector_remove_idx(mtx->vin,1);                       
        }
    }
    for (i=0; i<n; i++)
    {
        btc_tx_in *vin=vector_idx(mtx->vin,i);
        if (i==0 && vin->prevout.n==10e8) continue;
        if ( (vintx=NSPV_gettx(client,btc_uint256_to_bits256(vin->prevout.hash),0,0)) != NULL)
        {
            btc_tx_out *vout=vector_idx(vintx->vout,vin->prevout.n);
            totalinputs = vout->value;
            if ( IsPayToCryptoCondition(vout->script_pubkey) == 0 )        
                normalinputs += vout->value;;
        } else fprintf(stderr,"FinalizeCCTx couldnt find %s mgret.%d\n",bits256_str(str,btc_uint256_to_bits256(vin->prevout.hash)));
    }
    if ( totalinputs >= totaloutputs+2*txfee )
    {
        change = totalinputs - (totaloutputs+txfee);
        btc_tx_add_p2pkh_out(mtx,change,&NSPV_pubkey);
    }
}

cJSON *NSPV_CC_faucetget(btc_spv_client *client)
{
    cJSON *result = cJSON_CreateObject(); btc_tx *mtx=0; int32_t isKMD = 0; cstring *hex;
    int64_t txfee=10000,inputs,CCchange,nValue=FAUCETSIZE; btc_key key; btc_pubkey faucetpk;

    mtx = btc_tx_new(client->chainparams->komodo != 0 ? SAPLING_TX_VERSION : 1);
    isKMD = (strcmp(client->chainparams->name,"KMD") == 0);
    if ( isKMD != 0 )
        mtx->locktime = (uint32_t)time(NULL) - 777;    
    memcpy(key.privkey,FaucetCCpriv,sizeof(FaucetCCpriv));
    btc_pubkey_from_key(&key,&faucetpk);
    // if ( (inputs= AddFaucetInputs(cp,mtx,faucetpk,nValue+txfee,60)) > 0 )
    // {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            MakeCC1vout(mtx,EVAL_FAUCET,CCchange,faucetpk.pubkey);
        btc_tx_add_p2pkh_out(mtx,nValue,&NSPV_pubkey);
    // }
    if ((hex=FinalizeCCtx(client,mtx,txfee,NULL,NULL))!=NULL)
      printf("%s\n",hex->str);
    jaddstr(result,"result","error");
    jaddstr(result,"error","not implemented yet");
    jaddstr(result,"hex","deadbeef");
    jaddstr(result,"lastpeer",NSPV_lastpeer);
    btc_tx_free(mtx);
    return(result);
}
#endif // NSPV_CCTX_H
