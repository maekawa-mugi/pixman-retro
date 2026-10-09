# Pixman PS2 全PR入りビルド v5 — 2026-10-09

GitHubで確認できたPRは #1 の1件。最新HEAD b3e09a849a91acb126daa3316adc4279f2135d72 を使用。
master f227dcbe57e8fcfb5174ffd7eb55a69b6692d512 はこのPRの祖先で、masterの変更も含まれます。
https://github.com/maekawa-mugi/pixman-retro/pull/1

## 実行・項目別A/B表示

pixman_mmi_test.elfをPCSX2のELF起動、またはPS2のELFローダーで起動します。
OpenSSL側test/ps2/main.cのA/B表示を参考に、17テスト項目を固定行に表示します。
各行にはテスト名、A、Bが横に並び、それぞれWAIT→RUNNING→緑O/赤X（ms）へ更新されます。
完了した結果を消さずに同じ行へ残し、累積A/B時間や合計A/BのO/Xは表示しません。
最終行のTEST: OK! failures=0を確認してください。SleepThreadで画面を保持します。

例（Nはその項目の実測値）：

    Solid CA            A: O (N.NNN ms)         B: O (N.NNN ms)
    Masks/solid/CA      A: O (N.NNN ms)         B: O (N.NNN ms)
    Scaled OVER         A: O (N.NNN ms)         B: O (N.NNN ms)

A=EE MMI、B=C実装。各欄を個別に判定し、不一致は該当行・該当側だけ赤Xになります。
直接カーネル8項目は、既存の各試験をMMI版と別名リンクしたCモデル版で各1回実行します。
ランダムseedを毎回同じ初期値へ戻し、A/Bを同一入力・同一検証条件で比較します。
直接試験の時間はOpenSSLの参考コードと同じく試験全体（入力準備・検証を含む）の時間です。
API9項目は同じ入力に対する描画・照合と計時をまとめ、各項目のA/B描画時間だけを表示します。
独立C参照計算・入力準備・画面更新・照合はAPI描画時間に含みません。
A8全組合せのBはスカラー参照式の計算時間です。CのBLT未対応時はmemmove/memcpyへフォールバック。
別の反復ベンチマークは追加しません。追加の54条件も各側1回で検証・計時を行います。
途中の細かい進捗は約100ms間隔で更新し、項目・側の開始/完了・失敗時は即時更新します。
stdoutにはSUITE_AB_RESULT（項目名、A/B、O/X、その項目のms）と詳細を逐次出力します。
直接試験とAPI試験は計時対象が異なるため、異なる項目の時間を相互比較しないでください。

## 生成物とリンク

- `lib/libpixman-1.a`: Pixman本体
- `lib/libpixman-ee-mmi.a`: 全EE MMIバックエンド
- `include/pixman-1/`: 公開ヘッダー
- `pixman_mmi_test.map`: リンクマップ
- `SHA256SUMS.txt`: 成果物のハッシュ

アプリケーションへのリンクでは両アーカイブを `-Wl,--start-group ... -Wl,--end-group` で囲んでください。
このビルドは `-march=r5900 -G0 -D_EE -DPIXMAN_NO_TLS`、n32 ABI、静的PICなし、TLSなし。
単一スレッドのテスト用構成です。SDKランタイム・起動ファイル・リンクスクリプトは同梱クロスファイルを参照してください。

## v5の変更と検証

17行×A/Bの34欄を固定表示し、項目別の結果と時間へ変更しました。
直接試験のB用CモデルをPS2 ELFに別名でリンクし、16カーネルすべてのC版が含まれることを確認。
ホストで34欄すべてO、既存API回帰12,824ケース＋追加54条件もPASS。
A/B別の不一致注入ではLarge / scaled APIの該当側1欄だけX、残り33欄はOになることを確認。
単独host-checkもPASS。PS2クロスビルド成功、未解決シンボルなし。
v5の実機・PCSX2実行は未確認です。
fix-per-suite-ab.patchはv4からの差分、ps2-sdk-build.patchはPR HEADからの全差分です。
OpenSSLの参考ファイルは読んだだけで編集していません。

## v4の変更と検証

