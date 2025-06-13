# 🐸 かえる
**Language:** [English](./README.md)

このリポジトリには MediaTek のブートローダー (LK) 上で任意のコード実行を可能にする ARM v7 のペイロードが含まれています。

## 概要
>[!CAUTION]
>これで何をするのかよくわからない場合は、**デバイスを壊す可能性があります。**
>
>これは初心者向けのプロジェクトではありません。 **[ドキュメントをよく読んでから](https://github.com/R0rt1z2/kaeru/wiki)** ブートローダーの修正に伴う影響を理解してください。

このツールは、ブートローダーイメージを指定するとデバイスにフラッシュ可能なパッチ適用版を出力します。パッチを適用したイメージには、ブートプロセス中に任意のコードを実行可能にするカスタムペイロードが含まれています。

留意すべきこと:
- このペイロードは、**ARM v7** リトルカーネル (LK) を使用するデバイス向けに設計されています。
- オフセットとアドレスはデバイスとブートローダーのバージョンによって異なります。使用しているデバイスとブートローダーのバージョンに合わせて調整する必要がある場合があります。

以下の一覧は `kaeru` の最も一般的な使用例になります:
- **高度なデバッグ**: ペイロードを使用してブートローダーの関数と変数にフックをしてデバイスのブートプロセスをデバッグ可能にします。
- **カスタム fastboot コマンド**: 独自の fastboot コマンドをブートローダーに追加します。
- **ボタンコンボをリマップ**: カスタムブートモードとキーのボタンコンボを設定して様々なモード (リカバリー、fatsboot など) に入ることが可能です。
- **ブートローダーの警告を削除**: ブートローダーがアンロックされたデバイスの起動時に表示される `Unlocked Bootloader～` の警告を削除します。

<small>__... そしてその他多数！__</small>

## Wiki

kaeru の仕組みを理解するのに役立つ複数のガイドとメモを含む詳細な Wiki が用意されています。

ビルド方法や新しいデバイスのサポートの追加方法などについてはこちらをご覧ください (英語):
1. [Table of contents](https://github.com/R0rt1z2/kaeru/wiki)
2. [Introduction](https://github.com/R0rt1z2/kaeru/wiki/Introduction)
3. [Can I use kaeru on my device?](https://github.com/R0rt1z2/kaeru/wiki/Can-I-use-kaeru-on-my-device%3F)
4. [Porting kaeru to a new device](https://github.com/R0rt1z2/kaeru/wiki/Porting-kaeru-to-a-new-device)
5. [Customization and kaeru APIs](https://github.com/R0rt1z2/kaeru/wiki/Customization-and-kaeru-APIs)

## ライセンス

このプロジェクトは **GNU Affero General Public License v3.0 (AGPL-3.0)** に基づいてライセンスされています。

知っておくべき重要なポイント:

* ソフトウェアは自由に使用、変更、配布可能です。
* ソフトウェアを改変して公開と使用をする場合にはソースコードを公開する必要があります。
* 変更したバージョンを再配布する場合は同じライセンス (`AGPL-3.0`) を保持する必要があります。
* ソフトウェアがネットワークサービスを提供するために使用される場合は、変更を非公開にはできません。

詳細は [LICENSE](https://github.com/R0rt1z2/kaeru/tree/master/LICENSE) ファイルを参照してください。

### 著作権と開発者

- **© 2023–2025 [Skidbladnir Labs, S.L.](https://skidbladnir.cat/)**
- 開発者:
    - Roger Ortiz ([`@R0rt1z2`](https://github.com/R0rt1z2)) ([me@r0rt1z2.com](mailto:me@r0rt1z2.com))
    - Mateo De la Hoz ([`@AntiEngineer`](https://github.com/AntiEngineer)) ([me@antiengineer.com](mailto:me@antiengineer.com))

## 謝辞

### Linux
Makefile とビルド関連のスクリプトの一部は [`Linux` カーネル](https://github.com/torvalds/linux) のソースコードから改変されています。

`Linux` は [GNU General Public License v2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html) に基づいてライセンスされています。

> © The Linux Foundation とその貢献者

> <small>_オリジナルのソース: https://github.com/torvalds/linux_</small>

### nanoprintf
このプロジェクトには組み込みシステム向けに設計された `printf`-style 形式の最小実装である [`nanoprintf`](https://github.com/charlesnicholson/nanoprintf) が含まれています。

`nanoprintf` は [Unlicense](http://unlicense.org) と [Zero-Clause BSD (0BSD)](https://opensource.org/licenses/0BSD) のデュアルライセンスです。

> © 2019 Charles Nicholson

> <small>_オリジナルのソース: https://github.com/charlesnicholson/nanoprintf_</small>

## 免責事項

本ソフトウェアは明示、黙示を問わずいかなる種類の保証もなく **「現状のまま」** 提供されます。本ツールを使用することにより、ユーザーは以下に同意したと見なされます:

* ブートローダーや変更したイメージをフラッシュすると **デバイスに永続的な損傷を与える (文鎮化)** のリスクが高くなります。
* このソフトウェアの使用、誤用や使用できない理由のことから生じるあらゆる結果については、ユーザーが単独で責任を負うものとします。
* このプロジェクトの管理者および貢献者は、発生する可能性のある損害、データの消失、デバイスの破損または法的問題については**一切責任を負いません**。
* このプロジェクトは **教育および研究目的のみ**です。**違法または無許可な使用を意図したものではありません。**

リスクと影響を完全に理解した場合でのみ続行してください。
