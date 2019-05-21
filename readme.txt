HEVC test mod space
SAOが参照している画像を抜き出してポストフィルタをかけるために関数を追加したもの
encoderには組み込めたものの，decでは画質改善処理を加えられていない

./testHM.sh input.y 
./testHM_Sao-ON.y

modelフォルダに予め学習させたmodelファイルを用意し
TComPicYuv.cppのmodelファイル名を変更する必要がある．（自動化したい...）

default
cfg : encoder_lowdelay_main_rext.cfg
ga : 0.125(TComPicYuv.cpp内のsig_gain変数）

saoの後にpfsvmを導入している
