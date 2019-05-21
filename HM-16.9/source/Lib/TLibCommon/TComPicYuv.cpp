/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2016, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TComPicYuv.cpp
    \brief    picture YUV buffer class
*/

#include <cstdlib>
#include <assert.h>
#include <memory.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#include "TComPicYuv.h"
#include "TLibVideoIO/TVideoIOYuv.h"

//20190423
#include "svm.h"
#include <math.h>

//! \ingroup TLibCommon
//! \{

TComPicYuv::TComPicYuv()
{
  for(UInt i=0; i<MAX_NUM_COMPONENT; i++)
  {
    m_apiPicBuf[i]    = NULL;   // Buffer (including margin)
    m_piPicOrg[i]     = NULL;    // m_apiPicBufY + m_iMarginLuma*getStride() + m_iMarginLuma
  }

  for(UInt i=0; i<MAX_NUM_CHANNEL_TYPE; i++)
  {
    m_ctuOffsetInBuffer[i]=0;
    m_subCuOffsetInBuffer[i]=0;
  }

  m_bIsBorderExtended = false;
}




TComPicYuv::~TComPicYuv()
{
  destroy();
}



Void TComPicYuv::createWithoutCUInfo ( const Int picWidth,                 ///< picture width
                                       const Int picHeight,                ///< picture height
                                       const ChromaFormat chromaFormatIDC, ///< chroma format
                                       const Bool bUseMargin,              ///< if true, then a margin of uiMaxCUWidth+16 and uiMaxCUHeight+16 is created around the image.
                                       const UInt maxCUWidth,              ///< used for margin only
                                       const UInt maxCUHeight)             ///< used for margin only

{
  destroy();

  m_picWidth          = picWidth;
  m_picHeight         = picHeight;
  m_chromaFormatIDC   = chromaFormatIDC;
  m_marginX          = (bUseMargin?maxCUWidth:0) + 16;   // for 16-byte alignment
  m_marginY          = (bUseMargin?maxCUHeight:0) + 16;  // margin for 8-tap filter and infinite padding
  m_bIsBorderExtended = false;

  // assign the picture arrays and set up the ptr to the top left of the original picture
  for(UInt comp=0; comp<getNumberValidComponents(); comp++)
  {
    const ComponentID ch=ComponentID(comp);
    m_apiPicBuf[comp] = (Pel*)xMalloc( Pel, getStride(ch) * getTotalHeight(ch));
    m_piPicOrg[comp]  = m_apiPicBuf[comp] + (m_marginY >> getComponentScaleY(ch)) * getStride(ch) + (m_marginX >> getComponentScaleX(ch));
  }
  // initialize pointers for unused components to NULL
  for(UInt comp=getNumberValidComponents();comp<MAX_NUM_COMPONENT; comp++)
  {
    m_apiPicBuf[comp] = NULL;
    m_piPicOrg[comp]  = NULL;
  }

  for(Int chan=0; chan<MAX_NUM_CHANNEL_TYPE; chan++)
  {
    m_ctuOffsetInBuffer[chan]   = NULL;
    m_subCuOffsetInBuffer[chan] = NULL;
  }
}



Void TComPicYuv::create ( const Int picWidth,                 ///< picture width
                          const Int picHeight,                ///< picture height
                          const ChromaFormat chromaFormatIDC, ///< chroma format
                          const UInt maxCUWidth,              ///< used for generating offsets to CUs.
                          const UInt maxCUHeight,             ///< used for generating offsets to CUs.
                          const UInt maxCUDepth,              ///< used for generating offsets to CUs.
                          const Bool bUseMargin)              ///< if true, then a margin of uiMaxCUWidth+16 and uiMaxCUHeight+16 is created around the image.