共通ダッシュボードprogress.c/progress.hを追加し、直接テスト・API回帰・54条件を同じ進捗画面へ統合。
A/B計時は検証する呼び出しそのものを測定。独立C参照は各入力1回のみ。
ホストで17グループ・12,878条件がPASS、A/Bとも緑O、C経路のMMI呼び出し0。
54条件の108個のA/B出力はすべてiterations=1、結果Oを確認しました。
A/B個別の不一致注入は実行中の更新ログで赤Xになり、最後までX保持・終了コード1を確認。
PS2クロスビルド成功、未解決シンボルなし。v4の実機・PCSX2実行は未確認です。
fix-integrated-timing.patchはv3からの変更、ps2-sdk-build.patchはPR HEADからの全変更です。

## v3の修正

直接テストの8888 OVER定数ベクトルに14要素しか初期値がなく、最後の2個の丸め定数が0でした。
不足した128を補完し、推論配列サイズとコンパイル時チェックで16要素を保証します。
独立マイクロベンチにも同じ欠落があったため修正しました。
ホストモデルが渡された定数を無視していた点も修正し、各レーンの定数を使用します。
修正前にユーザーの実機ログと同じFAIL 8888 OVER quads=1をホストで再現。
修正後の直接テスト129ブロック数、API回帰12,824ケース、A/B全54条件はPASS。
要素を減らした検証用ソースはコンパイル時に拒否されることを確認しました。
描画APIの元の定数は正しく、v3ではライブラリのOVER命令列を変更していません。
v3の実機・PCSX2実行は未確認です。

## v2の修正

報告されたMasks/solid/CA、width=16、offset=4の失敗を修正しました。
solid CAの境界までのバイト数を32bitピクセル数として使っていたため、未整列のLQ/SQが描画範囲外を破壊。
prefixをsizeof(*dst)で割り、常に16バイト整列したポインターをMMIへ渡します。
ホストモデルにも整列assertを追加し、修正前に同グループで失敗することを再現。
修正後はoffset 0/4/8/12・幅1〜127・正負strideを含む回帰がPASSしました。
A/B切替で別チェーンのfast-path cacheを使わないよう、cache所有者のtoplevelも照合します。
Bと独立C参照からのMMI呼び出しは0を確認。A/Bそれぞれの不一致注入で赤X・終了コード1を検証済み。
詳細な差分はfix-ca-and-benchmark.patch（前回ビルドから）とps2-sdk-build.patch（PR HEADから）を参照。

## ビルド環境と調整

MSYS2 `/mingw32/bin`、PS2DEV GCC 15.2.0、binutils 2.45.1、インストール済みPS2SDKを使用。
PS2DEV=/usr/local/ps2dev、PS2SDK=/usr/local/ps2dev/ps2sdk。
Meson 1.12.1は作業用ディレクトリ内に配置しました。
PRのo32向けアセンブリを物理レジスタ名へ変更し、第5引数をn32ではレジスタ8から受け取ります。
o32のスタック引数受け取りも条件分岐で保持しています。
PS2SDKのpthreadリンク、newlibのint32_t型、noinlineマクロ衝突、16bpp fillの重複条件も調整。
変更は `ps2-sdk-build.patch` と `source/` に保存。元の作業ブランチ・未コミット変更は保持しました。

## 検証

- 完全なPS2クロスビルドとリンク成功。
- 最新PRの全直接カーネルテストはホストCモデルでPASS。
- Pixman APIの12,824ケースはホストCモデルでPASS。
- 既存回帰12,824ケース＋54条件の統合検証がPASS、A/Bとも緑O。
- A/B個別の不一致注入は該当側だけ赤X、終了コード1。
- 16カーネルのうち、現在使用する15カーネルに実際の呼び出しを確認。
  旧staged CAカーネルは融合CAへ置き換わったため、呼び出し0を許容。
- ELF32 little endian / R5900 / MIPS III / n32、エントリ0x001010d8、ロード先0x00100000。
- 未解決シンボルなし、EE MMI命令と全16カーネルがリンク済み。
- 実機・PCSX2では未実行。ホストCモデルのPASSはEE命令の動作保証ではありません。

## 再ビルド

この作業ディレクトリではMSYS bashから `bash build-ps2-mmi/all-pr/rebuild.sh`。
ZIP展開先ではPython 3とMeson、NinjaをPATHに配置して `bash rebuild.sh`。
PS2DEV/PS2SDK/CC/ARは環境変数で変更できます。
