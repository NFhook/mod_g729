/*/////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2005-2007 Intel Corporation. All Rights Reserved.
//
//     Intel(R) Integrated Performance Primitives
//     USC - Unified Speech Codec interface library
//
// By downloading and installing USC codec, you hereby agree that the
// accompanying Materials are being provided to you under the terms and
// conditions of the End User License Agreement for the Intel(R) Integrated
// Performance Primitives product previously accepted by you. Please refer
// to the file ippEULA.rtf or ippEULA.txt located in the root directory of your Intel(R) IPP
// product installation for more information.
//
// A speech coding standards promoted by ITU, ETSI, 3GPP and other
// organizations. Implementations of these standards, or the standard enabled
// platforms may require licenses from various entities, including
// Intel Corporation.
//
//
// Purpose: G.729/A/B/D/E speech codec: encoder API functions.
//
*/

#include <ippsc.h>
#include <ipps.h>
#include "vadg729.h"
#include "owng729.h"

static __ALIGN32 CONST Ipp16s LUT1_6k[CDBK1_DIM_6K] = { 0, 4,
 6, 5,
 2, 1,
 7, 3};
static __ALIGN32 CONST Ipp16s LUT2_6k[CDBK2_DIM_6K] = { 0, 4,
 3, 7,
 5, 1,
 6, 2};

static __ALIGN32 CONST Ipp16s tab1[3] = {
 (1<<10)+875,
 -3798,
 (1<<10)+875
};
static __ALIGN32 CONST Ipp16s tab2[3] = {
 (1<<12),
 (1<<12)+3711,
 -3733
};

static __ALIGN32 CONST Ipp16s hammingWin[LP_WINDOW_DIM] = {
    (1<<11)+573,  (1<<11)+575,(1<<11)+581,  2638,  2651,
    2668,  2689,
    2713,  2741,  2772,
    2808,  2847,  2890,
    2936,  2986,
    3040,  3097,  3158,  3223,  3291,  3363,  3438,  3517,  3599,  3685,  3774,  3867, 3963,
    4063,  4166,  4272,  4382,  4495,  4611,  4731,  4853,  4979,
    5108,  5240,  5376,  5514,  5655,  5800,  5947,
    6097,  6250,  6406,  6565,  6726,  6890,
    7057,  7227,  7399,  7573,  7750,  7930,
    8112,  8296,  8483,  8672,  8863,
    9057,  9252,  9450,  9650,  9852,
    10055, 10261, 10468, 10677, 10888,
    11101, 11315, 11531, 11748, 11967,
    12187, 12409, 12632, 12856,
    13082, 13308, 13536, 13764, 13994,
    14225, 14456, 14688, 14921,
    15155, 15389, 15624, 15859,
    16095, 16331, 16568, 16805,
    17042, 17279, 17516, 17754, 17991,
    18228, 18465, 18702, 18939,
    19175, 19411, 19647, 19882,
    20117, 20350, 20584, 20816, 21048,
    21279, 21509, 21738, 21967,
    22194, 22420, 22644, 22868,
    23090, 23311, 23531, 23749, 23965,
    24181, 24394, 24606, 24816,
    25024, 25231, 25435, 25638, 25839,
    26037, 26234, 26428, 26621, 26811, 26999,
    27184, 27368, 27548, 27727, 27903,
    28076, 28247, 28415, 28581, 28743, 28903,
    29061, 29215, 29367, 29515, 29661, 29804, 29944,
    30081, 30214, 30345, 30472, 30597, 30718, 30836, 30950,
    31062, 31170, 31274, 31376, 31474, 31568, 31659, 31747, 31831, 31911, 31988,
    32062, 32132, 32198, 32261, 32320, 32376, 32428, 32476, 32521, 32561, 32599,
    32632, 32662, 32688, 32711, 32729, 32744, 32755, 32763, IPP_MAX_16S, IPP_MAX_16S,
    32741, 32665, 32537, 32359, 32129,
    31850, 31521, 31143,
    30716, 30242,
    29720, 29151,
    28538,
    27879, 27177,
    26433,
    25647,
    24821,
    23957, 23055,
    22117,
    21145,
    20139,
    19102,
    18036,
    16941,
    15820,
    14674,
    13505,
    12315,
    11106,
    9879,
    8637,
    7381,
    6114,
    4838,
    3554,
    2264,
    971
};

static __ALIGN32 CONST Ipp16s CodecTypeToRate[5] = {
 3,
 3,
 2,
 4,
 0
};

static __ALIGN32 CONST Ipp16s prevRCpreset[2]={
 0,
 0
};

static void ACFsumUpd(Ipp16s *ACFsum,Ipp16s *ACFsumSfs,Ipp16s *pACF,Ipp16s *ACFsfs, Ipp32s* pSumMem);

static Ipp32s EncoderObjSize(void) {
    Ipp32s fltSize;
    Ipp32s objSize=sizeof(G729Encoder_Obj);
    ippsHighPassFilterSize_G729(&fltSize);
    objSize += fltSize;
    SynthesisFilterSize_G729(&fltSize);
    objSize += 2 * fltSize;
    VoiceActivityDetectSize_G729(&fltSize);
    objSize += fltSize;
    objSize += 4*32;
    return objSize;
}

G729_CODECFUN( APIG729_Status, apiG729Codec_ScratchMemoryAlloc,(Ipp32s *pCodecSize)) {
    if(NULL==pCodecSize)
        return APIG729_StsBadArgErr;
    *pCodecSize = G729_ENCODER_SCRATCH_MEMORY_SIZE;
    return APIG729_StsNoErr;
}
G729_CODECFUN( APIG729_Status, apiG729Encoder_Alloc,
               (G729Codec_Type codecType, Ipp32s *pCodecSize)) {
    if((codecType != G729_CODEC)&&(codecType != G729A_CODEC)
       &&(codecType != G729D_CODEC)&&(codecType != G729E_CODEC)&&(codecType != G729I_CODEC)) {
        return APIG729_StsBadCodecType;
    }
    *pCodecSize =  EncoderObjSize();
    return APIG729_StsNoErr;
}

G729_CODECFUN( APIG729_Status, apiG729Encoder_Mode,
               (G729Encoder_Obj* encoderObj, G729Encode_Mode mode)) {
    if(G729Encode_VAD_Enabled != mode && G729Encode_VAD_Disabled != mode) {
        return APIG729_StsBadArgErr;
    }
    encoderObj->objPrm.mode = mode;
    return APIG729_StsNoErr;
}