{
  createWithoutCUInfo(picWidth, picHeight, chromaFormatIDC, bUseMargin, maxCUWidth, maxCUHeight);


  const Int numCuInWidth  = m_picWidth  / maxCUWidth  + (m_picWidth  % maxCUWidth  != 0);
  const Int numCuInHeight = m_picHeight / maxCUHeight + (m_picHeight % maxCUHeight != 0);
  for(Int chan=0; chan<MAX_NUM_CHANNEL_TYPE; chan++)
  {
    const ChannelType ch= ChannelType(chan);
    const Int ctuHeight = maxCUHeight>>getChannelTypeScaleY(ch);
    const Int ctuWidth  = maxCUWidth>>getChannelTypeScaleX(ch);
    const Int stride    = getStride(ch);

    m_ctuOffsetInBuffer[chan] = new Int[numCuInWidth * numCuInHeight];

    for (Int cuRow = 0; cuRow < numCuInHeight; cuRow++)
    {
      for (Int cuCol = 0; cuCol < numCuInWidth; cuCol++)
      {
        m_ctuOffsetInBuffer[chan][cuRow * numCuInWidth + cuCol] = stride * cuRow * ctuHeight + cuCol * ctuWidth;
      }
    }

    m_subCuOffsetInBuffer[chan] = new Int[(size_t)1 << (2 * maxCUDepth)];

    const Int numSubBlockPartitions=(1<<maxCUDepth);
    const Int minSubBlockHeight    =(ctuHeight >> maxCUDepth);
    const Int minSubBlockWidth     =(ctuWidth  >> maxCUDepth);

    for (Int buRow = 0; buRow < numSubBlockPartitions; buRow++)
    {
      for (Int buCol = 0; buCol < numSubBlockPartitions; buCol++)
      {
        m_subCuOffsetInBuffer[chan][(buRow << maxCUDepth) + buCol] = stride  * buRow * minSubBlockHeight + buCol * minSubBlockWidth;
      }
    }
  }
}

Void TComPicYuv::destroy()
{
  for(Int comp=0; comp<MAX_NUM_COMPONENT; comp++)
  {
    m_piPicOrg[comp] = NULL;

    if( m_apiPicBuf[comp] )
    {
      xFree( m_apiPicBuf[comp] );
      m_apiPicBuf[comp] = NULL;
    }
  }

  for(UInt chan=0; chan<MAX_NUM_CHANNEL_TYPE; chan++)
  {
    if (m_ctuOffsetInBuffer[chan])
    {
      delete[] m_ctuOffsetInBuffer[chan];
      m_ctuOffsetInBuffer[chan] = NULL;
    }
    if (m_subCuOffsetInBuffer[chan])
    {
      delete[] m_subCuOffsetInBuffer[chan];
      m_subCuOffsetInBuffer[chan] = NULL;
    }
  }
}



Void  TComPicYuv::copyToPic (TComPicYuv*  pcPicYuvDst) const
{
  assert( m_chromaFormatIDC == pcPicYuvDst->getChromaFormat() );

  for(Int comp=0; comp<getNumberValidComponents(); comp++)
  {
    const ComponentID compId=ComponentID(comp);
    const Int width     = getWidth(compId);
    const Int height    = getHeight(compId);
    const Int strideSrc = getStride(compId);
    assert(pcPicYuvDst->getWidth(compId) == width);
    assert(pcPicYuvDst->getHeight(compId) == height);
    if (strideSrc==pcPicYuvDst->getStride(compId))
    {
      ::memcpy ( pcPicYuvDst->getBuf(compId), getBuf(compId), sizeof(Pel)*strideSrc*getTotalHeight(compId));
    }
    else
    {
      const Pel *pSrc       = getAddr(compId);
            Pel *pDest      = pcPicYuvDst->getAddr(compId);
      const UInt strideDest = pcPicYuvDst->getStride(compId);

      for(Int y=0; y<height; y++, pSrc+=strideSrc, pDest+=strideDest)
      {
        ::memcpy(pDest, pSrc, width*sizeof(Pel));
      }
    }
  }
}


Void TComPicYuv::extendPicBorder ()
{
  if ( m_bIsBorderExtended )
  {
    return;
  }

  for(Int comp=0; comp<getNumberValidComponents(); comp++)
  {
    const ComponentID compId=ComponentID(comp);
    Pel *piTxt=getAddr(compId); // piTxt = point to (0,0) of image within bigger picture.
    //cout << "Pel *piTxt is " << getAddr(compId) << "\n";//.yamaki
    //cout << "Pel is " << *piTxt <<"\n";
    const Int stride=getStride(compId);
    const Int width=getWidth(compId);
    const Int height=getHeight(compId);
    const Int marginX=getMarginX(compId);
    const Int marginY=getMarginY(compId);

    Pel*  pi = piTxt;
    // do left and right margins
    for (Int y = 0; y < height; y++)
    {
      for (Int x = 0; x < marginX; x++ )
      {
        pi[ -marginX + x ] = pi[0];
        pi[    width + x ] = pi[width-1];
      }
      pi += stride;
    }

    // pi is now the (0,height) (bottom left of image within bigger picture
    pi -= (stride + marginX);
    // pi is now the (-marginX, height-1)
    for (Int y = 0; y < marginY; y++ )
    {
      ::memcpy( pi + (y+1)*stride, pi, sizeof(Pel)*(width + (marginX<<1)) );
    }

    // pi is still (-marginX, height-1)
    pi -= ((height-1) * stride);
    // pi is now (-marginX, 0)
    for (Int y = 0; y < marginY; y++ )
    {
      ::memcpy( pi - (y+1)*stride, pi, sizeof(Pel)*(width + (marginX<<1)) );
    }
  }

  m_bIsBorderExtended = true;
}



