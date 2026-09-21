# WSE GEF File Formats

> Canonical source: [English GEF File Formats](../en/GefFileFormats.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的・責務・利用範囲

`WSE::Gef`はCoreに依存する同期・Caller-confinedなFile Controllerです。Worker・Callback・公開OS Handleは
ありません。`BINController`は型付き数値Block、`CSVController`は単純な設定表とKey索引Mapを扱い、
機器固有の設定の意味は解釈しません。

| 公開宣言 | 実装 | 保持状態 |
| --- | --- | --- |
| [BINController](../../../api/gef/bin/binController.h) | [BIN実装](../../../core/gef/bin/binController.cpp) | 型別のBlock index／値配列、次Index、Writer順序表 |
| [CSVController](../../../api/gef/csv/csvController.h) | [CSV実装](../../../core/gef/csv/csvController.cpp) | 所有Stringの行Vector |
| [SettingFormat](../../../api/gef/format/SettingFormat.h) | Header定義 | Type tag、固定列数、`datamap_2d` |
| [GefError](../../../api/gef/error/GefError.h) | [Error実装](../../../core/gef/error/GefError.cpp) | 安定Category／Code、診断情報 |

相互運用範囲は対応するLittle EndianのWindows／Linux x86-64・ARM64です。Big Endian Hostを認証しません。
値をCopyして所有し、Local Streamは操作終了時に閉じます。同一Controllerの同時変更は未対応です。

## BIN配置: GEF-BIN-01

Fileは連続Blockから成り、全体Magic、Version、Checksum、Alignment Paddingはありません。
Block境界のEOFで終了します。Block indexは0始まりのFile内順序であり、Fileへ格納しません。

| 相対Offset | Field | 符号化 |
| --- | --- | --- |
| 0 | Category | 下表の1 byte符号なし値 |
| 1 | 要素数N | 8 byte符号なしLittle Endian |
| 9 | Payload | Category幅のSampleをN個連続 |
| 9 + N×幅 | 次Block | Payload直後 |

| Tag | Category | 幅 | Sample |
| --- | --- | --- | --- |
| 1 | Enum | 1 | 符号なし8bit |
| 2 | Integer08 | 1 | 符号あり8bit |
| 3 | Integer16 | 2 | 符号あり16bit Little Endian |
| 4 | Integer32 | 4 | 符号あり32bit Little Endian |
| 5 | Integer64 | 8 | 符号あり64bit Little Endian |
| 6 | Float32 | 4 | IEEE-754 binary32 Little Endian |
| 7 | Float64 | 8 | IEEE-754 binary64 Little Endian |

符号あり多Byte値は対応Hostの2の補数表現を使います。Tag 0・未知Tagは`Format/MalformedData`、
Count・Payloadの切詰めは`Io/ReadFailed`です。CountはByte数ではなく要素数です。
Integer16の[4660, -2]を持つ1 Block全体は次のとおりです。

```text
03 | 02 00 00 00 00 00 00 00 | 34 12 FE FF
tag          count=2             samples
```

[BIN契約](../../../test/characterization/gef_bin_contract.cpp)はReaderに依存せずWriterのByteを比較し、
手書きFixtureをReaderへ渡します。独立Readerは1 byte、8 byte、N×幅byteを読み、Block境界のEOFまで
繰り返せます。`readWithLimits`は入力Byte数・Block数・総要素数をPayload復号前に制限します。
残量との減算・除算比較によりCount乗算のOverflowも避けます。

## BIN状態と処理順序: GEF-BIN-02

```text
writer: new controller -> setContents(block A) -> setContents(block B) -> write(path)
reader: new controller -> read(path) -> check result -> contents<T>(block index)
append: new writer -> setContents(new blocks) -> write(path, true)
verify: new reader -> read(appended file) -> inspect old and appended block indices
```

`setContents`は型別領域へCopyし、Writer順序表へType／Countを追加します。空Blockを含め1回で1 Indexを
消費します。`write(path,false)`は宛先を切り詰め、`write(path,true)`は既存Blockを書き換えず追記します。
次のReaderがFile全体にIndexを振ります。

`read`と`readWithLimits`はWriter順序表を含めて一時Controllerへ復号し、入力・Closeの成功後に
全状態を交換します。以前の値は置換され、Error戻り・確保例外では値・Index・Writer順序を保持します。
空Fileの成功はControllerを空にします。Read後の追加入力とWriteが可能です。
`setContents`も両領域の容量を先に確保し、確保失敗では論理内容を変更しません。
BINのMove後の移動元は空となり再利用できます。

`contents<T>(index)`は格納値をTへ変換した所有Vectorであり、厳密な型一致照会ではありません。
空結果だけでは存在しないIndexと空Blockを区別できません。`last_index()`は1 Block以上を前提とし、
空なら`std::logic_error`です。

直接の`write`は切詰め・追記を行い、Write／Flush／Close失敗を確認します。
失敗時に宛先が途中まで変更される可能性があります。元Fileを保つ置換には下記GEF-IO-04の
`writeAtomic`を使います。追記はTransactionではありません。

## 設定CSV文法: GEF-CSV-01

Portable入力はBOMなし・LF改行・Comma区切りのByte String表です。非ASCIIはUTF-8を使用しますが、
ControllerはEncodingを検証・変換しません。引用符に特別な意味はなく、引用Comma、複数行Cell、Escapeは
未実装です。空白をTrimしません。Text modeのCRLF処理はOSで異なるため共通FixtureはLFを使います。

先頭行をLabelとし、Data列0をRow Key、列1・2・3を対応Label名の単String Vector、
列4以降を個別Header名によらず`SETTING_PARAM`（`"Param"`）配下の順序付きVectorにします。

```text
Key,Category,Num,Remark,Param0,Param1
camera_a,enum,2,primary,10,20
```

`datamap_2d`は次のEntryになります。

```text
"camera_a" -> {
    "Category": ["enum"],
    "Num":      ["2"],
    "Remark":   ["primary"],
    "Param":   ["10", "20"]
}
```

`Category`・`Num`はStringのままで、数値型の強制やParam数との照合は行いません。
`convertCategory`は独立した大文字小文字を区別する完全一致Lookupで、`enum`、`int08`、`int16`、
`int32`、`int64`、`float32`、`float64`以外はNoneです。MapはStringを所有し、CSV表を借用しません。

## CSV状態と検証: GEF-CSV-02

`read`は入力Headerも含めて行を追記します。`readWithLimits`も上限付きの追記です。
`readReplace`は表全体を置換し、省略時は`ReadLimits{}`の既定上限を使用します。すべて一時状態へ読み、
入力・Close成功後だけ交換するため、Error戻り・確保例外では元の表を保持します。
空の追記は行を変更せず、空の置換は表を空にします。追記はHeaderを結合・除外しません。
新しい設定表へ読み替える場合は`readReplace`を使います。

`contents()`のConst参照はOwnerが存続し変更されない間だけ有効です。`toDataMAP2D`はLabel行と
1行以上のDataが必要で、足りなければ`Format/NoData`です。Label参照前に4 Cell以上を検査し、
各Data行には5 Cell以上を要求します。短いLabel／Data行は`Format/MalformedData`です。
生の読込はこの表構造を要求せず、Map変換時に検査します。

- 連続Delimiterは空Cellを作り、末尾Delimiterは追加の末尾空Cellを作りません。
  空行は空の行になり、改行なしの最終行も保持します。
- 重複Row Keyは後勝ちです。固定Labelの重複も衝突するため、一意にし`Param`を予約します。
- `write`は各Cellの後にComma、各行の後にBinary modeのLFを出します。Windows／Linuxで共通です。
  引用・Escapeはしません。入力はNative Text modeの
  変換を引き続き受けるため、Portable FixtureにはLFを使います。
- 一般の引用CSV、Schema検証、任意Encoding検証は未対応です。

## 入力上限と失敗からの復旧: GEF-IO-04

[ReadLimits](../../../api/gef/format/ReadLimits.h)は上限値を含めて受理します。

<div class="wse-gef-budgets" style="max-width:100%; overflow-x:auto;"><div style="min-width:42rem;">

| Field | 既定値 | 範囲 |
| --- | --- | --- |
| <code style="white-space:nowrap;">max_input_bytes</code> | <span style="white-space:nowrap;">67,108,864</span> | 今回の入力Streamから取り出すByte。Delimiter・改行を含む |
| <code style="white-space:nowrap;">max_blocks</code> | <span style="white-space:nowrap;">1,048,576</span> | 空Blockを含むBIN Block数 |
| <code style="white-space:nowrap;">max_elements</code> | <span style="white-space:nowrap;">4,194,304</span> | 全型合計のBIN要素数 |
| <code style="white-space:nowrap;">max_rows</code> | <span style="white-space:nowrap;">1,048,576</span> | Header・空行を含む結果CSV行数 |
| <code style="white-space:nowrap;">max_cells</code> | <span style="white-space:nowrap;">4,194,304</span> | 結果CSV総Cell数 |
| <code style="white-space:nowrap;">max_cell_bytes</code> | <span style="white-space:nowrap;">1,048,576</span> | CSV各CellのByte数 |

</div></div>

0は該当内容を許可しません。形式に無関係なFieldは無視します。CSV追記では既存行・Cellも合算し、
既存Cell長も検査します。入力Byteは今回のStreamだけです。CSVのText変換後に数えるため、Windowsでは
物理File Sizeの上限ではありません。制限するのは論理Dataで、Heap総量ではありません。Vector／Stringの
管理領域・余剰容量・旧状態・一時Copyが同時に存在します。容量の小さい環境では上限を小さく設定します。
Stream内部の先読みがあり、経過時間・Cancellation期限はありません。同時変更される入力のSnapshotも保証しません。
互換APIの`read(path)`は`ReadLimits::unlimited()`です。外部入力には上限付きAPIを選びます。
上限超過は`Resource/LimitExceeded`で状態を保持します。BINはPayload要素の確保前に宣言Countを残Byte・
要素数と比較します。宣言Payloadが上限を超えた場合、切詰めErrorより先に上限Errorを返しえます。

両Controllerの`writeAtomic(path)`は、呼出側が管理する信頼できるDirectory内の通常のLocal Fileを対象にします。
同じ親Directoryへ排他的に一時Directoryを確保し、その`data`へ書き、Flush・Closeを確認後、同一Filesystemの
Native Rename（Windowsは置換指定の`MoveFileExW`、Linuxはrename）で宛先を置換します。元Fileを先に削除しません。
置換成功前の失敗・例外では元の宛先とControllerを保つため、原因を解消して同じControllerで再試行できます。
宛先新規作成にも対応します。成功・失敗・例外で所有一時Pathを除去しますが、Storageアクセス自体が失敗した場合の
後始末はBest effortです。

これは置換専用で、Atomic追記、Filesystem同期、電源断時の永続性、Network Filesystem保証、悪意あるDirectory
所有者への保護はありません。File実体が変わるため、既存権限・Hard Link・他Metadataは引き継ぎません。
同時Writerを直列化せず、最後に成功した置換が残ります。呼出中はDirectoryを安定させます。
Native処理が同一Filesystemの置換を支える必要があり、未対応・拒否はErrorを返します。

Open失敗は`Io/FileOpenFailed`、入力・Close失敗は`Io/ReadFailed`、出力・Flush・Close・Rename失敗は
`Io/WriteFailed`です。一時Directoryの作成失敗も`Io/WriteFailed`です。確保・長さの失敗は標準例外のままで、
Error生成中にも確保例外が起こりえます。確保不要のError経路は保証しません。公開状態の反映前に例外が起きても
読込前の状態と`writeAtomic`の元Fileは保持しますが、直接WriteにはFile Rollback保証がありません。
上限付き再読込・保存例は[Error Handling Cookbook](../../ja/ErrorHandlingCookbook.md)を参照してください。

## Error・検証・拡張境界

操作結果は[Result契約](ResultContract.md)に従い、厳密Resultの`error()`は失敗時のみ参照します。
空の`last_index()`など使用条件違反は標準例外です。GEFはTimeout、Cancellation、Retry、言語Bindingを追加しません。

<div class="wse-gef-evidence" style="max-width:100%; overflow-x:auto;"><div style="min-width:48rem;">

| 契約 | 実行可能な根拠 | 限界 |
| --- | --- | --- |
| GEF-BIN-01 | `wse.gef.bin_contract`: 固定Byte、独立入力、未知Tag、Payload切詰め | 任意の不正CountやBig Endianの認証ではない |
| GEF-BIN-02 | 同Test: 7型、Block順序、追記、File不在、空Index | Read／Write復旧はGEF-IO-04で別途検証 |
| GEF-CSV-01 | [`wse.gef.csv_contract`](../../../test/characterization/gef_csv_contract.cpp): CellとMap化 | 引用CSVは未対応 |
| GEF-CSV-02 | 同Test: File不在、空Data、解析Cell往復 | Parser Fuzz・Storage障害の完全検証ではない |
| GEF-IO-04 | [`wse.gef.recovery_contract`](../../../test/characterization/gef_recovery_contract.cpp): 各上限の一致・超過・0、最大Count、Read／追記Rollback、短Header、再Write／Move、一時保存・Rename・Flush失敗と再試行。Linuxは`/dev/full`とDirectory読込障害も実行 | 制御した障害注入はWindows実Disk満杯・電源断Testではない。OOM／Fuzz／Metadata／同時Writerの網羅認証ではない |
| GEF-RECON-03 | `wse.gef.reconstruction`: 独立CodecとWSEのBIN／CSV交換 | Python InterpreterとGEFが必要。限定Fixtureであり初見開発者の受入ではない |

</div></div>

## 独立した形式再現: GEF-RECON-03

[gef_reference.py](../../../test/reconstruction/gef_reference.py)はPython標準Libraryのみを使用し、
BINは明示Little Endian幅の`struct`、CSVは単純なDelimiter処理で扱います。WSE Bindingや実装をImportしません。
別Processの[Native Bridge](../../../test/characterization/gef_reconstruction_bridge.cpp)は公開GEF APIのみを
使い、Wire解析を実装しません。GEFとPython InterpreterがあればCTestへ`wse.gef.reconstruction`を登録し、
WSE Python Bindingは不要です。

1. 上記Integer16固定Vectorに対して参照Encoder／Decoderを確認します。
2. 7型Tag、空Block、同Tagの追記Blockを参照Codecで書き、新しいWSE Controllerで読んで型別値とIndexを比較します。
3. 同じ列を新しいWSE Writerで書出／追記し、参照側でDecodeして、全値・順序と独立符号化したBIN Byteの完全一致を確認します。
4. 未知Tag、切れたCount、切れたPayloadを両Readerへ渡し、参照側の拒否とWSEの指定Error Codeを確認します。
   上限のない敵対的Countの検証ではありません。
5. 空白、隣接空Cell、文字としての引用符、可変Param数、重複Row Keyを含むCSVを交換し、Cellと後勝ち設定Mapを比較します。
   比較では防御的に出力CRLFをLFへ正規化しますが、WSE自体はLF出力です。一般CRLF入力のOS間互換性は追加しません。

この実行可能な演習は、独立したCode間の互換性の根拠です。作成者は既存実装を確認しているため、初見開発者が
質問なしで形式を再現できる証明ではありません。その別受入は[Design Verification](DesignVerification.md)で定義します。

Versionなし形式へ識別不能なVersion Fieldを追加したり、Tagを変更したりしません。将来形式には明示的な識別・
移行計画が必要です。Application固有SchemaはGEF外に置きます。実行と再現の受入は
[Design Verification](DesignVerification.md)を参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