G729_CODECFUN( APIG729_Status, apiG729Encoder_Init,
               (G729Encoder_Obj* encoderObj, G729Codec_Type codecType, G729Encode_Mode mode)) {
    Ipp32s i,fltSize;
    Ipp16s abEnc[6];

   Ipp8s* oldMemBuff;

    if(NULL==encoderObj)
        return APIG729_StsBadArgErr;

    if((codecType != G729_CODEC)&&(codecType != G729A_CODEC)
       &&(codecType != G729D_CODEC)&&(codecType != G729E_CODEC)&&(codecType != G729I_CODEC)) {
        return APIG729_StsBadCodecType;
    }

    oldMemBuff = encoderObj->Mem.base; /* if Reinit */

    ippsZero_16s((Ipp16s*)encoderObj,sizeof(*encoderObj)>>1) ;

    encoderObj->objPrm.objSize = EncoderObjSize();
    encoderObj->objPrm.mode = mode;
    encoderObj->objPrm.key = ENC_KEY;
    encoderObj->objPrm.codecType=codecType;


    encoderObj->mode = mode;
    encoderObj->preProc = (Ipp8s*)encoderObj + sizeof(G729Encoder_Obj);
    encoderObj->preProc = IPP_ALIGNED_PTR(encoderObj->preProc, 16);
    ippsHighPassFilterSize_G729(&fltSize);
    encoderObj->synFltw = (Ipp8s*)encoderObj->preProc + fltSize;
    encoderObj->synFltw = IPP_ALIGNED_PTR(encoderObj->synFltw, 16);
    SynthesisFilterSize_G729(&fltSize);
    encoderObj->synFltw0 = (Ipp8s*)encoderObj->synFltw + fltSize;
    encoderObj->synFltw0 = IPP_ALIGNED_PTR(encoderObj->synFltw0, 16);
    encoderObj->vadMem  = (Ipp8s*)encoderObj->synFltw0 + fltSize;
    encoderObj->vadMem = IPP_ALIGNED_PTR(encoderObj->vadMem, 16);
    abEnc[0] = tab2[0]; abEnc[1] = tab2[1]; abEnc[2] = tab2[2];
    abEnc[3] = tab1[0]; abEnc[4] = tab1[1]; abEnc[5] = tab1[2];
    for(i=0;i<4;i++) encoderObj->prevFrameQuantEn[i]=-14336;
    VoiceActivityDetectInit_G729(encoderObj->vadMem);
    ippsHighPassFilterInit_G729(abEnc,encoderObj->preProc);
    SynthesisFilterInit_G729(encoderObj->synFltw);
    SynthesisFilterInit_G729(encoderObj->synFltw0);


    ippsZero_16s(encoderObj->speechHistory,SPEECH_BUF_DIM);
    ippsZero_16s(encoderObj->prevExcitat, L_prevExcitat);
    ippsZero_16s(encoderObj->prevWgtSpeech, MAX_PITCH_LAG);
    ippsZero_16s(encoderObj->resFilMem0,  BWLPCF_DIM);
    encoderObj->betaPreFilter = PITCH_SHARP_MIN;
    ippsCopy_16s(presetOldA, encoderObj->prevSubfrLPC, LPF_DIM+1);
    encoderObj->prevRC[0] = prevRCpreset[0];
    encoderObj->prevRC[1] = prevRCpreset[1];

    ippsCopy_16s(presetLSP, encoderObj->prevSubfrLSP, LPF_DIM);
    ippsCopy_16s(presetLSP, encoderObj->prevSubfrLSPquant, LPF_DIM);
    ippsZero_16s(encoderObj->zeroPostFiltVec1 + BWLPCF_DIM+1, LP_SUBFRAME_DIM);
    encoderObj->BWDcounter2 = 0;
    encoderObj->FWDcounter2 = 0;

    for(i=0; i<LSP_MA_ORDER; i++)
        ippsCopy_16s( &resetPrevLSP[0], &encoderObj->prevLSPfreq[i][0], LPF_DIM);

    for(i=0; i<4; i++) encoderObj->coderErr[i] = BWF_HARMONIC;

    if(encoderObj->objPrm.codecType == G729A_CODEC) {
        encoderObj->seed = SEED_INIT;
        encoderObj->CNGidx = 0;
        ippsZero_16s(encoderObj->encPrm, PARM_DIM+1);
        Init_CNG_encoder(encoderObj);
    } else {
        encoderObj->prevLAR[0]=encoderObj->prevLAR[1]=0;
        encoderObj->prevSubfrSmooth=1;
        encoderObj->seed = SEED_INIT;
        encoderObj->CNGidx = 0;
        encoderObj->dominantBWDmode = 0;
        encoderObj->interpCoeff2_2 = 4506;
        encoderObj->statGlobal = 10000;
        ippsZero_16s(encoderObj->resFilMem, BWLPCF_DIM);
        ippsZero_16s(encoderObj->pPrevFilt, BWLPCF1_DIM);
        encoderObj->pPrevFilt[0] = (1<<12);
        encoderObj->prevLPmode = 0;
        for(i=0; i<5; i++) {
            encoderObj->pLag[i] = 20;
            encoderObj->pGain[i] = 11469;
        }
        ippsZero_16s(encoderObj->encPrm, PARM_DIM+1);
        Init_CNG_encoder(encoderObj);
        ippsZero_16s(encoderObj->BWDsynth, TBWD_DIM);
        encoderObj->pSynth = encoderObj->BWDsynth + SYNTH_BWD_DIM;
        ippsWinHybridGetStateSize_G729E_16s(&fltSize);
        if(fltSize > BWLPCF1_DIM*sizeof(Ipp32s)) {
            return APIG729_StsNotInitialized;
        }
        ippsWinHybridInit_G729E_16s((IppsWinHybridState_G729E_16s*)&encoderObj->hwState);
        ippsZero_16s(encoderObj->pPrevBwdRC, 2);
        ippsZero_16s(encoderObj->pPrevBwdLPC, BWLPCF1_DIM);
        encoderObj->pPrevBwdLPC[0]=(1<<12);
        ippsZero_16s(encoderObj->pBwdLPC2, BWLPCF1_DIM);
        encoderObj->pBwdLPC2[0] = (1<<12);
        encoderObj->BWDFrameCounter = 0;
        encoderObj->val_BWDFrameCounter = 0;
    }

   apiG729Encoder_InitBuff(encoderObj,oldMemBuff);

    return APIG729_StsNoErr;
}
void Init_CNG_encoder(G729Encoder_Obj *encoderObj) {
    Ipp16s i;
    ippsZero_16s(encoderObj->ACFsum, TOTAL_ACF_DIM);
    for(i=0; i<ACF_TOTAL; i++)
        encoderObj->ACFsumSfs[i] = 40;
    ippsZero_16s(encoderObj->pACF, ACF_DIM);
    for(i=0; i<ACF_NOW; i++)
        encoderObj->ACFsfs[i] = 40;
    for(i=0; i<GAIN_NUM; i++)
        encoderObj->energySfs[i] = 40;
    for(i=0; i<GAIN_NUM; i++)
        encoderObj->energy[i] = 0;
    encoderObj->gainNow = 0;
    encoderObj->ACFcounter = 0;
    encoderObj->speechDiff = 0;
    return;
}
void CNG_encoder(Ipp16s *prevExcitat, Ipp16s *prevSubfrLSPquant, Ipp16s *pAq, Ipp16s *pAna,
                G729Encoder_Obj *encoderObj) {
    Ipp16s *seed = &encoderObj->seed;
    Ipp16s VADPrev = ((VADmemory_Obj*)encoderObj->vadMem)->VADPrev;
    LOCAL_ARRAY(Ipp16s, curAcf, LPF_DIM+1, encoderObj);
    LOCAL_ARRAY(Ipp32s, acfInt, LPF_DIM+1, encoderObj);
    LOCAL_ARRAY(Ipp16s, bidArr, LPF_DIM, encoderObj);
    LOCAL_ARRAY(Ipp16s, Coeff, LPF_DIM+1, encoderObj);
    LOCAL_ARRAY(Ipp16s, newLSP, LPF_DIM, encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, lsp, LPF_DIM, encoderObj);
    LOCAL_ARRAY(Ipp16s, lsfSid_q, LPF_DIM, encoderObj);
    LOCAL_ARRAY(Ipp16s, s_ACFsum, LPF_DIM+1, encoderObj);
    LOCAL_ARRAY(Ipp32s, sumMem, LPF_DIM+1, encoderObj);
    Ipp16s *LPCc, energyVal, tmp1, tmp2, delay[2], foo;
    Ipp32s   i, curIgain, dist, distSfs, thresh, ACnorm = encoderObj->ACnorm;
    Ipp16s SIDframeCounter = encoderObj->SIDframeCounter;
    Ipp16s speechDiff = encoderObj->speechDiff;
    Ipp16s prevDTXEnergy = encoderObj->prevDTXEnergy;
    Ipp16s sidGain = encoderObj->sidGain;
    Ipp16s gainNow = encoderObj->gainNow;
    Ipp16s ACFcounter = encoderObj->ACFcounter;
    Ipp16s *energy = encoderObj->energy;
    Ipp16s *energySfs = encoderObj->energySfs;
    Ipp16s *pACF = encoderObj->pACF;
    Ipp16s *ACFsfs = encoderObj->ACFsfs;
    Ipp16s *reflC = encoderObj->reflC;
    Ipp16s *prevCoeff = encoderObj->prevCoeff;
    Ipp16s *quantLspSID = encoderObj->quantLspSID;
    Ipp16s *exc = prevExcitat+L_prevExcitat;
    energy[1] = energy[0]; energySfs[1] = energySfs[0];
    Sum_G729_16s_Sfs(pACF, ACFsfs, curAcf, &energySfs[0], ACF_NOW,sumMem);
    if(curAcf[0] == 0) energy[0] = 0;
    else {
        for(i=0; i<LPF_DIM+1;i++)
            acfInt[i] = curAcf[i] << 16;
        if(ippStsOverflow == ippsLevinsonDurbin_G729B(acfInt, Coeff,bidArr,energy)) {
            ippsCopy_16s(encoderObj->prevSubfrLPC,Coeff,LPF_DIM+1);
            bidArr[0] = encoderObj->prevRC[0];
            bidArr[1] = encoderObj->prevRC[1];
        } else {
            ippsCopy_16s(Coeff,encoderObj->prevSubfrLPC,LPF_DIM+1);
            encoderObj->prevRC[0] = bidArr[0];
            encoderObj->prevRC[1] = bidArr[1];
        }
    }
    /* if 1-st frame of silence => SID frame */
    if(VADPrev != 0) {
        SIDframeCounter = 0;
        pAna[0] = 2;
        encoderObj->energyDim = 1;
        QuantSIDGain_G729B_16s(energy, energySfs, encoderObj->energyDim, &energyVal, &curIgain);

    } else {
        if(encoderObj->energyDim < GAIN_NUM) encoderObj->energyDim++;
        QuantSIDGain_G729B_16s(energy, energySfs, encoderObj->energyDim, &energyVal, &curIgain);
        ippsDotProdAutoScale_16s32s_Sfs(reflC,curAcf, LPF_DIM+1, &dist, &distSfs);
        tmp1 = (Ipp16s)(ACnorm + 9);
        tmp2 = (Ipp16s)distSfs;
        tmp1 = (Ipp16s)(tmp1 - tmp2);
        thresh = energy[0] + ((energy[0] * 4855 + BWF_HARMONIC)>>15);
        if(tmp1<0)
            thresh >>= (- tmp1);
        else
            thresh <<= tmp1;
        if(dist > thresh) {
            speechDiff = 1;
        }
        if(Abs_16s((Ipp16s)(prevDTXEnergy - energyVal)) > 2)
            speechDiff = 1;
        SIDframeCounter++;
        if(SIDframeCounter < SID_FRAME_MIN) {
            pAna[0] = 0;
        } else {
            pAna[0]=(Ipp16s)((speechDiff != 0)?2:0);
            SIDframeCounter = SID_FRAME_MIN;
        }
    }
    if(pAna[0] == 2) {
        SIDframeCounter = speechDiff = 0;
        {
            /* calculate the previous average filter */
            Sum_G729_16s_Sfs(encoderObj->ACFsum, encoderObj->ACFsumSfs, s_ACFsum, &tmp1, ACF_TOTAL,sumMem);
            if(s_ACFsum[0] == 0) {
                ippsCopy_16s(presetOldA,prevCoeff,LPF_DIM+1);
            } else {
                for(i=0; i<LPF_DIM+1;i++)
                    acfInt[i] = s_ACFsum[i] << 16;
                if(ippStsOverflow == ippsLevinsonDurbin_G729B(acfInt, prevCoeff,bidArr,&tmp1)) {
                    ippsCopy_16s(encoderObj->prevSubfrLPC,prevCoeff,LPF_DIM+1);
                    bidArr[0] = encoderObj->prevRC[0];
                    bidArr[1] = encoderObj->prevRC[1];
                } else {
                    ippsCopy_16s(prevCoeff,encoderObj->prevSubfrLPC,LPF_DIM+1);
                    encoderObj->prevRC[0] = bidArr[0];
                    encoderObj->prevRC[1] = bidArr[1];
                }
            }
        }
        ippsAutoCorr_NormE_NR_16s(prevCoeff,LPF_DIM+1,reflC,LPF_DIM+1,&ACnorm);
        ippsDotProdAutoScale_16s32s_Sfs(reflC,curAcf, LPF_DIM+1, &dist, &distSfs);
        tmp1 = (Ipp16s)(ACnorm + 9);
        tmp2 = (Ipp16s)distSfs;
        tmp1 = (Ipp16s)(tmp1 - tmp2);
        thresh = energy[0] + ((energy[0] * 3161 + BWF_HARMONIC)>>15);
        if(tmp1<0)
            thresh >>= (- tmp1);
        else
            thresh <<= tmp1;

        if(dist <= thresh) {
            LPCc = prevCoeff;
        } else {
            LPCc = Coeff;
            ippsAutoCorr_NormE_NR_16s(Coeff,LPF_DIM+1,reflC,LPF_DIM+1,&ACnorm);
        }

        if(encoderObj->codecType==G729A_CODEC)
            ippsLPCToLSP_G729A_16s(LPCc, prevSubfrLSPquant, newLSP);
        else
            ippsLPCToLSP_G729_16s(LPCc, prevSubfrLSPquant, newLSP);
        ippsLSPToLSF_G729_16s(newLSP, lsp);
        if(lsp[0] < LSF_MIN)
            lsp[0] = LSF_MIN;
        for(i=0 ; i <LPF_DIM-1 ; i++)
            if((lsp[i+1] - lsp[i]) < (BW_EXP_FACT<<1))
                lsp[i+1] = (Ipp16s)(lsp[i] + (BW_EXP_FACT<<1));
        if(lsp[LPF_DIM-1] > LSF_MAX)
            lsp[LPF_DIM-1] = LSF_MAX;
        if(lsp[LPF_DIM-1] < lsp[LPF_DIM-2])
            lsp[LPF_DIM-2] = (Ipp16s)(lsp[LPF_DIM-1] - BW_EXP_FACT);
        ippsLSFQuant_G729B_16s(lsp, (Ipp16s*)(encoderObj->prevLSPfreq), lsfSid_q, &pAna[1]);

        ippsLSFToLSP_G729_16s(lsfSid_q,quantLspSID);

        prevDTXEnergy = energyVal;
        sidGain = SIDgain[curIgain];
        pAna[4] = (Ipp16s)curIgain;
    }
    if(VADPrev != 0) gainNow = sidGain;
    else {
        gainNow = (Ipp16s)(((gainNow * GAIN0) + BWF_HARMONIC) >> 15);
        gainNow = (Ipp16s)(gainNow + ((sidGain * GAIN1 + BWF_HARMONIC) >> 15));
    }

    if(gainNow == 0) {
        ippsZero_16s(exc,LP_FRAME_DIM);
        for(i = 0; i < LP_FRAME_DIM; i += LP_SUBFRAME_DIM)
            updateExcErr_G729(0, LP_SUBFRAME_DIM+1, encoderObj->coderErr);
    } else {
        for(i = 0;  i < LP_FRAME_DIM; i += LP_SUBFRAME_DIM) {
            Ipp32s invSq;
            Ipp16s Gp;
            LOCAL_ARRAY(Ipp16s, pos, 4, encoderObj);
            LOCAL_ARRAY(Ipp16s, sign, 4, encoderObj);
            LOCAL_ALIGN_ARRAY(32, Ipp16s, excg, LP_SUBFRAME_DIM, encoderObj);
            LOCAL_ARRAY(Ipp16s,tmpArray,LP_SUBFRAME_DIM, encoderObj);
            const Ipp16s *excCached;

            RandomCodebookParm_G729B_16s(seed,pos,sign,&Gp,delay);
            ippsDecodeAdaptiveVector_G729_16s_I(delay,&prevExcitat[i]);
            if(encoderObj->CNGidx > CNG_STACK_SIZE-1) { /* not cached */
                ippsRandomNoiseExcitation_G729B_16s(seed,excg,LP_SUBFRAME_DIM);
                ippsDotProd_16s32s_Sfs(excg,excg,LP_SUBFRAME_DIM,&invSq,0);
                ippsInvSqrt_32s_I(&invSq,1);
                excCached=excg;
            } else {
                *seed = cngSeedOut[encoderObj->CNGidx];
                invSq = cngInvSqrt[encoderObj->CNGidx];
                excCached=&cngCache[encoderObj->CNGidx][0];
                encoderObj->CNGidx++;
            }
            NoiseExcitationFactorization_G729B_16s(excCached,invSq,gainNow,excg,LP_SUBFRAME_DIM);
            if(ComfortNoiseExcitation_G729B_16s_I(excg,pos,sign,gainNow,Gp,&exc[i],&foo,tmpArray)<0) {
                Gp = 0;
            }
            updateExcErr_G729(Gp, delay[0], encoderObj->coderErr);
            LOCAL_ARRAY_FREE(Ipp16s,tmpArray,LP_SUBFRAME_DIM, encoderObj);
            LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, excg, LP_SUBFRAME_DIM, encoderObj);
            LOCAL_ARRAY_FREE(Ipp16s, sign, 4, encoderObj);
            LOCAL_ARRAY_FREE(Ipp16s, pos, 4, encoderObj);
        }
    }

    ippsInterpolate_G729_16s(prevSubfrLSPquant,quantLspSID,lsp,LPF_DIM);

    ippsLSPToLPC_G729_16s(lsp,&pAq[0]);
    ippsLSPToLPC_G729_16s(quantLspSID,&pAq[LPF_DIM+1]);
    ippsCopy_16s(quantLspSID,prevSubfrLSPquant,LPF_DIM);
    if(ACFcounter == 0) ACFsumUpd(encoderObj->ACFsum,encoderObj->ACFsumSfs,encoderObj->pACF,encoderObj->ACFsfs,sumMem);

    encoderObj->SIDframeCounter = SIDframeCounter;
    encoderObj->ACnorm = (Ipp16s)ACnorm;
    encoderObj->speechDiff = speechDiff;
    encoderObj->prevDTXEnergy = prevDTXEnergy;
    encoderObj->sidGain = sidGain;
    encoderObj->gainNow = gainNow;
    encoderObj->ACFcounter = ACFcounter;

    LOCAL_ARRAY_FREE(Ipp32s, sumMem, LPF_DIM+1, encoderObj);
    LOCAL_ARRAY_FREE(Ipp16s, s_ACFsum, LPF_DIM+1, encoderObj);
    LOCAL_ARRAY_FREE(Ipp16s, lsfSid_q, LPF_DIM, encoderObj);
    LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, lsp, LPF_DIM, encoderObj);
    LOCAL_ARRAY_FREE(Ipp16s, newLSP, LPF_DIM, encoderObj);
    LOCAL_ARRAY_FREE(Ipp16s, Coeff, LPF_DIM+1, encoderObj);
    LOCAL_ARRAY_FREE(Ipp16s, bidArr, LPF_DIM, encoderObj);
    LOCAL_ARRAY_FREE(Ipp32s, acfInt, LPF_DIM+1, encoderObj);
    LOCAL_ARRAY_FREE(Ipp16s, curAcf, LPF_DIM+1, encoderObj);
    return;
}

void CNG_Update( Ipp16s *pVal1, Ipp16s pVal2, Ipp16s Vad, G729Encoder_Obj *encoderObj
               ) {
    Ipp16s i,*p1,*p2;
    LOCAL_ARRAY(Ipp32s, sumMem, LPF_DIM+1, encoderObj);
    Ipp16s *pACF = encoderObj->pACF;
    Ipp16s *ACFsfs = encoderObj->ACFsfs;
    Ipp16s ACFcounter = encoderObj->ACFcounter;

    p1 = pACF + ACF_DIM - 1;
    p2 = p1 - (LPF_DIM+1);
    for(i=0; i<(ACF_DIM-(LPF_DIM+1)); i++)
        *(p1--) = *(p2--);
    for(i=ACF_NOW-1; i>=1; i--)
        ACFsfs[i] = ACFsfs[i-1];
    ACFsfs[0] = (Ipp16s)(-(16 + pVal2));
    for(i=0; i<LPF_DIM+1; i++)
        pACF[i] = pVal1[i];
    ACFcounter++;
    if(ACFcounter == ACF_NOW) {
        ACFcounter = 0;
        if(Vad != 0)
            ACFsumUpd(encoderObj->ACFsum,encoderObj->ACFsumSfs,encoderObj->pACF,encoderObj->ACFsfs, sumMem);
    }
    encoderObj->ACFcounter = ACFcounter;
    LOCAL_ARRAY_FREE(Ipp32s, sumMem, LPF_DIM+1, encoderObj);
    return;
}
G729_CODECFUN( APIG729_Status, apiG729Encoder_InitBuff,
               (G729Encoder_Obj* encoderObj, Ipp8s *buff)) {
    if(NULL==encoderObj || NULL==buff)
        return APIG729_StsBadArgErr;

    encoderObj->Mem.base = buff;
    encoderObj->Mem.CurPtr = encoderObj->Mem.base;
    encoderObj->Mem.VecPtr = (Ipp32s *)(encoderObj->Mem.base+G729_ENCODER_SCRATCH_MEMORY_SIZE);

    return APIG729_StsNoErr;
}
static void vad_update_A(Ipp16s *pAq_t, Ipp16s *pAp_t, Ipp16s *exc, Ipp16s *speechHistory, Ipp16s *val1,
                         Ipp16s *wsp,  Ipp16s *resFilMem0,  Ipp8s* synFltw);
static void vad_update_I(Ipp16s *pAq_t, Ipp16s *pAp_t, Ipp16s *exc, Ipp16s *speech, Ipp16s *val1,
                         Ipp16s *wsp,  Ipp16s *resFilMem0,  Ipp8s* synFltw,  Ipp8s* synFltw0, Ipp16s *wfact1, Ipp16s *wfact2,
                         Ipp16s *pAp1, Ipp16s *pAp2, Ipp16s *resFilMem, Ipp16s *error, Ipp16s *pSynth, Ipp16s *pGain);
static APIG729_Status G729Encode
(G729Encoder_Obj* encoderObj,const Ipp16s *src, Ipp8u* dst, G729Codec_Type codecType , Ipp32s *frametype);
static APIG729_Status G729AEncode
(G729Encoder_Obj* encoderObj,const Ipp16s *src, Ipp8u* dst, Ipp32s *frametype);
static APIG729_Status G729BaseEncode
(G729Encoder_Obj* encoderObj,const Ipp16s *src, Ipp8u* dst, Ipp32s *frametype);

G729_CODECFUN(  APIG729_Status, apiG729Encode,
                (G729Encoder_Obj* encoderObj,const Ipp16s *src, Ipp8u* dst, G729Codec_Type codecType , Ipp32s *frametype)) {
    Ipp16s baseMode, vadEnable = (Ipp16s)(encoderObj->objPrm.mode == G729Encode_VAD_Enabled);
    if(encoderObj->objPrm.codecType != G729I_CODEC) encoderObj->codecType = encoderObj->objPrm.codecType;
    else encoderObj->codecType = codecType;
    baseMode = (Ipp16s)((encoderObj->codecType == G729_CODEC)&&(vadEnable != 1));
    if(codecType==G729A_CODEC) {
        if(G729AEncode(encoderObj,src,dst,frametype) != APIG729_StsNoErr) {
            return APIG729_StsErr;
        }
    } else if(baseMode) {
        if(G729BaseEncode(encoderObj,src,dst,frametype) != APIG729_StsNoErr) {
            return APIG729_StsErr;
        }
    } else {
        if(G729Encode(encoderObj,src,dst,codecType,frametype) != APIG729_StsNoErr) {
            return APIG729_StsErr;
        }
    }
    return APIG729_StsNoErr;
}

