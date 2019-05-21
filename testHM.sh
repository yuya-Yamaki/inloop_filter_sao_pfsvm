#! /bin/sh
IMG=$1
QP=32
SAO=0
WIDTH=352
HEIGHT=288
LOG="./log/qp$QP-sao$SAO.log"
HMOPT="-c HM-16.9/cfg/encoder_lowdelay_main_rext.cfg --InputChromaFormat=400 -cf 400 -f 30 -fr 30 --InternalBitDepth=8"
HMENC=HM-16.9/bin/TAppEncoderStatic
HMDEC=HM-16.9/bin/TAppDecoderStatic
DEC_IMG="./dec_dir/test.pgm"
$HMENC $HMOPT -q $QP -wdt $WIDTH -hgt $HEIGHT --SAO=$SAO -i $IMG | tee $LOG
$HMDEC -d 8 -b str.bin -o rec8bit.y | tee -a $LOG
rawtopgm $WIDTH $HEIGHT rec8bit.y > $DEC_IMG
