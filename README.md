# Computer Graphics Gems JP 2015:5．「スキニング分解」 サンプルコード
本コードは，Computer Graphics Gems JP 2015の5章「スキニング分解」で紹介しているアルゴリズム"Smooth Skinning Decomposition with Rigid Bones"の実装サンプルです．

## ビルド方法
本コード単体ではビルドできません．外部ライブラリとして，[Eigen](http://eigen.tuxfamily.org/ "Eigen")および[QuadProg++](http://quadprog.sourceforge.net/ "QuadProg++")が必要です．

ビルドのための最も簡単な手順は次の通りです．

1. Eigenのインストールフォルダにインクルードパスを通す．
2. QuadProg++をダウンロードし，下記の4つのファイルをssdrフォルダにコピーする．
 * QuadProg++.hh
 * QuadProg++.cc
 * Array.hh
 * Array.cc

なお，ビルドおよび実行テストには Eigen 3.2.4 と QuadProg++ 1.2.1 を用いています．

## 計算パラメータの調整
SSDRの主な計算パラメータは，HorseObject::OnInit内，HorseObject.cpp の339行目あたり，ssdrParam 構造体に指定されています．
* numIndices： 各頂点当たりに割り当てられる最大ボーン数
* numMinBones： 頂点アニメーション近似に用いる最小ボーン数
* numMaxIterations： 最大反復回数
これら3つのパラメータの変更することで，それにともなう計算結果の変化を確認いただけると思います．

## 参考文献

1. Binh Huy Le and Zhigang Deng, Smooth Skinning Decomposition with Rigid Bones, ACM Transactions on Graphics, 31 (6), (2012), 199:1-199:10.
2. Binh Huy Le and Zhigang Deng, Robust and Accurate Skeletal Rigging from Mesh Sequences, ACM Transactions on Graphics, 33 (4), (2014), 84:1-84:10.

## 変更履歴
1. 2015/08/24 初版公開