APIG729_Status G729Encode
(G729Encoder_Obj* encoderObj,const Ipp16s *src, Ipp8u* dst, G729Codec_Type codecType , Ipp32s *frametype) {

    LOCAL_ALIGN_ARRAY(32, Ipp16s, impResp, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, val1, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, tmpVec2, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, codeword, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, y1, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, y2, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAp_t,(LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAq_t,(LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, tmpvec, BWLPCF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAp1, BWLPCF1_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, tmp_not_used,LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAp2, BWLPCF1_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, interpLSF, LPF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, newLSF, LPF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp32s,   BWDacf, BWLPCF1_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp32s,   BWDacfHigh, BWLPCF1_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, BWDrc,BWLPCF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pBwdLPC2,2*BWLPCF1_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, nullArr, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, LTPresid, LP_SUBFRAME_DIM,encoderObj);

    LOCAL_ARRAY(Ipp16s,indexFC,2,encoderObj);
    LOCAL_ARRAY(Ipp16s,delay,2,encoderObj);
    LOCAL_ARRAY(Ipp16s,LAR,4,encoderObj);
    LOCAL_ARRAY(Ipp16s,wfact1,2,encoderObj);
    LOCAL_ARRAY(Ipp16s,wfact2,2,encoderObj);
    LOCAL_ARRAY(Ipp16s,LSPcode,2,encoderObj);
    LOCAL_ARRAY(Ipp16s,freqNow,LPF_DIM,encoderObj);
    Ipp16s *pAq, *pAp, bestOPLag, index, gainAc, gainCode, gainPit, tmp, improveFlag, subfr;
    Ipp8s *synFltw = encoderObj->synFltw;
    Ipp16s *pAna = encoderObj->encPrm,*anau;
    Ipp16s *speechHistory = encoderObj->speechHistory;
    Ipp16s *prevSubfrLSP = encoderObj->prevSubfrLSP;
    Ipp16s *prevSubfrLSPquant = encoderObj->prevSubfrLSPquant;
    Ipp16s betaPreFilter = encoderObj->betaPreFilter;
    Ipp16s *resFilMem0  = encoderObj->resFilMem0;
    Ipp16s *prevWgtSpeech = encoderObj->prevWgtSpeech;
    Ipp16s *wsp = prevWgtSpeech + MAX_PITCH_LAG;
    Ipp16s *prevExcitat = encoderObj->prevExcitat;
    Ipp16s *exc = prevExcitat + L_prevExcitat;
    Ipp32s   i, j, subfrIdx, valOpenDelay;
    Ipp16s vadEnable = (Ipp16s)(encoderObj->objPrm.mode == G729Encode_VAD_Enabled);
    Ipp8s *synFltw0 = encoderObj->synFltw0;
    Ipp16s *resFilMem  = encoderObj->resFilMem;
    Ipp16s *error  = resFilMem + BWLPCF_DIM;
    Ipp16s lpMode = 0;
    Ipp16s satFilt;
    Ipp16s mAp, mAq, m_a;
    Ipp16s *ppAp, *ppAq;
    Ipp16s tmp1, tmp2,avgLag;

    VADmemory_Obj* vM =  (VADmemory_Obj*)encoderObj->vadMem;
    if(NULL==src || NULL ==dst)
        return APIG729_StsBadArgErr;
    if(encoderObj->preProc == NULL)
        return APIG729_StsNotInitialized;
    if((codecType != G729_CODEC)&&(codecType != G729D_CODEC)&&(codecType != G729E_CODEC))
        return APIG729_StsBadCodecType;
    if(encoderObj->objPrm.objSize <= 0)
        return APIG729_StsNotInitialized;
    if(ENC_KEY != encoderObj->objPrm.key)
        return APIG729_StsBadCodecType;
    if(encoderObj->objPrm.codecType != G729I_CODEC) encoderObj->codecType = encoderObj->objPrm.codecType;
    else encoderObj->codecType = codecType;
    ippsZero_16s(nullArr, LP_SUBFRAME_DIM);
    ippsCopy_16s(src,encoderObj->speechHistory + SPEECH_BUF_DIM - LP_FRAME_DIM,LP_FRAME_DIM);
    ippsHighPassFilter_G729_16s_ISfs(
                                    encoderObj->speechHistory + SPEECH_BUF_DIM - LP_FRAME_DIM,LP_FRAME_DIM,12,encoderObj->preProc);
    {
        LOCAL_ALIGN_ARRAY(32, Ipp16s, rCoeff, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp16s, newLSP, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp16s, newQlsp, LPF_DIM+1,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp32s, autoR, VAD_LPC_DIM +2,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp16s, yVal, LP_WINDOW_DIM,encoderObj);
        Ipp16s Vad=1;
        Ipp16s norma;
        Ipp16s *rhNBE = &newQlsp[0];
        norma=1;
        ippsMul_NR_16s_Sfs(LPC_WINDOW,hammingWin,yVal,LP_WINDOW_DIM,15);
        j = LPF_DIM;
        if(vadEnable == 1)
            j = VAD_LPC_DIM ;
        while(ippsAutoCorr_NormE_16s32s(yVal,LP_WINDOW_DIM,autoR,j+1,&i)!=ippStsNoErr) {
            ippsRShiftC_16s_I(2,yVal,LP_WINDOW_DIM);
            norma+=4;
        }
        norma = (Ipp16s)(norma - i);
        for(i=0;i<LPF_DIM+1;i++)
            rhNBE[i] = (Ipp16s)(autoR[i]>>16);
        ippsLagWindow_G729_32s_I(autoR+1,VAD_LPC_DIM );
        if(ippsLevinsonDurbin_G729_32s16s(autoR, LPF_DIM, &pAp_t[LPF_DIM+1], rCoeff, &tmp ) == ippStsOverflow) {
            for(i=0; i<=LPF_DIM; i++) {
                pAp_t[LPF_DIM+1+i] = encoderObj->prevSubfrLPC[i];
            }
            rCoeff[0] = encoderObj->prevRC[0];
            rCoeff[1] = encoderObj->prevRC[1];
        } else {
            for(i=0; i<=LPF_DIM; i++) encoderObj->prevSubfrLPC[i] = pAp_t[LPF_DIM+1+i];
            encoderObj->prevRC[0] = rCoeff[0];
            encoderObj->prevRC[1] = rCoeff[1];
        }
        ippsLPCToLSP_G729_16s(&pAp_t[LPF_DIM+1], prevSubfrLSP, newLSP);
        if(vadEnable == 1) {
            ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);
            {
                LOCAL_ALIGN_ARRAY(32, Ipp16s, vecTmp_VAD_LPC_DIMP1, VAD_LPC_DIM +1, encoderObj);
                VoiceActivityDetect_G729(LPC_WINDOW,newLSF,autoR,norma,rCoeff[1],&Vad,encoderObj->vadMem, vecTmp_VAD_LPC_DIMP1);
                LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, vecTmp_VAD_LPC_DIMP1, VAD_LPC_DIM +1, encoderObj);
            }
            VADMusicDetection( encoderObj->codecType, autoR[0], norma,rCoeff ,encoderObj->pLag , encoderObj->pGain,
                               encoderObj->prevLPmode, &Vad,encoderObj->vadMem);
            CNG_Update(rhNBE,norma,Vad,encoderObj);
            if(Vad==0) {
                ippsMove_16s(&encoderObj->BWDsynth[LP_FRAME_DIM], &encoderObj->BWDsynth[0], SYNTH_BWD_DIM);
                if( encoderObj->prevLPmode == 0) {
                    ippsInterpolate_G729_16s(newLSP,prevSubfrLSP,tmpvec,LPF_DIM);
                    ippsLSPToLPC_G729_16s(tmpvec,pAp_t);
                    ippsLSPToLSF_Norm_G729_16s(tmpvec, interpLSF);
                    ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);
                } else {

                    ippsLSPToLPC_G729_16s(newLSP,pAp_t);
                    ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);
                    ippsCopy_16s(newLSF, interpLSF, LPF_DIM);
                }
                if(encoderObj->statGlobal > 10000) {
                    encoderObj->statGlobal -= 2621;
                    if( encoderObj->statGlobal < 10000)
                        encoderObj->statGlobal = 10000 ;
                }
                lpMode = 0;
                encoderObj->dominantBWDmode = 0;
                encoderObj->interpCoeff2_2 = 4506;

                /* the next frame's LSPs update*/
                ippsCopy_16s(newLSP, prevSubfrLSP, LPF_DIM);

                _ippsRCToLAR_G729_16s(rCoeff,LAR+2,2);
                ippsInterpolate_G729_16s(encoderObj->prevLAR,LAR+2,LAR,2);
                encoderObj->prevLAR[0] = LAR[2];
                encoderObj->prevLAR[1] = LAR[3];
                {
                    LOCAL_ALIGN_ARRAY(32, Ipp16s, PWGammaFactorMem, LPF_DIM, encoderObj);
                    _ippsPWGammaFactor_G729_16s(LAR,interpLSF,&encoderObj->prevSubfrSmooth,wfact1,wfact2, PWGammaFactorMem);
                    _ippsPWGammaFactor_G729_16s(LAR+2,newLSF,&encoderObj->prevSubfrSmooth,wfact1+1,wfact2+1, PWGammaFactorMem);
                    LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, PWGammaFactorMem, LPF_DIM, encoderObj);
                }
                CNG_encoder(prevExcitat, prevSubfrLSPquant, pAq_t, pAna, encoderObj);
                if(*pAna==2)
                    *pAna=1;
                vM->VADPPrev = vM->VADPrev;
                vM->VADPrev = Vad;
                vad_update_I(pAq_t, pAp_t, exc, PRESENT_SPEECH, val1,
                             wsp,  resFilMem0,  synFltw,  synFltw0, wfact1, wfact2,
                             pAp1, pAp2, resFilMem, error, encoderObj->pSynth, encoderObj->pGain);
                ippsCopy_16s(&pAq_t[LPF_DIM+1], encoderObj->pPrevFilt, LPF_DIM+1);
                for(i=LPF_DIM+1; i <BWLPCF1_DIM; i++) encoderObj->pPrevFilt[i] = 0;
                encoderObj->prevLPmode = lpMode;
                encoderObj->betaPreFilter = PITCH_SHARP_MIN;
                ippsMove_16s(&speechHistory[LP_FRAME_DIM], &speechHistory[0], SPEECH_BUF_DIM-LP_FRAME_DIM);
                ippsMove_16s(&prevWgtSpeech[LP_FRAME_DIM], &prevWgtSpeech[0], MAX_PITCH_LAG);
                ippsMove_16s(&prevExcitat[LP_FRAME_DIM], &prevExcitat[0], MAX_PITCH_LAG+INTERPOLATION_FILTER_DIM);
                anau = encoderObj->encPrm+1;
                if(pAna[0] == 0) {
                    *frametype=0;
                } else {
                    *frametype=1;
                    dst[0] = (Ipp8u)(((anau[0] & 1) << 7) | ((anau[1] & 31) << 2) | ((anau[2] & 15)>>2));
                    dst[1] = (Ipp8u)(((anau[2] & 3) << 6) | ((anau[3] & 31) << 1));
                }
                CLEAR_SCRATCH_MEMORY(encoderObj);
                return APIG729_StsNoErr;
            }
        }

        *pAna++ = CodecTypeToRate[encoderObj->codecType];
        encoderObj->seed = SEED_INIT;
        vM->VADPPrev = vM->VADPrev;
        vM->VADPrev = Vad;
        encoderObj->CNGidx = 0;
        LOCAL_ALIGN_ARRAY_FREE(32,Ipp16s, yVal, LP_WINDOW_DIM,encoderObj);

        if(encoderObj->codecType == G729_CODEC) {
            ippsLSPQuant_G729_16s(newLSP,(Ipp16s*)encoderObj->prevLSPfreq,newQlsp,pAna);
            pAna += 2;
        } else {
            if(encoderObj->codecType == G729E_CODEC) {
                ippsWinHybrid_G729E_16s32s(encoderObj->BWDsynth, BWDacf,
                                           (IppsWinHybridState_G729E_16s*)&encoderObj->hwState);
                BWDLagWindow(BWDacf, BWDacfHigh);
                if(ippsLevinsonDurbin_G729_32s16s(BWDacfHigh, BWLPCF_DIM, &pBwdLPC2[BWLPCF1_DIM], BWDrc, &tmp )
                   == ippStsOverflow) {
                    for(i=0; i<=BWLPCF_DIM; i++) {
                        pBwdLPC2[BWLPCF1_DIM+i] = encoderObj->pPrevBwdLPC[i];
                    }
                    BWDrc[0] = encoderObj->pPrevBwdRC[0];
                    BWDrc[1] = encoderObj->pPrevBwdRC[1];
                } else {
                    for(i=0; i<=BWLPCF_DIM; i++) encoderObj->pPrevBwdLPC[i] = pBwdLPC2[BWLPCF1_DIM+i];
                    encoderObj->pPrevBwdRC[0] = BWDrc[0];
                    encoderObj->pPrevBwdRC[1] = BWDrc[1];
                }

                satFilt = 0;
                for(i=BWLPCF1_DIM; i<2*BWLPCF1_DIM; i++) if(pBwdLPC2[i] >= IPP_MAX_16S)
                        satFilt = 1;
                if(satFilt == 1)
                    ippsCopy_16s(encoderObj->pBwdLPC2, &pBwdLPC2[BWLPCF1_DIM], BWLPCF1_DIM);
                else
                    ippsCopy_16s(&pBwdLPC2[BWLPCF1_DIM], encoderObj->pBwdLPC2, BWLPCF1_DIM);

                ippsMulPowerC_NR_16s_Sfs(&pBwdLPC2[BWLPCF1_DIM], N0_98, &pBwdLPC2[BWLPCF1_DIM], BWLPCF1_DIM,15);
            }
            ippsMove_16s(&encoderObj->BWDsynth[LP_FRAME_DIM], &encoderObj->BWDsynth[0], SYNTH_BWD_DIM);
            ippsLSPQuant_G729E_16s(newLSP,(const Ipp16s*)encoderObj->prevLSPfreq,freqNow,newQlsp,LSPcode);
        }

        if( encoderObj->prevLPmode == 0) {

            ippsInterpolate_G729_16s(newLSP,prevSubfrLSP,tmpvec,LPF_DIM);
            ippsLSPToLPC_G729_16s(tmpvec,pAp_t);
            ippsLSPToLSF_Norm_G729_16s(tmpvec, interpLSF);
            ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);

            ippsInterpolate_G729_16s(newQlsp,prevSubfrLSPquant,tmpvec,LPF_DIM);

            ippsLSPToLPC_G729_16s(tmpvec,&pAq_t[0]);
            ippsLSPToLPC_G729_16s(newQlsp,&pAq_t[LPF_DIM+1]);
        } else {
            ippsLSPToLPC_G729_16s(newLSP,pAp_t);
            ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);
            ippsCopy_16s(newLSF, interpLSF, LPF_DIM);
            ippsLSPToLPC_G729_16s(newQlsp,&pAq_t[LPF_DIM+1]);
            ippsMove_16s(&pAq_t[LPF_DIM+1], pAq_t, LPF_DIM+1);
        }
        if(encoderObj->codecType == G729E_CODEC) {
            SetLPCMode_G729E(PRESENT_SPEECH, pAq_t, pBwdLPC2, &lpMode,
                             newLSP, prevSubfrLSP, encoderObj);
        }

        /* the next frame's LSPs update*/

        ippsCopy_16s(newLSP,prevSubfrLSP,LPF_DIM);
        if(lpMode==0) {
            ippsCopy_16s(newQlsp,prevSubfrLSPquant,LPF_DIM);
            _ippsRCToLAR_G729_16s(rCoeff,LAR+2,2);
            mAq = LPF_DIM;
            mAp = LPF_DIM;
            if(encoderObj->dominantBWDmode == 0)
                pAp = pAp_t;
            else
                pAp = pAq_t;
            pAq = pAq_t;
            tmp = (Ipp16s)(encoderObj->prevLAR[0] + LAR[2]);
            LAR[0] = (Ipp16s)((tmp<0)? ~(( ~tmp) >> 1 ) : tmp>>1);
            tmp = (Ipp16s)(encoderObj->prevLAR[1] + LAR[3]);
            LAR[1] = (Ipp16s)((tmp<0)? ~(( ~tmp) >> 1 ) : tmp>>1);
            encoderObj->prevLAR[0] = LAR[2];
            encoderObj->prevLAR[1] = LAR[3];
            {
                LOCAL_ALIGN_ARRAY(32, Ipp16s, PWGammaFactorMem, LPF_DIM, encoderObj);
                _ippsPWGammaFactor_G729_16s(LAR,interpLSF,&encoderObj->prevSubfrSmooth,wfact1,wfact2, PWGammaFactorMem);
                _ippsPWGammaFactor_G729_16s(LAR+2,newLSF,&encoderObj->prevSubfrSmooth,wfact1+1,wfact2+1, PWGammaFactorMem);
                LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, PWGammaFactorMem, LPF_DIM, encoderObj);
            }
            ippsMulPowerC_NR_16s_Sfs(&pAp[0],wfact1[0], pAp1,LPF_DIM+1,15);
            ippsMulPowerC_NR_16s_Sfs(&pAp[0],wfact2[0], pAp2,LPF_DIM+1,15);

            ippsResidualFilter_G729_16s(&PRESENT_SPEECH[0],pAp1, &wsp[0]);
            ippsSynthesisFilterLow_NR_16s_ISfs(pAp2,wsp,LP_SUBFRAME_DIM,12,((SynthesisFilterState*)synFltw)->buffer+20);
            ippsCopy_16s((wsp+LP_SUBFRAME_DIM-30), ((SynthesisFilterState*)synFltw)->buffer, 30);

            ippsMulPowerC_NR_16s_Sfs(&pAp[LPF_DIM+1],wfact1[1], pAp1,LPF_DIM+1,15);
            ippsMulPowerC_NR_16s_Sfs(&pAp[LPF_DIM+1],wfact2[1], pAp2,LPF_DIM+1,15);
            ippsResidualFilter_G729_16s(&PRESENT_SPEECH[LP_SUBFRAME_DIM], pAp1, &wsp[LP_SUBFRAME_DIM]);
            ippsSynthesisFilterLow_NR_16s_ISfs(pAp2,&wsp[LP_SUBFRAME_DIM],LP_SUBFRAME_DIM,12,((SynthesisFilterState*)synFltw)->buffer+20);
            ippsCopy_16s((&wsp[LP_SUBFRAME_DIM]+LP_SUBFRAME_DIM-30), ((SynthesisFilterState*)synFltw)->buffer, 30);

            ippsOpenLoopPitchSearch_G729_16s(wsp, &bestOPLag);

            if((encoderObj->codecType != G729_CODEC)) {
                if(encoderObj->codecType == G729E_CODEC)
                    *pAna++ = lpMode;

                for( i = LSP_MA_ORDER-1 ; i > 0 ; i-- )
                    ippsMove_16s(encoderObj->prevLSPfreq[i-1], encoderObj->prevLSPfreq[i], LPF_DIM);
                ippsCopy_16s(freqNow, encoderObj->prevLSPfreq[0], LPF_DIM);
                *pAna++ = LSPcode[0]; *pAna++ = LSPcode[1];
            }
            ippsCopy_16s(&pAq[LPF_DIM+1], encoderObj->pPrevFilt, LPF_DIM+1);
            ippsZero_16s(encoderObj->pPrevFilt+LPF_DIM+1,BWLPCF1_DIM-LPF_DIM-1);
            ippsZero_16s(encoderObj->zeroPostFiltVec1+LPF_DIM+1,BWLPCF1_DIM-LPF_DIM-1);

            for(i= 0; i< 4; i++)
                encoderObj->pLag[i] = encoderObj->pLag[i+1];

            avgLag = (Ipp16s)(encoderObj->pLag[0]+encoderObj->pLag[1]);
            avgLag = (Ipp16s)(avgLag + encoderObj->pLag[2]);
            avgLag = (Ipp16s)(avgLag + encoderObj->pLag[3]);
            avgLag = (Ipp16s)((avgLag * BWF_HARMONIC_E + (Ipp32s)BWF_HARMONIC)>>15);

            tmp1 = (Ipp16s)(bestOPLag - (avgLag<<1));
            tmp2 = (Ipp16s)(bestOPLag - ((avgLag<<1)+avgLag));
            if( Abs_16s(tmp1) < 4) {
                encoderObj->pLag[4] = (Ipp16s)(bestOPLag>>1);
            } else if( Abs_16s(tmp2) < 6) {
                encoderObj->pLag[4] = (Ipp16s)((bestOPLag * 10923)>>15);
            } else {
                encoderObj->pLag[4] = bestOPLag;
            }
        } else {
            if(encoderObj->dominantBWDmode == 0) {
                mAp = LPF_DIM; pAp = pAp_t;
                wfact1[0] = wfact1[1] = COEFF1;
                wfact2[0] = wfact2[1] = 13107;
            } else {
                mAp = BWLPCF_DIM; pAp = pBwdLPC2;
                wfact1[0] = wfact1[1] = 32112;
                wfact2[0] = wfact2[1] = 13107;
            }

            ppAp = pAp;
            pAq = pBwdLPC2;
            mAq = BWLPCF_DIM;

            if(encoderObj->dominantBWDmode == 0) {
                for(j=LPF_DIM+1; j<BWLPCF1_DIM; j++) encoderObj->zeroPostFiltVec1[j] = 0;
            }
            /* next frame's previous filter update */
            ippsCopy_16s(&pAq[BWLPCF1_DIM], encoderObj->pPrevFilt, BWLPCF1_DIM);

            ippsMulPowerC_NR_16s_Sfs(&pAp[0],wfact1[0], pAp1,mAp+1,15);
            ippsMulPowerC_NR_16s_Sfs(&pAp[0],wfact2[0], pAp2,mAp+1,15);

            ippsResidualFilter_G729E_16s( pAp1, mAp,&PRESENT_SPEECH[0], &wsp[0], LP_SUBFRAME_DIM);
            SynthesisFilterOvf_G729_16s_I(pAp2,wsp,LP_SUBFRAME_DIM,synFltw,BWLPCF_DIM-mAp);

            /* weighted speech signal for 2-nd subframe */
            ippsMulPowerC_NR_16s_Sfs(&pAp[mAp+1],wfact1[1], pAp1,mAp+1,15);
            ippsMulPowerC_NR_16s_Sfs(&pAp[mAp+1],wfact2[1], pAp2,mAp+1,15);
            ippsResidualFilter_G729E_16s( pAp1,mAp, &PRESENT_SPEECH[LP_SUBFRAME_DIM], &wsp[LP_SUBFRAME_DIM], LP_SUBFRAME_DIM);
            SynthesisFilterOvf_G729_16s_I(pAp2,&wsp[LP_SUBFRAME_DIM],LP_SUBFRAME_DIM,synFltw,BWLPCF_DIM-mAp);
            if(encoderObj->codecType == G729E_CODEC) *pAna++ = lpMode;
            ippsOpenLoopPitchSearch_G729_16s(wsp, &bestOPLag);

            for(i= 0; i< 4; i++)
                encoderObj->pLag[i] = encoderObj->pLag[i+1];
            avgLag = (Ipp16s)(encoderObj->pLag[0]+encoderObj->pLag[1]);
            avgLag = (Ipp16s)(avgLag + encoderObj->pLag[2]);
            avgLag = (Ipp16s)(avgLag + encoderObj->pLag[3]);
            avgLag = (Ipp16s)((avgLag * BWF_HARMONIC_E)>>15);
            tmp1 = (Ipp16s)(bestOPLag - (avgLag<<1));
            tmp2 = (Ipp16s)(bestOPLag - ((avgLag<<1)+avgLag));
            if( Abs_16s(tmp1) < 4) {
                encoderObj->pLag[4] = (Ipp16s)(bestOPLag>>1);
            } else if( Abs_16s(tmp2) < 6) {
                encoderObj->pLag[4] = (Ipp16s)((bestOPLag * 10923)>>15);
            } else {
                encoderObj->pLag[4] = bestOPLag;
            }
        }
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp32s, autoR, VAD_LPC_DIM +2,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, newQlsp, LPF_DIM+1,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, newLSP, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, rCoeff, LPF_DIM,encoderObj);
    }
    ppAp = pAp;
    ppAq = pAq;
    valOpenDelay=bestOPLag;
    subfr = 0;
    for(subfrIdx = 0;  subfrIdx < LP_FRAME_DIM; subfrIdx += LP_SUBFRAME_DIM) {

        ippsMulPowerC_NR_16s_Sfs(ppAp,wfact1[subfr], pAp1,mAp+1,15);
        ippsMulPowerC_NR_16s_Sfs(ppAp,wfact2[subfr], pAp2,mAp+1,15);
        subfr++;

        ippsCopy_16s(pAp1,encoderObj->zeroPostFiltVec1,mAp+1);

        ippsSynthesisFilter_G729E_16s(ppAq, mAq,  encoderObj->zeroPostFiltVec1, impResp, LP_SUBFRAME_DIM, nullArr);
        ippsSynthesisFilter_G729E_16s_I(pAp2, mAp, impResp, LP_SUBFRAME_DIM, nullArr);

        ippsResidualFilter_G729E_16s( ppAq,mAq, &PRESENT_SPEECH[subfrIdx], &exc[subfrIdx], LP_SUBFRAME_DIM);
        ippsCopy_16s(&exc[subfrIdx],LTPresid,LP_SUBFRAME_DIM);
        ippsSynthesisFilter_G729E_16s(ppAq, mAq, &exc[subfrIdx], error, LP_SUBFRAME_DIM, &resFilMem[BWLPCF_DIM-mAq]);

        ippsResidualFilter_G729E_16s( pAp1, mAp,error, val1, LP_SUBFRAME_DIM);
        ippsSynthesisFilter_G729E_16s_I(pAp2, mAp, val1, LP_SUBFRAME_DIM, &resFilMem0[BWLPCF_DIM-mAp]);

        if(encoderObj->codecType == G729D_CODEC) {
            ippsAdaptiveCodebookSearch_G729D_16s((Ipp16s)valOpenDelay, val1,impResp,&prevExcitat[subfrIdx],(Ipp16s)(subfrIdx!=0),delay);
        } else {

            ippsAdaptiveCodebookSearch_G729_16s((Ipp16s)valOpenDelay, val1,impResp,&prevExcitat[subfrIdx],delay,tmp_not_used,(Ipp16s)(subfrIdx!=0));
        }

         if(subfrIdx == 0) {  /* 1-st subframe */
            if(delay[0] <= 85) {
                index = (Ipp16s)(3 * (delay[0] - 19) + delay[1] - 1);
            } else {
                index = (Ipp16s)(delay[0] - 85 + 197);
            }
        } else { /* 2-nd subframe */
            i = valOpenDelay - 5;
            if(i < MIN_PITCH_LAG) {
                i = MIN_PITCH_LAG;
            }
            if(i + 9 > MAX_PITCH_LAG) {
                i = MAX_PITCH_LAG - 9;
            }
            if(encoderObj->codecType == G729D_CODEC) {
                i = delay[0] - i;
                if(i < 3)
                    index = (Ipp16s)i;
                else if(i < 7) {
                    index = (Ipp16s)((i - 3) * 3 + delay[1] + 3);
                } else {
                    index = (Ipp16s)(i + 6);
                }
            } else {
                index = (Ipp16s)((delay[0] - i)*3 + 2 + delay[1]);
            }
        }
        valOpenDelay=delay[0];
        *pAna++ = index; /* Pitch delay for the current 1-st or 2-nd subframe  */
        if( (subfrIdx == 0) && (encoderObj->codecType != G729D_CODEC) ) {
            *pAna = (Ipp16s)equality(index);
            if( encoderObj->codecType == G729E_CODEC) *pAna = (Ipp16s)(*pAna ^ ((index>>1) & 1));
            pAna++;
        }
        if(encoderObj->codecType == G729_CODEC) {

            ippsAdaptiveCodebookGain_G729_16s(val1,impResp,&exc[subfrIdx],y1,&gainAc);
        } else {
            if(encoderObj->codecType == G729D_CODEC)
                ippsDecodeAdaptiveVector_G729_16s_I(delay,&prevExcitat[subfrIdx]);

            ippsAdaptiveCodebookGain_G729_16s(val1,impResp,&exc[subfrIdx],y1,&gainAc);
        }
        if(delay[1] > 0) {
            i = delay[0] + 1;
        } else {
            i = delay[0];
        }
        improveFlag = calcErr_G729(i, encoderObj->coderErr);
        if(improveFlag && gainAc > PITCH_GAIN_MAX) gainAc = PITCH_GAIN_MAX;

        ippsAdaptiveCodebookContribution_G729_16s(gainAc,y1,val1,tmpVec2);

        betaPreFilter = (Ipp16s)(betaPreFilter << 1);
        if(valOpenDelay < LP_SUBFRAME_DIM)
            ippsHarmonicFilter_16s_I(betaPreFilter,valOpenDelay,&impResp[valOpenDelay],LP_SUBFRAME_DIM-valOpenDelay);

        switch(encoderObj->codecType) {
            case G729_CODEC:
                {

                    LOCAL_ALIGN_ARRAY(32, Ipp32s, rr, CORR_DIM, encoderObj);
                    LOCAL_ALIGN_ARRAY(32, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
                    ippsCrossCorr_NormM_16s(impResp,tmpVec2,LP_SUBFRAME_DIM,Dn);

                    ippsToeplizMatrix_G729_16s32s(impResp, rr);
                    ippsFixedCodebookSearch_G729_32s16s(Dn, rr, codeword,indexFC,&encoderObj->extraTime, (Ipp16s)subfrIdx);
                    CodewordImpConv_G729(indexFC[1],codeword,impResp,y2);

                    *pAna++ = indexFC[1]; /*  index  of positions */
                    *pAna++ = indexFC[0]; /*  index of signs  */

                    LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
                    LOCAL_ALIGN_ARRAY_FREE(32, Ipp32s, rr, CORR_DIM, encoderObj);
                    break;
                }
            case G729D_CODEC:
                {
                    LOCAL_ALIGN_ARRAY(32, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
                    Ipp16s sTmpSignIndexes;

                    /*  target vector with impulse response correlation */
                    ippsCrossCorr_NormM_16s(impResp,tmpVec2,LP_SUBFRAME_DIM,Dn);

                    ippsFixedCodebookSearch_G729D_16s( Dn, impResp, codeword, y2, &sTmpSignIndexes,&index);

                    *pAna++ = index;/*  index  of positions */
                    *pAna++ = sTmpSignIndexes;  /*  index of signs  */
                    LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
                    break;

                }
            case G729E_CODEC:
                {

                    /* calculate residual after LTP */
                    ippsAdaptiveCodebookContribution_G729_16s(gainAc,&exc[subfrIdx],LTPresid,LTPresid);
                    /* lpMode - 0=Forward, 1=Backward*/
                    ippsFixedCodebookSearch_G729E_16s(lpMode,tmpVec2, LTPresid, impResp, codeword, y2, pAna);
                    pAna += 5;
                    break;
                }
            default:
                {
                    LOCAL_ALIGN_ARRAY(32, Ipp32s, rr, CORR_DIM, encoderObj);
                    LOCAL_ALIGN_ARRAY(32, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
                    /* correlation of target vector with impulse response */
                    ippsCrossCorr_NormM_16s(impResp,tmpVec2,LP_SUBFRAME_DIM,Dn);

                    /*  impulse response auto correlation matrix */
                    ippsToeplizMatrix_G729_16s32s(impResp, rr);
                    ippsFixedCodebookSearch_G729_32s16s(Dn, rr, codeword,indexFC,&encoderObj->extraTime, (Ipp16s)subfrIdx);
                    CodewordImpConv_G729(indexFC[1],codeword,impResp,y2);

                    *pAna++ = indexFC[1];/*  index  of positions */
                    *pAna++ = indexFC[0];/*  index of signs  */

                    encoderObj->codecType = G729_CODEC;
                    LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
                    LOCAL_ALIGN_ARRAY_FREE(32, Ipp32s, rr, CORR_DIM, encoderObj);
                    break;
                }
        }

        /* include fixed-gain pitch contribution into codeword */
        if(valOpenDelay < LP_SUBFRAME_DIM)
            ippsHarmonicFilter_16s_I(betaPreFilter,valOpenDelay,&codeword[valOpenDelay],LP_SUBFRAME_DIM-valOpenDelay);

        if(encoderObj->codecType != G729D_CODEC) {
            LOCAL_ARRAY(Ipp16s, gains, 2, encoderObj);
            LOCAL_ARRAY(Ipp16s, gInd, 2, encoderObj);
            ippsGainQuant_G729_16s(val1, y1, codeword, y2, encoderObj->prevFrameQuantEn, gains, gInd,improveFlag);
            gainPit  = gains[0];
            gainCode = gains[1];
            *pAna++ = (Ipp16s)((LUT1[gInd[0]]<<CDBK2_BIT_NUM) | LUT2[gInd[1]]);

            LOCAL_ARRAY_FREE(Ipp16s, gInd, 2, encoderObj);
            LOCAL_ARRAY_FREE(Ipp16s, gains, 2, encoderObj);
        } else {
            LOCAL_ARRAY(Ipp16s, gains, 2, encoderObj);
            LOCAL_ARRAY(Ipp16s, gInd, 2, encoderObj);
            ippsGainQuant_G729D_16s(val1,y1,codeword,y2,encoderObj->prevFrameQuantEn,gains,gInd,improveFlag);
            gainPit  = gains[0];
            gainCode = gains[1];
            *pAna++ = (Ipp16s)((LUT1_6k[gInd[0]]<<CDBK2_BIT_NUM_6K) | LUT2_6k[gInd[1]]);

            LOCAL_ARRAY_FREE(Ipp16s, gInd, 2, encoderObj);
            LOCAL_ARRAY_FREE(Ipp16s, gains, 2, encoderObj);
        }

        updateExcErr_G729(gainPit, valOpenDelay,encoderObj->coderErr);

        for(i= 0; i< 4; i++)
            encoderObj->pGain[i] = encoderObj->pGain[i+1];
        encoderObj->pGain[4] = gainPit;

        betaPreFilter = gainPit;
        if(betaPreFilter > PITCH_SHARP_MAX)  betaPreFilter = PITCH_SHARP_MAX;
        if(betaPreFilter < PITCH_SHARP_MIN)  betaPreFilter = PITCH_SHARP_MIN;

        ippsInterpolateC_NR_G729_16s_Sfs(
                                        &exc[subfrIdx],gainPit,codeword,gainCode,&exc[subfrIdx],LP_SUBFRAME_DIM,14);
        SynthesisFilter_G729_16s(ppAq,&exc[subfrIdx],&encoderObj->pSynth[subfrIdx],LP_SUBFRAME_DIM,synFltw0,BWLPCF_DIM-mAq);
        ippsSub_16s(&encoderObj->pSynth[subfrIdx+LP_SUBFRAME_DIM-BWLPCF_DIM],&PRESENT_SPEECH[subfrIdx+LP_SUBFRAME_DIM-BWLPCF_DIM],encoderObj->resFilMem/*+20*/,BWLPCF_DIM);
        m_a = BWLPCF_DIM;
        if(gainCode <= IPP_MAX_16S/2 && gainCode >= IPP_MIN_16S/2) {
            ippsInterpolateC_G729_16s_Sfs(
                                         &y1[LP_SUBFRAME_DIM-m_a],gainPit,&y2[LP_SUBFRAME_DIM-m_a],(Ipp16s)(2*gainCode),tmpvec,m_a,14);
        } else {

            for(i = 0; i < m_a; i++) {
                tmpvec[i] = (Ipp16s)(((gainPit*y1[LP_SUBFRAME_DIM-m_a+i])>>14)+((gainCode*y2[LP_SUBFRAME_DIM-m_a+i])>>13));
            }
        }
        ippsSub_16s(tmpvec,&val1[LP_SUBFRAME_DIM-m_a],resFilMem0,m_a);
        ppAq += mAq+1;
        ppAp += mAp+1;
    }

    ippsMove_16s(&speechHistory[LP_FRAME_DIM], &speechHistory[0], SPEECH_BUF_DIM-LP_FRAME_DIM);
    ippsMove_16s(&prevWgtSpeech[LP_FRAME_DIM], &prevWgtSpeech[0], MAX_PITCH_LAG);
    ippsMove_16s(&prevExcitat[LP_FRAME_DIM], &prevExcitat[0], L_prevExcitat);
    encoderObj->prevLPmode = lpMode;

    encoderObj->betaPreFilter = betaPreFilter;
    if((encoderObj->codecType == G729_CODEC)&&(vadEnable != 1)) {
        anau = encoderObj->encPrm;
    } else if(encoderObj->codecType == G729E_CODEC) {
        anau = encoderObj->encPrm+2;
    } else {
        anau = encoderObj->encPrm+1;
    }
    if(encoderObj->codecType == G729_CODEC) {
        *frametype=3;
        dst[0] = (Ipp8u)(anau[0] & 255);
        dst[1] = (Ipp8u)((anau[1] & 0x3ff) >> 2);
        dst[2] = (Ipp8u)(((anau[1] & 3) << 6) | ((anau[2]>>2)&0x3f));
        dst[3] = (Ipp8u)(((anau[2] & 3) << 6) | ((anau[3] & 1) << 5) | ((anau[4] & 8191) >> 8));
        dst[4] = (Ipp8u)(anau[4] & 255);
        dst[5] = (Ipp8u)(((anau[5] & 15)<<4) | ((anau[6] & 127) >> 3));
        dst[6] = (Ipp8u)(((anau[6] & 7)<< 5) | (anau[7] & 31));
        dst[7] = (Ipp8u)((anau[8] & 8191) >> 5);
        dst[8] = (Ipp8u)(((anau[8] & 31) << 3) | ((anau[9] & 15) >> 1));
        dst[9] = (Ipp8u)(((anau[9] & 1) << 7) | (anau[10] & 127));
    } else if(encoderObj->codecType == G729D_CODEC) {
        *frametype=2;
        dst[0] = (Ipp8u)(anau[0] & 255);
        dst[1] = (Ipp8u)((anau[1] & 0x3ff) >> 2);
        dst[2] = (Ipp8u)(((anau[1] & 3) << 6) | ((anau[2]>>2)&0x3f));
        dst[3] = (Ipp8u)(((anau[2] & 3) << 6) | ((anau[3]>>3)&0x3f));
        dst[4] = (Ipp8u)(((anau[3] & 7) << 5) | ((anau[4] & 3) << 3) | ((anau[5] >> 3)& 7));
        dst[5] = (Ipp8u)(((anau[5] & 7) << 5) | ((anau[6] & 15) << 1)| ((anau[7] >> 8)& 1));
        dst[6] = (Ipp8u)(anau[7] & 255);
        dst[7] = (Ipp8u)((anau[8] & 3) << 6 | (anau[9] & 0x3f));
    } else if(encoderObj->codecType == G729E_CODEC) {
        *frametype=4;
        if(lpMode == 0) { /* forward */
            dst[0] = (Ipp8u)((anau[0] >> 2) & 0x3f);
            dst[1] = (Ipp8u)(((anau[0] & 3) << 6) | ((anau[1]>>4)&0x3f));
            dst[2] = (Ipp8u)(((anau[1] & 15) << 4) | ((anau[2]>>4)&15));
            dst[3] = (Ipp8u)(((anau[2] & 15) << 4) | ((anau[3]&1)<<3) | ((anau[4]>>4)&7));
            dst[4] = (Ipp8u)(((anau[4] & 15) << 4) | ((anau[5]>>3)&15));
            dst[5] = (Ipp8u)(((anau[5] & 7) << 5) | ((anau[6]>>2)&31));
            dst[6] = (Ipp8u)(((anau[6] & 3) << 6) | ((anau[7]>>1)&0x3f));
            dst[7] = (Ipp8u)(((anau[7]& 1) << 7)  | (anau[8]&127));
            dst[8] = (Ipp8u)(((anau[9]& 127) << 1) | ((anau[10]>>4)&1));
            dst[9] = (Ipp8u)(((anau[10] & 15) << 4) | ((anau[11]>>3)&15));
            dst[10] = (Ipp8u)(((anau[11] & 7) << 5) | ((anau[12]>>2)&31));
            dst[11] = (Ipp8u)(((anau[12] & 3) << 6) | ((anau[13]>>1)&0x3f));
            dst[12] = (Ipp8u)(((anau[13]& 1) << 7)  | (anau[14]&127));
            dst[13] = (Ipp8u)(((anau[15]& 127) << 1) | ((anau[16]>>6)&1));
            dst[14] = (Ipp8u)(((anau[16] & 0x3f) << 2));
        } else { /* backward */
            dst[0] = (Ipp8u)((3<<6) | ((anau[0] >> 2) & 0x3f));
            dst[1] = (Ipp8u)(((anau[0] & 3) << 6) | ((anau[1]&1)<<5) | ((anau[2]>>8)&31));
            dst[2] = (Ipp8u)(anau[2] & 255);
            dst[3] = (Ipp8u)((anau[3] >> 2) & 255);
            dst[4] = (Ipp8u)(((anau[3] & 3) << 6) | ((anau[4]>>1)&0x3f));
            dst[5] = (Ipp8u)(((anau[4]& 1) << 7)  | (anau[5]&127));
            dst[6] = (Ipp8u)(((anau[6]& 127) << 1) | ((anau[7]>>6)&1));
            dst[7] = (Ipp8u)(((anau[7]&0x3f) << 2) | ((anau[8] >>3)&3));
            dst[8] = (Ipp8u)(((anau[8] & 7) << 5) | ((anau[9]>>8)&31));
            dst[9] = (Ipp8u)(anau[9] & 255);
            dst[10] = (Ipp8u)((anau[10] >> 2) & 255);
            dst[11] = (Ipp8u)(((anau[10] & 3) << 6) | ((anau[11]>>1)&0x3f));
            dst[12] = (Ipp8u)(((anau[11]& 1) << 7)  | (anau[12]&127));
            dst[13] = (Ipp8u)(((anau[13]& 127) << 1) | ((anau[14]>>6)&1));
            dst[14] = (Ipp8u)(((anau[14] & 0x3f) << 2));
        }
    }
    CLEAR_SCRATCH_MEMORY(encoderObj);
    return APIG729_StsNoErr;
}

G729_CODECFUN(  APIG729_Status, apiG729EncodeVAD,
                (G729Encoder_Obj* encoderObj,const Ipp16s *src, Ipp16s *dst, G729Codec_Type codecType, Ipp32s *frametype )) {
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAp_t,(LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAq_t,(LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, rCoeff, LPF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, newLSP, LPF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, rhNBE, LPF_DIM+1,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp32s, autoR, VAD_LPC_DIM +2,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, tmpvec, BWLPCF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, val1, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, newLSF, LPF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, interpLSF, LPF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAp1, BWLPCF1_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAp2, BWLPCF1_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, yVal, LP_WINDOW_DIM,encoderObj);
    LOCAL_ARRAY(Ipp16s,LAR,4,encoderObj);
    LOCAL_ARRAY(Ipp16s,wfact1,2,encoderObj);
    LOCAL_ARRAY(Ipp16s,wfact2,2,encoderObj);
    Ipp16s s,norma,Vad=1,tmp,lpMode;
    Ipp16s *speechHistory = encoderObj->speechHistory;
    Ipp16s *prevSubfrLSP = encoderObj->prevSubfrLSP;
    Ipp16s vadEnable = (Ipp16s)((encoderObj->objPrm.mode == G729Encode_VAD_Enabled));
    Ipp16s *prevSubfrLSPquant = encoderObj->prevSubfrLSPquant;
    Ipp16s *resFilMem0  = encoderObj->resFilMem0;
    Ipp16s *prevWgtSpeech = encoderObj->prevWgtSpeech;
    Ipp16s *wsp = prevWgtSpeech + MAX_PITCH_LAG;
    Ipp16s *prevExcitat = encoderObj->prevExcitat;
    Ipp16s *exc = prevExcitat + L_prevExcitat;
    Ipp16s *pAna = encoderObj->encPrm,*anau;
    Ipp8s *synFltw = encoderObj->synFltw;
    Ipp8s *synFltw0 = encoderObj->synFltw0;
    Ipp16s *resFilMem  = encoderObj->resFilMem;
    Ipp16s *error  = resFilMem + BWLPCF_DIM;
    Ipp32s   i;

    VADmemory_Obj* vM =  (VADmemory_Obj*)encoderObj->vadMem;
    if(NULL==encoderObj || NULL==src || NULL ==dst)
        return APIG729_StsBadArgErr;
    if(encoderObj->preProc == NULL)
        return APIG729_StsNotInitialized;
    if((codecType != G729_CODEC)&&(codecType != G729A_CODEC)
       &&(codecType != G729D_CODEC)&&(codecType != G729E_CODEC))
        return APIG729_StsBadCodecType;
    if(encoderObj->objPrm.objSize <= 0)
        return APIG729_StsNotInitialized;
    if(ENC_KEY != encoderObj->objPrm.key)
        return APIG729_StsBadCodecType;

    if(encoderObj->objPrm.codecType != G729I_CODEC) encoderObj->codecType = encoderObj->objPrm.codecType;
    else encoderObj->codecType = codecType;

    if((encoderObj->codecType == G729_CODEC)||(encoderObj->codecType == G729A_CODEC)) *frametype = 3;
    else if(encoderObj->codecType == G729D_CODEC) *frametype = 2;
    else if(encoderObj->codecType == G729E_CODEC) *frametype = 4;

    if(!vadEnable) return APIG729_StsNoErr;

    ippsCopy_16s(src,encoderObj->speechHistory + SPEECH_BUF_DIM - LP_FRAME_DIM,LP_FRAME_DIM);
    ippsHighPassFilter_G729_16s_ISfs(
                                    encoderObj->speechHistory + SPEECH_BUF_DIM - LP_FRAME_DIM,LP_FRAME_DIM,12,encoderObj->preProc);
    norma=1;
    ippsMul_NR_16s_Sfs(LPC_WINDOW,hammingWin,yVal,LP_WINDOW_DIM,15);
    while(ippsAutoCorr_NormE_16s32s(yVal,LP_WINDOW_DIM,autoR,VAD_LPC_DIM +1,&i)!=ippStsNoErr) {
        ippsRShiftC_16s_I(2,yVal,LP_WINDOW_DIM);
        norma+=4;
    }
    norma = (Ipp16s)(norma - i);
    for(i=0; i<LPF_DIM+1; i++)
        rhNBE[i] = (Ipp16s)(autoR[i] >> 16);
    ippsLagWindow_G729_32s_I(autoR+1,VAD_LPC_DIM );

    if(encoderObj->codecType == G729A_CODEC) {
        if(ippStsOverflow == ippsLevinsonDurbin_G729B(autoR, pAp_t,rCoeff,&s)) {
            ippsCopy_16s(encoderObj->prevSubfrLPC,pAp_t,LPF_DIM+1);
            rCoeff[0] = encoderObj->prevRC[0];
            rCoeff[1] = encoderObj->prevRC[1];
        } else {
            ippsCopy_16s(pAp_t,encoderObj->prevSubfrLPC,LPF_DIM+1);
            encoderObj->prevRC[0] = rCoeff[0];
            encoderObj->prevRC[1] = rCoeff[1];
        }

        ippsLPCToLSP_G729A_16s(pAp_t, prevSubfrLSP, newLSP);
        ippsLSPToLSF_Norm_G729_16s(newLSP, tmpvec);
        {
            LOCAL_ALIGN_ARRAY(32, Ipp16s, tmpVecVAD_LPC_DIM2, VAD_LPC_DIM +1, encoderObj);
            VoiceActivityDetect_G729(LPC_WINDOW,tmpvec,autoR,norma,rCoeff[1],&Vad,encoderObj->vadMem, tmpVecVAD_LPC_DIM2);
            LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, tmpVecVAD_LPC_DIM2, VAD_LPC_DIM +1, encoderObj);
        }
        CNG_Update(rhNBE,norma,Vad,encoderObj);
        if(Vad == 0) {

            CNG_encoder(prevExcitat, prevSubfrLSPquant, pAq_t, pAna, encoderObj);
            vad_update_A(pAq_t,pAp_t,exc,PRESENT_SPEECH,val1,wsp,resFilMem0,synFltw);
            vM->VADPPrev = vM->VADPrev;
            vM->VADPrev = Vad;
            encoderObj->betaPreFilter = PITCH_SHARP_MIN;

            ippsMove_16s(&speechHistory[LP_FRAME_DIM], &speechHistory[0], SPEECH_BUF_DIM-LP_FRAME_DIM);
            ippsMove_16s(&prevWgtSpeech[LP_FRAME_DIM], &prevWgtSpeech[0], MAX_PITCH_LAG);
            ippsMove_16s(&prevExcitat[LP_FRAME_DIM], &prevExcitat[0], L_prevExcitat);

            anau = encoderObj->encPrm+1;
            if(pAna[0] == 0) {
                *frametype=0;
            } else {
                *frametype=1;
                dst[0] = anau[0];
                dst[1] = anau[1];
                dst[2] = anau[2];
                dst[3] = anau[3];
            }
            CLEAR_SCRATCH_MEMORY(encoderObj);
            return APIG729_StsNoErr;
        }
        *pAna++ = 1;
        encoderObj->seed = SEED_INIT;
        encoderObj->CNGidx = 0;
        vM->VADPPrev = vM->VADPrev;
        vM->VADPrev = Vad;
    } else {
        if(ippsLevinsonDurbin_G729_32s16s(autoR, LPF_DIM, &pAp_t[LPF_DIM+1], rCoeff, &tmp ) == ippStsOverflow) {
            ippsCopy_16s(encoderObj->prevSubfrLPC,&pAp_t[LPF_DIM+1],LPF_DIM+1);
            rCoeff[0] = encoderObj->prevRC[0];
            rCoeff[1] = encoderObj->prevRC[1];
        } else {
            ippsCopy_16s(&pAp_t[LPF_DIM+1],encoderObj->prevSubfrLPC,LPF_DIM+1);
            encoderObj->prevRC[0] = rCoeff[0];
            encoderObj->prevRC[1] = rCoeff[1];
        }
        ippsLPCToLSP_G729_16s(&pAp_t[LPF_DIM+1], prevSubfrLSP, newLSP);

        ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);
        {
            LOCAL_ALIGN_ARRAY(32, Ipp16s, tmpVecVAD_LPC_DIM3, VAD_LPC_DIM +1, encoderObj);
            VoiceActivityDetect_G729(LPC_WINDOW,newLSF,autoR,norma,rCoeff[1],&Vad,encoderObj->vadMem, tmpVecVAD_LPC_DIM3);
            LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, tmpVecVAD_LPC_DIM3, VAD_LPC_DIM +1, encoderObj);
        }

        VADMusicDetection( encoderObj->codecType, autoR[0], norma,rCoeff ,encoderObj->pLag , encoderObj->pGain,
                           encoderObj->prevLPmode, &Vad,encoderObj->vadMem);
        CNG_Update(rhNBE,norma,Vad,encoderObj);

        if(Vad==0) {
            ippsMove_16s(&encoderObj->BWDsynth[LP_FRAME_DIM], &encoderObj->BWDsynth[0], SYNTH_BWD_DIM);

            if( encoderObj->prevLPmode == 0) {
                ippsInterpolate_G729_16s(newLSP,prevSubfrLSP,tmpvec,LPF_DIM);
                ippsLSPToLPC_G729_16s(tmpvec,pAp_t);
                ippsLSPToLSF_Norm_G729_16s(tmpvec, interpLSF);
                ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);
            } else {
                ippsLSPToLPC_G729_16s(newLSP,pAp_t);
                ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);
                ippsCopy_16s(newLSF, interpLSF, LPF_DIM);
            }
            if(encoderObj->statGlobal > 10000) {
                encoderObj->statGlobal -= 2621;
                if( encoderObj->statGlobal < 10000)
                    encoderObj->statGlobal = 10000 ;
            }
            lpMode = 0;
            encoderObj->dominantBWDmode = 0;
            encoderObj->interpCoeff2_2 = 4506;

            ippsCopy_16s(newLSP, prevSubfrLSP, LPF_DIM);

            _ippsRCToLAR_G729_16s(rCoeff,LAR+2,2);
            ippsInterpolate_G729_16s(encoderObj->prevLAR,LAR+2,LAR,2);
            encoderObj->prevLAR[0] = LAR[2];
            encoderObj->prevLAR[1] = LAR[3];
            {
                LOCAL_ALIGN_ARRAY(32, Ipp16s, PWGammaFactorMem, LPF_DIM, encoderObj);
                _ippsPWGammaFactor_G729_16s(LAR,interpLSF,&encoderObj->prevSubfrSmooth,wfact1,wfact2, PWGammaFactorMem);
                _ippsPWGammaFactor_G729_16s(LAR+2,newLSF,&encoderObj->prevSubfrSmooth,wfact1+1,wfact2+1, PWGammaFactorMem);
                LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, PWGammaFactorMem, LPF_DIM, encoderObj);
            }

            CNG_encoder(prevExcitat, prevSubfrLSPquant, pAq_t, pAna, encoderObj);
            if(*pAna==2) *pAna=1;
            vM->VADPPrev = vM->VADPrev;
            vM->VADPrev = Vad;

            vad_update_I(pAq_t, pAp_t, exc, PRESENT_SPEECH, val1,
                         wsp,  resFilMem0,  synFltw,  synFltw0, wfact1, wfact2,
                         pAp1, pAp2, resFilMem, error, encoderObj->pSynth, encoderObj->pGain);

            ippsCopy_16s(&pAq_t[LPF_DIM+1], encoderObj->pPrevFilt, LPF_DIM+1);
            for(i=LPF_DIM+1; i <BWLPCF1_DIM; i++) encoderObj->pPrevFilt[i] = 0;
            encoderObj->prevLPmode = lpMode;

            encoderObj->betaPreFilter = PITCH_SHARP_MIN;

            ippsMove_16s(&speechHistory[LP_FRAME_DIM], &speechHistory[0], SPEECH_BUF_DIM-LP_FRAME_DIM);
            ippsMove_16s(&prevWgtSpeech[LP_FRAME_DIM], &prevWgtSpeech[0], MAX_PITCH_LAG);
            ippsMove_16s(&prevExcitat[LP_FRAME_DIM], &prevExcitat[0], MAX_PITCH_LAG+INTERPOLATION_FILTER_DIM);

            anau = encoderObj->encPrm+1;
            if(pAna[0] == 0) {
                *frametype=0;
            } else {
                *frametype=1;
                dst[0] = anau[0];
                dst[1] = anau[1];
                dst[2] = anau[2];
                dst[3] = anau[3];
            }
            CLEAR_SCRATCH_MEMORY(encoderObj);
            return APIG729_StsNoErr;
        }
        *pAna++ = CodecTypeToRate[encoderObj->codecType];
        encoderObj->seed = SEED_INIT;
        vM->VADPPrev = vM->VADPrev;
        vM->VADPrev = Vad;
        encoderObj->CNGidx = 0;
        LOCAL_ARRAY_FREE(Ipp16s, yVal, LP_WINDOW_DIM,encoderObj);
    }
    ippsMove_16s(&speechHistory[LP_FRAME_DIM], &speechHistory[0], SPEECH_BUF_DIM-LP_FRAME_DIM);
    ippsMove_16s(&prevWgtSpeech[LP_FRAME_DIM], &prevWgtSpeech[0], MAX_PITCH_LAG);
    ippsMove_16s(&prevExcitat[LP_FRAME_DIM], &prevExcitat[0], L_prevExcitat);

    CLEAR_SCRATCH_MEMORY(encoderObj);
    return APIG729_StsNoErr;
}

