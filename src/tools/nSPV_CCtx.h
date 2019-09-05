
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