// NOTE: This function is never called, but may be useful for developers.
Void TComPicYuv::dump (const std::string &fileName, const BitDepths &bitDepths, const Bool bAppend, const Bool bForceTo8Bit) const
{
  FILE *pFile = fopen (fileName.c_str(), bAppend?"ab":"wb");

  Bool is16bit=false;
  for(Int comp = 0; comp < getNumberValidComponents() && !bForceTo8Bit; comp++)
  {
    if (bitDepths.recon[toChannelType(ComponentID(comp))]>8)
    {
      is16bit=true;
    }
  }

  for(Int comp = 0; comp < getNumberValidComponents(); comp++)
  {
    const ComponentID  compId = ComponentID(comp);
    const Pel         *pi     = getAddr(compId);
    const Int          stride = getStride(compId);
    const Int          height = getHeight(compId);
    const Int          width  = getWidth(compId);

    if (is16bit)
    {
      for (Int y = 0; y < height; y++ )
      {
        for (Int x = 0; x < width; x++ )
        {
          UChar uc = (UChar)((pi[x]>>0) & 0xff);
          fwrite( &uc, sizeof(UChar), 1, pFile );
          uc = (UChar)((pi[x]>>8) & 0xff);
          fwrite( &uc, sizeof(UChar), 1, pFile );
        }
        pi += stride;
      }
    }
    else
    {
      const Int shift  = bitDepths.recon[toChannelType(compId)] - 8;
      const Int offset = (shift>0)?(1<<(shift-1)):0;
      for (Int y = 0; y < height; y++ )
      {
        for (Int x = 0; x < width; x++ )
        {
          UChar uc = (UChar)Clip3<Pel>(0, 255, (pi[x]+offset)>>shift);
          fwrite( &uc, sizeof(UChar), 1, pFile );
        }
        pi += stride;
      }
    }
  }

  fclose(pFile);
}