static void ACFsumUpd(Ipp16s *ACFsum,Ipp16s *ACFsumSfs,Ipp16s *pACF,Ipp16s *ACFsfs, Ipp32s* pSumMem) {
    Ipp16s i, *p = &ACFsum[TOTAL_ACF_DIM - 1];
    for(i=0; i<TOTAL_ACF_DIM - (LPF_DIM+1); i++) {
        p[- i] = p[- i - (LPF_DIM+1)];
    }
    for(i=ACF_TOTAL-1; i>=1; i--) {
        ACFsumSfs[i] = ACFsumSfs[i-1];
    }
    Sum_G729_16s_Sfs(pACF, ACFsfs, ACFsum, ACFsumSfs, ACF_NOW,pSumMem);
    return;
}

void vad_update_A(Ipp16s *pAq_t, Ipp16s *pAp_t, Ipp16s *exc, Ipp16s *speech, Ipp16s *val1,
                  Ipp16s *wsp,  Ipp16s *resFilMem0,  Ipp8s *synFltw) {
    Ipp16s *pAq = pAq_t,*pAp, tmp;
    Ipp32s   i;
    SynthesisFilterState *SynFltSt=(SynthesisFilterState*)synFltw;

    for(i=0; i < LP_FRAME_DIM; i += LP_SUBFRAME_DIM) {

        ippsMul_NR_16s_Sfs(gammaFac1,&pAq[0],&pAp_t[0],LPF_DIM+1,15);
        pAp = pAp_t + LPF_DIM+1;
        tmp = 0;
        ippsPreemphasize_G729A_16s(BWF2,pAp_t,pAp,LPF_DIM+1,&tmp);

        ippsResidualFilter_G729_16s(&speech[i],pAq,val1);

        ippsSynthesisFilter_NR_16s_Sfs(pAp,val1,&wsp[i],LP_SUBFRAME_DIM, 12, SynFltSt->buffer);
        ippsCopy_16s((&wsp[i]+LP_SUBFRAME_DIM-LPF_DIM), SynFltSt->buffer, LPF_DIM);

        ippsSub_16s_I(&exc[i],val1,LP_SUBFRAME_DIM);
        ippsSynthesisFilterLow_NR_16s_ISfs(pAp_t,val1,LP_SUBFRAME_DIM,12,resFilMem0);
        ippsCopy_16s(&val1[LP_SUBFRAME_DIM-LPF_DIM],resFilMem0,LPF_DIM);
        pAq += LPF_DIM+1;
    }
}
void vad_update_I(Ipp16s *pAq_t, Ipp16s *pAp_t, Ipp16s *exc, Ipp16s *speech, Ipp16s *val1,
                  Ipp16s *wsp,  Ipp16s *resFilMem0,  Ipp8s* synFltw,  Ipp8s* synFltw0, Ipp16s *wfact1, Ipp16s *wfact2,
                  Ipp16s *pAp1, Ipp16s *pAp2, Ipp16s *resFilMem, Ipp16s *error, Ipp16s *pSynth,
                  Ipp16s *pGain) {
    Ipp16s *pAp=pAp_t,*pAq = pAq_t,subfrIdx,subfr = 0;
    Ipp32s k;
    for(subfrIdx=0; subfrIdx < LP_FRAME_DIM; subfrIdx += LP_SUBFRAME_DIM) {
        ippsMulPowerC_NR_16s_Sfs(pAp,wfact1[subfr], pAp1,LPF_DIM+1,15);
        ippsMulPowerC_NR_16s_Sfs(pAp,wfact2[subfr], pAp2,LPF_DIM+1,15);

        subfr++;
        ippsResidualFilter_G729_16s(&speech[subfrIdx], pAp1, &wsp[subfrIdx]);
        SynthesisFilterOvf_G729_16s_I(pAp2,&wsp[subfrIdx],LP_SUBFRAME_DIM,synFltw,20);

        SynthesisFilter_G729_16s(pAq,&exc[subfrIdx],&pSynth[subfrIdx],LP_SUBFRAME_DIM,synFltw0,BWLPCF_DIM-LPF_DIM);

        for(k=0; k<LP_SUBFRAME_DIM; k++)
            error[k] = (Ipp16s)(speech[subfrIdx+k] - pSynth[subfrIdx+k]);
        ippsResidualFilter_G729_16s( error, pAp1, val1);
        ippsSynthesisFilter_NR_16s_ISfs(pAp2, val1,LP_SUBFRAME_DIM, 12, &resFilMem0[BWLPCF_DIM-LPF_DIM]);

        ippsCopy_16s(&val1[LP_SUBFRAME_DIM-BWLPCF_DIM],resFilMem0,BWLPCF_DIM);
        ippsCopy_16s(&error[LP_SUBFRAME_DIM-BWLPCF_DIM],resFilMem,BWLPCF_DIM);
        for(k= 0; k< 4; k++)
            pGain[k] = pGain[k+1];
        pGain[4] =  BWF_HARMONIC_E;

        pAp += LPF_DIM+1;
        pAq += LPF_DIM+1;
    }
}

