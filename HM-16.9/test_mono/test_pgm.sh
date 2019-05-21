#! /bin/sh
IMG=$1
if [ ! -r "$IMG" ]; then
        echo Usage: ./test_pgm.sh hoge.pgm
        exit
fi
QP=32
SAO=1
HMOPT="-c ../cfg/encoder_intra_main_rext.cfg -cf 400 -f 1 -fr 1 --InternalBitDepth=8"
HMENC=../bin/TAppEncoderStatic
HMDEC=../bin/TAppDecoderStatic
HMDEC=../bin/TAppDecoderAnalyserStatic
WIDTH=`pamfile $IMG | gawk '{print $4}'`
HEIGHT=`pamfile $IMG | gawk '{print $6}'`
tail -n +4 $IMG > input.y
$HMENC $HMOPT -q $QP -wdt $WIDTH -hgt $HEIGHT --SAO=$SAO -i input.y
SIZE=`ls -l str.bin | gawk '{print $5}'`
$HMDEC -d 8 -b str.bin -o rec8bit.y
rawtopgm $WIDTH $HEIGHT rec8bit.y > decoded.pgm
SNR=`pnmpsnr $IMG decoded.pgm 2>&1 | gawk '{print $7}'`
printf "QP:SIZE:SNR %3d%10d%10.2f\n" $QP $SIZE $SNR