//原画出力function.yamaki
  Void TComPicYuv::pelOrgOut(TComPicYuv * pelcPicYuvTrueOrg, int poc) const
  {
    cout << "pelOrgOut.func start\n";
    const Int width = getWidth(COMPONENT_Y);
    const Int height = getHeight(COMPONENT_Y);
    const Int stride = getStride(COMPONENT_Y);
    const Pel* picval = pelcPicYuvTrueOrg->getAddr(COMPONENT_Y);
    FILE* fp;
    Int** org;
    char filename[100];

    sprintf(filename, "HEVCTestOutputImg/output%d.pgm", poc);

    fp = fopen(filename, "wb");

    org = (Int**)malloc(sizeof(Int *) * height);
    for( Int y = 0; y < height; y++ ){
      org[y] = (Int*)malloc(sizeof(Int) * width);
    }

    cout << "width is " << width << "\n";
    cout << "height is " << height << "\n";
    cout << "stride is " << stride << "\n";

    for( Int y = 0; y < height; y++ ){
      for( Int x = 0; x < width; x++ ){
        org[y][x] = (Int)picval[x + y * stride];
      }
    }

    fprintf(fp, "P5\n%d %d\n%d\n", width, height, 255);
    for( Int y = 0; y < height; y++){
      for( Int x = 0; x < width; x++){
        putc(org[y][x], fp);
      }
    }
    fclose(fp);

    //メモリ開放
    for(Int y = 0; y < height; y++){
      free(org[y]);
    }
    free(org);
  }

  //2/18.yamaki
  TComPicYuv* TComPicYuv::ModPicYuvRec(TComPicYuv* pelcPicYuvRec, TComPicYuv* pelcPicYuvOrg, int poc)
  {
      std::cout << "ModPicYuvRec.func start\n";
      const Int width = getWidth(COMPONENT_Y);
      const Int height = getHeight(COMPONENT_Y);
      const Int stride = getStride(COMPONENT_Y);
      Pel* picOrgval = pelcPicYuvOrg->getAddr(COMPONENT_Y);
      Pel* picRecval = pelcPicYuvRec->getAddr(COMPONENT_Y);
      Int** cls;
      struct svm_model *svm_model;
      struct svm_node *svm_x;
      int num_class, side_info;
      double th_list[MAX_CLASS / 2], fvector[NUM_FEATURES], sig_gain = 0.125;
      double offset[MAX_CLASS];
      int cls_hist[MAX_CLASS];
      double sn_before, sn_after;
      int success, k, i, j, n, label, y;

      cls = (Int**)malloc(sizeof(Int *) * height);
      for (j = 0; j < height; j++) {
          cls[j] = (Int*)malloc(sizeof(Int) * width);
      }

      std::cout << "width is " << width << "\n";
      std::cout << "height is " << height << "\n";
      std::cout << "stride is " << stride << "\n";

      svm_model = svm_load_model("model/akiyo-q32-sao1-c2.0-gm0.25-ga0.125-l3.svm");
      num_class = svm_model->nr_class;
      
      TComPicYuv::set_thresholds(picOrgval, picRecval, 1, num_class, th_list, height, width, stride);

      sn_before = TComPicYuv::calc_snr(picOrgval, picRecval, height, width, stride);

      printf("PSNR = %.4f (dB)\n", sn_before);
      printf("# of classes = %d\n", num_class);
      printf("Thresholds = {%.1f", th_list[0]);

      for (k = 1; k < 3 / 2; k++) {
          printf(", %.1f", th_list[k]);
      }
      printf("}\n");
      printf("Gain factor = %f\n", sig_gain);
      svm_x = Malloc(struct svm_node, NUM_FEATURES + 1);
      success = 0;
      for (k = 0; k < num_class; k++) {
          offset[k] = 0.0;
          cls_hist[k] = 0;
      }

      //get feature vectors that is input of SVM
      for (i = 0; i < height; i++) {
          for (j = 0; j < width; j++) {
              TComPicYuv::get_fvector(picRecval, i, j, sig_gain, fvector, height, width, stride);
              n = 0;
              for (k = 0; k < NUM_FEATURES; k++) {
                  if (fvector[k] != 0.0) {
                      svm_x[n].index = k + 1;
                      svm_x[n].value = fvector[k];
                      n++;
                  }
              }
              //各画素を三つのクラスに分類したので、各画素にラベルを割り当てる
              svm_x[n].index = -1;
              label = (int)svm_predict(svm_model, svm_x);
              if (label == TComPicYuv::get_label(picOrgval, picRecval, i, j, num_class, th_list, stride)) {
                  success++;
              }
              cls[i][j] = label;
              //各画素の再生誤差の総計→後にオフセットを計算
              offset[label] += picOrgval[j + i * stride] - picRecval[j + i * stride];
              cls_hist[label]++;
          }
          fprintf(stderr, ".");
      }//finish for roop

      fprintf(stderr, "\n");
      printf("Accuracy = %.2f (%%)\n", 100.0 * success / (width * height));
      side_info = 0;
      for (k = 0; k < num_class; k++) {
          if (cls_hist[k] > 0) {
              offset[k] /= cls_hist[k];//offset値は各クラスの再生誤差の平均値
          }
          printf("Offset[%d] = %.2f (%d)\n", k, offset[k], cls_hist[k]);
          offset[k] = n = floor(offset[k] + 0.5);
          if (n < 0) n = -n;
          side_info += (n + 1);
          if (n > 0) side_info++;
      }

      //add offset to pels
      for (i = 0; i < height; i++) {
          for (j = 0; j < width; j++) {
              label = cls[i][j];
              k = picRecval[j + i * stride] + offset[label];
              if (k < 0) k = 0;
              if (k > 255) k = 255;
              picRecval[j + i * stride] = k;
          }
      }
      
      //メモリ開放
      for (y = 0; y < height; y++) {
          free(cls[y]);
      }
      free(cls);

      sn_after = TComPicYuv::calc_snr(picOrgval, picRecval, height, width, stride);
      printf("PSNR = %.4f(dB)\n", sn_after);
      printf("GAIN = %+.4f(dB)\n", sn_after - sn_before);
      svm_free_and_destroy_model(&svm_model);
      free(svm_x);
      return pelcPicYuvRec;
  }

  void TComPicYuv::set_thresholds(Pel* org, Pel* rec, int num_img, int num_class, double *th_list, int height, int width, int stride)
  {
      int hist[MAX_DIFF + 1];
      int i, j, k;
      double class_size;

      for (k = 0; k < MAX_DIFF + 1; k++) {
          hist[k] = 0;
      }
      class_size = 0;

      for (i = 0; i < height; i++) {
          for (j = 0; j < width; j++) {
              k = org[j + i * stride] - rec[j + i * stride];
              if (k < 0)k = -k;
              if (k > MAX_DIFF)k = MAX_DIFF;
              hist[k]++;
          }
      }
      
      class_size += width * height;
      for (k = 0; k < MAX_DIFF + 1; k++) {
          printf("%4d%8d\n", k, hist[k]);
      }
      class_size /= num_class;
      i = 0;
      for (k = 0; k < MAX_DIFF; k++) {
          if (hist[k] > class_size * (1 + 2 * i)) {
              th_list[i++] = k + 0.5;
          }
          hist[k + 1] += hist[k];
      }
  }

  double TComPicYuv::calc_snr(Pel* img1, Pel* img2, int height, int width, int stride)
  {
      int i, j, d;
      double mse;

      mse = 0.0;
      for (i = 0; i < height; i++) {
          for (j = 0; j < width; j++) {
              d = img1[j + i * stride] - img2[j + i * stride];
              mse += d * d;
          }
      }
      mse /= (width * height);
      return (10.0 * log10(255 * 255 / mse));
  }

  int TComPicYuv::get_fvector(Pel *img, int i, int j, double gain, double *fvector, int height, int width, int stride)
  {
      typedef struct {
          int y, x;
      } POINT;

      const POINT dyx[] = {
          /* 0 (1) */
          { 0, 0 },
          /* 1 (5) */
          { 0,-1 },{ -1, 0 },{ 0, 1 },{ 1, 0 },
          /* 2 (13) */
          { 0,-2 },{ -1,-1 },{ -2, 0 },{ -1, 1 },{ 0, 2 },{ 1, 1 },{ 2, 0 },{ 1,-1 },
          /* 3 (25) */
          { 0,-3 },{ -1,-2 },{ -2,-1 },{ -3, 0 },{ -2, 1 },{ -1, 2 },{ 0, 3 },{ 1, 2 },
          { 2, 1 },{ 3, 0 },{ 2,-1 },{ 1,-2 }
      };
      int k, x, y, v0, vk, num_nonzero;
      /*注目画素の再生値＝v0*/
      v0 = img[j + i * stride];
      num_nonzero = 0;

      /*for構文の繰り返しは12次元特徴ベクトルとしたので12回で良い
      for (k=o; k<12; k++){}
      #define NUM_FEATURES 12;(pfsvm.h)*/
      for (k = 0; k < NUM_FEATURES; k++) {
          y = i + dyx[k + 1].y;
          if (y < 0) y = 0;
          if (y > height - 1) y = height - 1;
          x = j + dyx[k + 1].x;
          if (x < 0) x = 0;
          if (x > width - 1) x = width - 1;
          /*vk=img->val[y][x](周辺画素の再生値)-img->val[i][j](注目画素の再生値)*/
          vk = img[x + y * stride] - v0;
          if (vk != 0) num_nonzero++;

          /*スケーリング関数（シグモイド関数）用いて-1<特徴ベクトル<+1の範囲にする
          vkが入力（キャスト演算子で小数に変換されている）
          k=0,1,2,...,11の12次元特徴ベクトルが得られる*/
          fvector[k] = 2.0 / (1 + exp(-(double)vk * gain)) - 1.0;
      }
      return (num_nonzero);
  }

  int TComPicYuv::get_label(Pel *org, Pel *dec, int i, int j, int num_class, double *th_list, int stride)
  {
      int d, sgn, k;
      /*d=org->val[i][j](原画像の画素値)-dec->val[i][j](注目画素の再生値)*/
      d = org[j + i * stride] - dec[j + i * stride];
      if (d > 0) {
          sgn = 1;
      }
      else {
          d = -d;
          sgn = -1;
      }
      for (k = 0; k < num_class / 2; k++) {
          if (d < th_list[k]) break;
      }
      return (sgn * k + num_class / 2);/*0,1,2でlabelをつけている*/
  }

//! \}