APIG729_Status G729AEncode
(G729Encoder_Obj* encoderObj,const Ipp16s *src, Ipp8u* dst, Ipp32s *frametype) {

    LOCAL_ALIGN_ARRAY(32, Ipp16s, impResp, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, val1, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, tmpVec2, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, codeword, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, y1, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, y2, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAp_t,(LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAq_t,(LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, tmpvec, LPF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, pAp1, (LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(32, Ipp16s, tmp_not_used,LP_SUBFRAME_DIM,encoderObj);

    LOCAL_ARRAY(Ipp16s,indexFC,2,encoderObj);
    LOCAL_ARRAY(Ipp16s,delay,2,encoderObj);
    Ipp8s *synFltw = encoderObj->synFltw;
    Ipp16s *pAna = encoderObj->encPrm,*anau;
    Ipp16s *speechHistory = encoderObj->speechHistory;
    Ipp16s *prevSubfrLSP = encoderObj->prevSubfrLSP;
    Ipp16s *prevSubfrLSPquant = encoderObj->prevSubfrLSPquant;
    Ipp16s betaPreFilter = encoderObj->betaPreFilter;
    Ipp16s *resFilMem0  = encoderObj->resFilMem0;
    Ipp16s *prevWgtSpeech = encoderObj->prevWgtSpeech;
    Ipp16s *wsp = prevWgtSpeech + MAX_PITCH_LAG;
    Ipp16s *prevExcitat = encoderObj->prevExcitat;
    Ipp16s *exc = prevExcitat + L_prevExcitat;
    Ipp32s subfrIdx, valOpenDelay, i;
    Ipp16s bestOPLag, index, gainAc, gainCode, gainPit, tmp, improveFlag,*ppAp, *ppAq;
    Ipp16s vadEnable = (Ipp16s)((encoderObj->objPrm.mode == G729Encode_VAD_Enabled));

    VADmemory_Obj *vM = (VADmemory_Obj*)encoderObj->vadMem;
    if(NULL==encoderObj || NULL==src || NULL ==dst)
        return APIG729_StsBadArgErr;
    if(encoderObj->preProc == NULL)
        return APIG729_StsNotInitialized;
    if(encoderObj->objPrm.objSize <= 0)
        return APIG729_StsNotInitialized;
    if(ENC_KEY != encoderObj->objPrm.key)
        return APIG729_StsBadCodecType;

    encoderObj->codecType = encoderObj->objPrm.codecType;

    ippsCopy_16s(src,encoderObj->speechHistory + SPEECH_BUF_DIM - LP_FRAME_DIM,LP_FRAME_DIM);
    ippsHighPassFilter_G729_16s_ISfs(
                                    encoderObj->speechHistory + SPEECH_BUF_DIM - LP_FRAME_DIM,LP_FRAME_DIM,12,encoderObj->preProc);

    {
        LOCAL_ALIGN_ARRAY(32, Ipp16s, rCoeff, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp16s, newLSP, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp16s, newQlsp, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp16s, rhNBE, LPF_DIM+1,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp32s, autoR, VAD_LPC_DIM +2,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp16s, yVal, LP_WINDOW_DIM,encoderObj);
        Ipp16s Vad=1, s,norma,j;
        norma=1;
        ippsMul_NR_16s_Sfs(LPC_WINDOW,hammingWin,yVal,LP_WINDOW_DIM,15);
        j = LPF_DIM;
        if(vadEnable == 1)
            j = VAD_LPC_DIM ;
        while(ippsAutoCorr_NormE_16s32s(yVal,LP_WINDOW_DIM,autoR,j+1,&i)!=ippStsNoErr) {
            ippsRShiftC_16s_I(2,yVal,LP_WINDOW_DIM);
            norma+=4;
        }
        norma = (Ipp16s)(norma - i);

        if(vadEnable == 1) {
            for(i=0; i<LPF_DIM+1; i++)
                rhNBE[i] = (Ipp16s)(autoR[i] >> 16);
        }
        ippsLagWindow_G729_32s_I(autoR+1,VAD_LPC_DIM );
        if(ippStsOverflow == ippsLevinsonDurbin_G729B(autoR, pAp_t,rCoeff,&s)) {
            ippsCopy_16s(encoderObj->prevSubfrLPC,pAp_t,LPF_DIM+1);
            rCoeff[0] = encoderObj->prevRC[0];
            rCoeff[1] = encoderObj->prevRC[1];
        } else {
            ippsCopy_16s(pAp_t,encoderObj->prevSubfrLPC,LPF_DIM+1);
            encoderObj->prevRC[0] = rCoeff[0];
            encoderObj->prevRC[1] = rCoeff[1];
        }
        ippsLPCToLSP_G729A_16s(pAp_t, prevSubfrLSP, newLSP);
        if(vadEnable == 1) {
            ippsLSPToLSF_Norm_G729_16s(newLSP, tmpvec);
            {
                LOCAL_ALIGN_ARRAY(32, Ipp16s, tmpVecVAD_LPC_DIM4, VAD_LPC_DIM +1, encoderObj);
                VoiceActivityDetect_G729(LPC_WINDOW,tmpvec,autoR,norma,rCoeff[1],&Vad,encoderObj->vadMem, tmpVecVAD_LPC_DIM4);
                LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, tmpVecVAD_LPC_DIM4, VAD_LPC_DIM +1, encoderObj);
            }
            CNG_Update(rhNBE,norma,Vad,encoderObj);

            if(Vad == 0) {
                CNG_encoder(prevExcitat, prevSubfrLSPquant, pAq_t, pAna, encoderObj);
                vad_update_A(pAq_t,pAp_t,exc,PRESENT_SPEECH,val1,wsp,resFilMem0,synFltw);

                vM->VADPPrev = vM->VADPrev;
                vM->VADPrev = Vad;
                encoderObj->betaPreFilter = PITCH_SHARP_MIN;

                ippsMove_16s(&speechHistory[LP_FRAME_DIM], &speechHistory[0], SPEECH_BUF_DIM-LP_FRAME_DIM);
                ippsMove_16s(&prevWgtSpeech[LP_FRAME_DIM], &prevWgtSpeech[0], MAX_PITCH_LAG);
                ippsMove_16s(&prevExcitat[LP_FRAME_DIM], &prevExcitat[0], L_prevExcitat);

                anau = encoderObj->encPrm+1;
                if(pAna[0] == 0) {
                    *frametype=0;
                } else {
                    *frametype=1;
                    dst[0] = (Ipp8u)(((anau[0] & 1) << 7) | ((anau[1] & 31) << 2) | ((anau[2] & 15)>>2));
                    dst[1] = (Ipp8u)(((anau[2] & 3) << 6) | ((anau[3] & 31) << 1));
                }
                CLEAR_SCRATCH_MEMORY(encoderObj);
                return APIG729_StsNoErr;
            }
        }
        *pAna++ = 1;

        encoderObj->seed = SEED_INIT;
        encoderObj->CNGidx = 0;
        vM->VADPPrev = vM->VADPrev;
        vM->VADPrev = Vad;

        ippsLSPQuant_G729_16s(newLSP,(Ipp16s*)encoderObj->prevLSPfreq,newQlsp,pAna);
        pAna += 2;

        ippsInterpolate_G729_16s(newQlsp,prevSubfrLSPquant,tmpvec,LPF_DIM);
        ippsLSPToLPC_G729_16s(tmpvec,&pAq_t[0]);
        ippsLSPToLPC_G729_16s(newQlsp,&pAq_t[LPF_DIM+1]);

        ippsMul_NR_16s_Sfs(gammaFac1,&pAq_t[0],&pAp_t[0],2*(LPF_DIM+1),15);

        ippsCopy_16s(newLSP,prevSubfrLSP,LPF_DIM);
        ippsCopy_16s(newQlsp,prevSubfrLSPquant,LPF_DIM);

        LOCAL_ALIGN_ARRAY_FREE(32,Ipp16s, yVal, LP_WINDOW_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp32s, autoR, VAD_LPC_DIM +2,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, rhNBE, LPF_DIM+1,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, newQlsp, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, newLSP, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, rCoeff, LPF_DIM,encoderObj);
    }

    ippsResidualFilter_G729_16s(&PRESENT_SPEECH[0],&pAq_t[0], &exc[0]);
    ippsResidualFilter_G729_16s(&PRESENT_SPEECH[LP_SUBFRAME_DIM], &pAq_t[LPF_DIM+1], &exc[LP_SUBFRAME_DIM]);

    tmp = 0;
    ippsPreemphasize_G729A_16s(BWF2,pAp_t,pAp1,LPF_DIM+1,&tmp);
    tmp = 0;
    ippsPreemphasize_G729A_16s(BWF2,pAp_t+LPF_DIM+1,pAp1+LPF_DIM+1,LPF_DIM+1,&tmp);

    {
        SynthesisFilterState *SynFltSt=(SynthesisFilterState*)synFltw;
        ippsSynthesisFilter_NR_16s_Sfs(pAp1, &exc[0], &wsp[0], LP_SUBFRAME_DIM, 12, SynFltSt->buffer);
        ippsCopy_16s((&wsp[0]+LP_SUBFRAME_DIM-LPF_DIM), SynFltSt->buffer, LPF_DIM);

        ippsSynthesisFilter_NR_16s_Sfs(pAp1+LPF_DIM+1, &exc[LP_SUBFRAME_DIM], &wsp[LP_SUBFRAME_DIM], LP_SUBFRAME_DIM, 12, SynFltSt->buffer);
        ippsCopy_16s((&wsp[LP_SUBFRAME_DIM]+LP_SUBFRAME_DIM-LPF_DIM), SynFltSt->buffer, LPF_DIM);
    }

    ippsOpenLoopPitchSearch_G729A_16s(wsp, &bestOPLag);
    ppAp = pAp_t;
    ppAq = pAq_t;

    valOpenDelay=bestOPLag;

    for(subfrIdx = 0;  subfrIdx < LP_FRAME_DIM; subfrIdx += LP_SUBFRAME_DIM) {
        ippsSynthesisFilterZeroStateResponse_NR_16s(ppAp, impResp, LP_SUBFRAME_DIM, 12);
        ippsSynthesisFilter_NR_16s_Sfs(ppAp, &exc[subfrIdx], val1,LP_SUBFRAME_DIM,12,resFilMem0);
        ippsAdaptiveCodebookSearch_G729A_16s((Ipp16s)valOpenDelay, val1,impResp,&prevExcitat[subfrIdx],delay,tmp_not_used,(Ipp16s)(subfrIdx!=0));

        if(subfrIdx == 0) {
            if(delay[0] <= 85) {
                index = (Ipp16s)(3 * (delay[0] - 19) + delay[1] - 1);
            } else {
                index = (Ipp16s)(delay[0] - 85 + 197);
            }
        } else {
            i = valOpenDelay - 5;
            if(i < MIN_PITCH_LAG) {
                i = MIN_PITCH_LAG;
            }
            if(i + 9 > MAX_PITCH_LAG) {
                i = MAX_PITCH_LAG - 9;
            }
            index = (Ipp16s)((delay[0] - i)*3 + 2 + delay[1]);
        }
        valOpenDelay=delay[0];

        *pAna++ = index;
        if( subfrIdx == 0 ) {
            *pAna++ = (Ipp16s)equality(index);
        }
        ippsAdaptiveCodebookGain_G729A_16s(val1,ppAp,&exc[subfrIdx],y1,&gainAc);
        if(delay[1] > 0) {
            i = delay[0] + 1;
        } else {
            i = delay[0];
        }
        improveFlag = calcErr_G729(i, encoderObj->coderErr);
        if(improveFlag && gainAc > PITCH_GAIN_MAX) gainAc = PITCH_GAIN_MAX;
        ippsAdaptiveCodebookContribution_G729_16s(gainAc,y1,val1,tmpVec2);

        betaPreFilter = (Ipp16s)(betaPreFilter << 1);
        if(valOpenDelay < LP_SUBFRAME_DIM)
            ippsHarmonicFilter_16s_I(betaPreFilter,valOpenDelay,&impResp[valOpenDelay],LP_SUBFRAME_DIM-valOpenDelay);
        {
            LOCAL_ALIGN_ARRAY(32, Ipp32s, rr, CORR_DIM, encoderObj);
            LOCAL_ALIGN_ARRAY(32, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
            ippsCrossCorr_NormM_16s(impResp,tmpVec2,LP_SUBFRAME_DIM,Dn);

            ippsToeplizMatrix_G729_16s32s(impResp, rr);
            ippsFixedCodebookSearch_G729A_32s16s(Dn, rr, codeword, indexFC);
            CodewordImpConv_G729(indexFC[1],codeword,impResp,y2);


            *pAna++ = indexFC[1];
            *pAna++ = indexFC[0];

            LOCAL_ALIGN_ARRAY_FREE(32, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
            LOCAL_ALIGN_ARRAY_FREE(32, Ipp32s, rr, CORR_DIM, encoderObj);
        }
        if(valOpenDelay < LP_SUBFRAME_DIM)
            ippsHarmonicFilter_16s_I(betaPreFilter,valOpenDelay,&codeword[valOpenDelay],LP_SUBFRAME_DIM-valOpenDelay);
        {
            LOCAL_ARRAY(Ipp16s, gains, 2, encoderObj);
            LOCAL_ARRAY(Ipp16s, gInd, 2, encoderObj);
            ippsGainQuant_G729_16s(val1, y1, codeword, y2, encoderObj->prevFrameQuantEn, gains, gInd,improveFlag);
            gainPit  = gains[0];
            gainCode = gains[1];
            *pAna++ = (Ipp16s)((LUT1[gInd[0]]<<CDBK2_BIT_NUM) | LUT2[gInd[1]]);

            LOCAL_ARRAY_FREE(Ipp16s, gInd, 2, encoderObj);
            LOCAL_ARRAY_FREE(Ipp16s, gains, 2, encoderObj);
        }
        updateExcErr_G729(gainPit, valOpenDelay,encoderObj->coderErr);

        betaPreFilter = gainPit;
        if(betaPreFilter > PITCH_SHARP_MAX)  betaPreFilter = PITCH_SHARP_MAX;
        if(betaPreFilter < PITCH_SHARP_MIN)  betaPreFilter = PITCH_SHARP_MIN;

        ippsInterpolateC_NR_G729_16s_Sfs(
                                        &exc[subfrIdx],gainPit,codeword,gainCode,&exc[subfrIdx],LP_SUBFRAME_DIM,14);

        if(gainCode <= IPP_MAX_16S/2 && gainCode >= IPP_MIN_16S/2) {
            ippsInterpolateC_G729_16s_Sfs(
                                         &y1[LP_SUBFRAME_DIM-LPF_DIM],gainPit,&y2[LP_SUBFRAME_DIM-LPF_DIM],(Ipp16s)(2*gainCode),tmpvec,LPF_DIM,14);
        } else {
            for(i = 0; i < LPF_DIM; i++) tmpvec[i] = (Ipp16s)(((gainPit*y1[LP_SUBFRAME_DIM-LPF_DIM+i])>>14)+
                                                     ((gainCode*y2[LP_SUBFRAME_DIM-LPF_DIM+i])>>13));
        }
        ippsSub_16s(tmpvec,&val1[LP_SUBFRAME_DIM-LPF_DIM],resFilMem0,LPF_DIM);
        ppAp += LPF_DIM+1; ppAq += LPF_DIM+1;
    }

    ippsMove_16s(&speechHistory[LP_FRAME_DIM], &speechHistory[0], SPEECH_BUF_DIM-LP_FRAME_DIM);
    ippsMove_16s(&prevWgtSpeech[LP_FRAME_DIM], &prevWgtSpeech[0], MAX_PITCH_LAG);
    ippsMove_16s(&prevExcitat[LP_FRAME_DIM], &prevExcitat[0], L_prevExcitat);

    encoderObj->betaPreFilter = betaPreFilter;

    anau = encoderObj->encPrm+1;
    *frametype=3;
    dst[0] = (Ipp8u)(anau[0] & 255);
    dst[1] = (Ipp8u)((anau[1] & 0x3ff) >> 2);
    dst[2] = (Ipp8u)(((anau[1] & 3) << 6) | ((anau[2]>>2)&0x3f));
    dst[3] = (Ipp8u)(((anau[2] & 3) << 6) | ((anau[3] & 1) << 5) | ((anau[4] & 8191) >> 8));
    dst[4] = (Ipp8u)(anau[4] & 255);
    dst[5] = (Ipp8u)(((anau[5] & 15)<<4) | ((anau[6] & 127) >> 3));
    dst[6] = (Ipp8u)(((anau[6] & 7)<< 5) | (anau[7] & 31));
    dst[7] = (Ipp8u)((anau[8] & 8191) >> 5);
    dst[8] = (Ipp8u)(((anau[8] & 31) << 3) | ((anau[9] & 15) >> 1));
    dst[9] = (Ipp8u)(((anau[9] & 1) << 7) | (anau[10] & 127));

    CLEAR_SCRATCH_MEMORY(encoderObj);
    return APIG729_StsNoErr;
}

APIG729_Status G729BaseEncode
(G729Encoder_Obj* encoderObj,const Ipp16s *src, Ipp8u* dst, Ipp32s *frametype) {
    LOCAL_ALIGN_ARRAY(16, Ipp16s, impResp, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, val1, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, codeword, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, y1, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, y2, LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, tmp_not_used,LP_SUBFRAME_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, tmpvec, LPF_DIM,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, PWGammaFactorMem, LPF_DIM, encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, pAp_t,(LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, pAq_t,(LPF_DIM+1)*2,encoderObj);
    LOCAL_ALIGN_ARRAY(16, Ipp16s, pAp1, (LPF_DIM+1)*2,encoderObj);

    LOCAL_ARRAY(Ipp16s,LAR,4,encoderObj);
    LOCAL_ARRAY(Ipp16s,wfact1,2,encoderObj);
    LOCAL_ARRAY(Ipp16s,wfact2,2,encoderObj);
    Ipp8s *synFltw = encoderObj->synFltw;
    Ipp16s *pAna = encoderObj->encPrm,*anau;
    Ipp16s *speechHistory = encoderObj->speechHistory;
    Ipp16s *prevSubfrLSP = encoderObj->prevSubfrLSP;
    Ipp16s *prevSubfrLSPquant = encoderObj->prevSubfrLSPquant;
    Ipp16s betaPreFilter = encoderObj->betaPreFilter;
    Ipp16s *resFilMem0  = encoderObj->resFilMem0;
    Ipp16s *prevWgtSpeech = encoderObj->prevWgtSpeech;
    Ipp16s *wsp = prevWgtSpeech + MAX_PITCH_LAG;
    Ipp16s *prevExcitat = encoderObj->prevExcitat;
    Ipp16s *exc = prevExcitat + L_prevExcitat;
    Ipp32s subfrIdx, valOpenDelay, i;
    Ipp16s bestOPLag, index, gainAc, gainCode, gainPit, tmp,improveFlag;
    Ipp8s *synFltw0 = encoderObj->synFltw0;
    Ipp16s *resFilMem  = encoderObj->resFilMem;
    Ipp16s *error  = resFilMem + BWLPCF_DIM;
    Ipp16s subfr, *ppAp, *ppAq;
    Ipp16s *pAp2,*interpLSF,*newLSF;

    if(NULL==encoderObj || NULL==src || NULL ==dst)
        return APIG729_StsBadArgErr;
    if(encoderObj->preProc == NULL)
        return APIG729_StsNotInitialized;
    if(encoderObj->objPrm.objSize <= 0)
        return APIG729_StsNotInitialized;
    if(ENC_KEY != encoderObj->objPrm.key)
        return APIG729_StsBadCodecType;

    interpLSF = &impResp[0];
    newLSF = &impResp[LPF_DIM];
    pAp2 = &pAp1[LPF_DIM+1];

    ippsCopy_16s(src,encoderObj->speechHistory + SPEECH_BUF_DIM - LP_FRAME_DIM,LP_FRAME_DIM);
    ippsHighPassFilter_G729_16s_ISfs(
                                    encoderObj->speechHistory + SPEECH_BUF_DIM - LP_FRAME_DIM,LP_FRAME_DIM,12,encoderObj->preProc);

    {
        LOCAL_ALIGN_ARRAY(16, Ipp16s, rCoeff, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY(16, Ipp16s, newLSP, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY(16, Ipp16s, newQlsp, LPF_DIM+1,encoderObj);
        LOCAL_ALIGN_ARRAY(16, Ipp32s, autoR, VAD_LPC_DIM +2,encoderObj);
        LOCAL_ALIGN_ARRAY(32, Ipp16s, yVal, LP_WINDOW_DIM,encoderObj);
        Ipp16s s,norma;

        norma=1;
        ippsMul_NR_16s_Sfs(LPC_WINDOW,hammingWin,yVal,LP_WINDOW_DIM,15);
        while(ippsAutoCorr_NormE_16s32s(yVal,LP_WINDOW_DIM,autoR,LPF_DIM+1,&i)!=ippStsNoErr) {
            ippsRShiftC_16s_I(2,yVal,LP_WINDOW_DIM);
            norma+=4;
        }
        norma = (Ipp16s)(norma - i);

        ippsLagWindow_G729_32s_I(autoR+1,LPF_DIM);
        ippsLevinsonDurbin_G729B(autoR, &pAp_t[LPF_DIM+1],rCoeff,&s);
        ippsLPCToLSP_G729_16s(&pAp_t[LPF_DIM+1], prevSubfrLSP, newLSP);
        ippsLSPQuant_G729_16s(newLSP,(Ipp16s*)encoderObj->prevLSPfreq,newQlsp,pAna);
        pAna += 2;
        ippsInterpolate_G729_16s(newLSP,prevSubfrLSP,tmpvec,LPF_DIM);
        ippsLSPToLPC_G729_16s(tmpvec,pAp_t);
        ippsLSPToLSF_Norm_G729_16s(tmpvec, interpLSF);
        ippsLSPToLSF_Norm_G729_16s(newLSP, newLSF);

        ippsInterpolate_G729_16s(newQlsp,prevSubfrLSPquant,tmpvec,LPF_DIM);
        ippsLSPToLPC_G729_16s(tmpvec,&pAq_t[0]);
        ippsLSPToLPC_G729_16s(newQlsp,&pAq_t[LPF_DIM+1]);


        ippsCopy_16s(newLSP,prevSubfrLSP,LPF_DIM);
        ippsCopy_16s(newQlsp,prevSubfrLSPquant,LPF_DIM);
        _ippsRCToLAR_G729_16s(rCoeff,LAR+2,2);

        tmp = (Ipp16s)(encoderObj->prevLAR[0] + LAR[2]);
        LAR[0] = (Ipp16s)((tmp<0)? ~(( ~tmp) >> 1 ) : tmp>>1);
        tmp = (Ipp16s)(encoderObj->prevLAR[1] + LAR[3]);
        LAR[1] = (Ipp16s)((tmp<0)? ~(( ~tmp) >> 1 ) : tmp>>1);
        encoderObj->prevLAR[0] = LAR[2];
        encoderObj->prevLAR[1] = LAR[3];
        _ippsPWGammaFactor_G729_16s(LAR,interpLSF,&encoderObj->prevSubfrSmooth,wfact1,wfact2, PWGammaFactorMem);
        _ippsPWGammaFactor_G729_16s(LAR+2,newLSF,&encoderObj->prevSubfrSmooth,wfact1+1,wfact2+1, PWGammaFactorMem);

        ippsMulPowerC_NR_16s_Sfs(&pAp_t[0],wfact1[0], pAp1,LPF_DIM+1,15);
        ippsMulPowerC_NR_16s_Sfs(&pAp_t[0],wfact2[0], pAp2,LPF_DIM+1,15);
        ippsIIR16sLow_G729_16s(pAp1, &PRESENT_SPEECH[-LPF_DIM], wsp, ((SynthesisFilterState*)synFltw)->buffer+BWLPCF_DIM-LPF_DIM);

        ippsMulPowerC_NR_16s_Sfs(&pAp_t[LPF_DIM+1],wfact1[1], pAp1,LPF_DIM+1,15);
        ippsMulPowerC_NR_16s_Sfs(&pAp_t[LPF_DIM+1],wfact2[1], pAp2,LPF_DIM+1,15);
        ippsIIR16sLow_G729_16s(pAp1, &PRESENT_SPEECH[LP_SUBFRAME_DIM-LPF_DIM], &wsp[LP_SUBFRAME_DIM], ((SynthesisFilterState*)synFltw)->buffer+BWLPCF_DIM-LPF_DIM);

        ippsOpenLoopPitchSearch_G729_16s(wsp, &bestOPLag);
        LOCAL_ALIGN_ARRAY_FREE(32,Ipp16s, yVal, LP_WINDOW_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(16, Ipp32s, autoR, VAD_LPC_DIM +2,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(16, Ipp16s, newQlsp, LPF_DIM+1,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(16, Ipp16s, newLSP, LPF_DIM,encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(16, Ipp16s, rCoeff, LPF_DIM,encoderObj);
    }

    ppAp = pAp_t;
    ppAq = pAq_t;

    subfr = 0;
    valOpenDelay=bestOPLag;
    {
        LOCAL_ALIGN_ARRAY(16, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
        LOCAL_ALIGN_ARRAY(16, Ipp32s, rr, CORR_DIM, encoderObj);
        LOCAL_ARRAY(Ipp16s, gains, 2, encoderObj);
        LOCAL_ARRAY(Ipp16s, gInd, 2, encoderObj);
        LOCAL_ARRAY(Ipp16s,indexFC,2,encoderObj);
        LOCAL_ARRAY(Ipp16s,delay,2,encoderObj);
        Ipp16s *tmpVec2 = tmp_not_used;

        for(subfrIdx = 0;  subfrIdx < LP_FRAME_DIM; subfrIdx += LP_SUBFRAME_DIM) {

            ippsMulPowerC_NR_16s_Sfs(ppAp,wfact1[subfr], pAp1,LPF_DIM+1,15);
            ippsMulPowerC_NR_16s_Sfs(ppAp,wfact2[subfr], pAp2,LPF_DIM+1,15);
            subfr++;

            ippsCopy_16s(pAp1,encoderObj->zeroPostFiltVec1,LPF_DIM+1);

            ippsSynthesisFilter_NR_16s_Sfs(ppAq,encoderObj->zeroPostFiltVec1,impResp,LP_SUBFRAME_DIM,12,NULL);
            ippsSynthesisFilterLow_NR_16s_ISfs(pAp2,impResp,LP_SUBFRAME_DIM,12,NULL);

            ippsResidualFilter_G729_16s(&PRESENT_SPEECH[subfrIdx], ppAq, &exc[subfrIdx]);
            ippsSynthesisFilter_NR_16s_Sfs(ppAq,&exc[subfrIdx],error,LP_SUBFRAME_DIM,12,&resFilMem[BWLPCF_DIM-LPF_DIM]);

            ippsIIR16sLow_G729_16s(pAp1, error-LPF_DIM, val1, &resFilMem0[BWLPCF_DIM-LPF_DIM]);

            ippsAdaptiveCodebookSearch_G729_16s((Ipp16s)valOpenDelay, val1,impResp,&prevExcitat[subfrIdx],delay,tmp_not_used,(Ipp16s)(subfrIdx!=0));

            if(subfrIdx == 0) {
                if(delay[0] <= 85) {
                    index = (Ipp16s)(3 * (delay[0] - 19) + delay[1] - 1);
                } else {
                    index = (Ipp16s)(delay[0] - 85 + 197);
                }
            } else {
                i = valOpenDelay - 5;
                if(i < MIN_PITCH_LAG) {
                    i = MIN_PITCH_LAG;
                }
                if(i + 9 > MAX_PITCH_LAG) {
                    i = MAX_PITCH_LAG - 9;
                }
                index = (Ipp16s)((delay[0] - i)*3 + 2 + delay[1]);
            }
            valOpenDelay=delay[0];

            *pAna++ = index;

            if(subfrIdx == 0) {
                *pAna++ = (Ipp16s)equality(index);
            }

            ippsAdaptiveCodebookGain_G729_16s(val1,impResp,&exc[subfrIdx],y1,&gainAc);
            if(delay[1] > 0) {
                i = delay[0] + 1;
            } else {
                i = delay[0];
            }
            improveFlag = calcErr_G729(i, encoderObj->coderErr);
            if(improveFlag && gainAc > PITCH_GAIN_MAX) gainAc = PITCH_GAIN_MAX;
            betaPreFilter = (Ipp16s)(betaPreFilter << 1);
            if(valOpenDelay < LP_SUBFRAME_DIM)
                ippsHarmonicFilter_16s_I(betaPreFilter,valOpenDelay,&impResp[valOpenDelay],LP_SUBFRAME_DIM-valOpenDelay);

            ippsAdaptiveCodebookContribution_G729_16s(gainAc,y1,val1,tmpVec2);

            ippsCrossCorr_NormM_16s(impResp,tmpVec2,LP_SUBFRAME_DIM,Dn);

            ippsToeplizMatrix_G729_16s32s(impResp, rr);
            ippsFixedCodebookSearch_G729_32s16s(Dn, rr, codeword,indexFC,&encoderObj->extraTime, (Ipp16s)subfrIdx);
            CodewordImpConv_G729(indexFC[1],codeword,impResp,y2);

            *pAna++ = indexFC[1];
            *pAna++ = indexFC[0];

            if(valOpenDelay < LP_SUBFRAME_DIM)
                ippsHarmonicFilter_16s_I(betaPreFilter,valOpenDelay,&codeword[valOpenDelay],LP_SUBFRAME_DIM-valOpenDelay);

            ippsGainQuant_G729_16s(val1, y1, codeword, y2, encoderObj->prevFrameQuantEn, gains, gInd,improveFlag);
            gainPit  = gains[0];
            gainCode = gains[1];
            *pAna++ = (Ipp16s)((LUT1[gInd[0]]<<CDBK2_BIT_NUM) | LUT2[gInd[1]]);

            updateExcErr_G729(gainPit, valOpenDelay,encoderObj->coderErr);

            betaPreFilter = gainPit;
            if(betaPreFilter > PITCH_SHARP_MAX)  betaPreFilter = PITCH_SHARP_MAX;
            if(betaPreFilter < PITCH_SHARP_MIN)  betaPreFilter = PITCH_SHARP_MIN;

            ippsInterpolateC_NR_G729_16s_Sfs(
                                            &exc[subfrIdx],gainPit,codeword,gainCode,&exc[subfrIdx],LP_SUBFRAME_DIM,14);

            if(ippsSynthesisFilter_NR_16s_Sfs(ppAq,&exc[subfrIdx],&encoderObj->pSynth[subfrIdx],LP_SUBFRAME_DIM,
                                              12, ((SynthesisFilterState*)synFltw0)->buffer+BWLPCF_DIM-LPF_DIM)!= ippStsOverflow)
                ippsCopy_16s((&encoderObj->pSynth[subfrIdx]+LP_SUBFRAME_DIM-LPF_DIM),
                             ((SynthesisFilterState*)synFltw0)->buffer+BWLPCF_DIM-LPF_DIM, LPF_DIM);
            ippsSub_16s(&encoderObj->pSynth[subfrIdx+LP_SUBFRAME_DIM-LPF_DIM],&PRESENT_SPEECH[subfrIdx+LP_SUBFRAME_DIM-LPF_DIM],encoderObj->resFilMem+BWLPCF_DIM-LPF_DIM,LPF_DIM);

            if(gainCode <= IPP_MAX_16S/2 && gainCode >= IPP_MIN_16S/2) {
                ippsInterpolateC_G729_16s_Sfs(
                                             &y1[LP_SUBFRAME_DIM-LPF_DIM],gainPit,&y2[LP_SUBFRAME_DIM-LPF_DIM],(Ipp16s)(2*gainCode),tmpvec,LPF_DIM,14);
            } else for(i = 0; i < LPF_DIM; i++)
                    tmpvec[i] = (Ipp16s)(((gainPit*y1[LP_SUBFRAME_DIM-LPF_DIM+i])>>14)+((gainCode*y2[LP_SUBFRAME_DIM-LPF_DIM+i])>>13));
            ippsSub_16s(tmpvec,&val1[LP_SUBFRAME_DIM-LPF_DIM],resFilMem0+BWLPCF_DIM-LPF_DIM,LPF_DIM);

            ppAq += LPF_DIM+1;
            ppAp += LPF_DIM+1;
        }
        LOCAL_ARRAY_FREE(Ipp16s,delay,2,encoderObj);
        LOCAL_ARRAY_FREE(Ipp16s,indexFC,2,encoderObj);
        LOCAL_ARRAY_FREE(Ipp16s, gInd, 2, encoderObj);
        LOCAL_ARRAY_FREE(Ipp16s, gains, 2, encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(16, Ipp32s, rr, CORR_DIM, encoderObj);
        LOCAL_ALIGN_ARRAY_FREE(16, Ipp16s, Dn, LP_SUBFRAME_DIM, encoderObj);
    }

    ippsMove_16s(&speechHistory[LP_FRAME_DIM], &speechHistory[0], SPEECH_BUF_DIM-LP_FRAME_DIM);
    ippsMove_16s(&prevWgtSpeech[LP_FRAME_DIM], &prevWgtSpeech[0], MAX_PITCH_LAG);
    ippsMove_16s(&prevExcitat[LP_FRAME_DIM], &prevExcitat[0], L_prevExcitat);

    encoderObj->betaPreFilter = betaPreFilter;

    anau = encoderObj->encPrm;
    *frametype=3;
    dst[0] = (Ipp8u)(anau[0] & 255);
    dst[1] = (Ipp8u)((anau[1] & 0x3ff) >> 2);
    dst[2] = (Ipp8u)(((anau[1] & 3) << 6) | ((anau[2]>>2)&0x3f));
    dst[3] = (Ipp8u)(((anau[2] & 3) << 6) | ((anau[3] & 1) << 5) | ((anau[4] & 8191) >> 8));
    dst[4] = (Ipp8u)(anau[4] & 255);
    dst[5] = (Ipp8u)(((anau[5] & 15)<<4) | ((anau[6] & 127) >> 3));
    dst[6] = (Ipp8u)(((anau[6] & 7)<< 5) | (anau[7] & 31));
    dst[7] = (Ipp8u)((anau[8] & 8191) >> 5);
    dst[8] = (Ipp8u)(((anau[8] & 31) << 3) | ((anau[9] & 15) >> 1));
    dst[9] = (Ipp8u)(((anau[9] & 1) << 7) | (anau[10] & 127));
    CLEAR_SCRATCH_MEMORY(encoderObj);
    return APIG729_StsNoErr;
}
