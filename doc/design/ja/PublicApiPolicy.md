# WSE公開API共通Policy

> Canonical source: [English Public API Policy](../en/PublicApiPolicy.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的

本書はCore、XPT、GEF、IUI、OUI、Tmrおよび多言語Bindingに共通する公開API規則を定義します。
Component固有設計と矛盾する場合は、より限定的なComponent規則を優先します。今後の互換性に関わる削除は
Version方針に従い、[非推奨API・移行Guide](../../ja/DeprecationMigration.md)と機械可読台帳でReplacementおよび
SemVer major条件を管理します。

## Error境界

- 新しい失敗可能Operationは、Componentの`Result<T>`／`Status`と安定Category／Codeを返します。
  MessageとNative codeは診断専用であり、制御分岐へ使用しません。正準の構築・状態規則は
  [Result／Status契約](ResultContract.md)を参照してください。
- XPT、OUI、Tmrから多言語Bindingへの変換は、それぞれ`fromXptError()`、`fromOuiError()`、
  `fromTmrError()`だけを正規実装とします。言語Adapter内でCategory mappingを複製しません。
- C++のNative code accessorは`nativeCode() const noexcept`です。
- Modern XPT／OUI／Tmr／Binding APIからOS Exceptionを越境させません。Coreの使用条件違反は
  標準例外、予期される操作失敗は対応するComponent Resultで報告します。

## 検査なしのデータアクセス

Coreデータクラスの非const／constの`operator[]`は正式な公開APIです。
[Portable Core](PortableCore.md#データの高速アクセス)で定義する、元のメモリへの直接・非所有アクセスを維持します。
Resultへの置換や自動的な境界検査は要求しません。

## Logging

`formatLogMessage()`は標準LoggerのTAG・日時・ファイル名・行番号の書式を共有し、Sinkへ配送しない。呼出し側所有の非同期Loggerから使用でき、出力失敗は呼出し側が管理する。


- Library実装は`std::cout`、`std::cerr`、`printf`またはPlatform debug outputへ直接診断を出しません。
  Console／Fileへの実出力はWSE Logging sinkだけが担当します。
- 構造化LogはLevel、Component source、Messageおよび任意Detailsを使います。Credential、Private IP、
  Device serial、Protocol dump、Camera frameおよび機種固有Dataを既定で記録しません。
- Error返却とLog出力を同じ意味にしません。呼出側が処理可能な失敗はError値で返し、重複Logを強制しません。

## `const`、OwnershipおよびThread

- 状態を変更しないObserverは`const`、失敗しない単純Observerは`noexcept`にします。
- Getterは所有値または`const` Viewを既定とし、Mutable accessは名称と戻り型で明示します。
- OwnerはCopy不可／Move可能または共有Ownerを明示し、PIMPL、OS handle、ThreadおよびCallback stateへ
  単一RAII ownerを置きます。Raw pointerは所有に使いません。
- Callbackの実行Thread、保持期間、解除同期、再入可能操作および例外境界をComponent設計書へ記載します。

## 関数引数

引数は、名前が用途を、位置が読み手にとっての重要度を、型が誰が書き込めるかを示します。三者が一致
しているため、宣言を開かなくても呼び出し側だけで意味を読み取れます。

- すべての引数に方向の接尾辞を付けます。入力は`_in`、出力は`_out`、読んでから書く引数は`_inout`です。
  Pointer引数には`p_`接頭辞も付け、方向と受け渡し方の両方を名前で示します。
- 引数順は`[in,out]`、`[out]`、`[in]`の順です。Member関数の`this`は暗黙のin-outであるため、明示引数は
  `[out]`、`[in]`の順になります。
- `_out`と`_inout`はPointerで受け取ります。実装は先頭行で参照へエイリアスするため、本体は参照として
  読め、呼び出し側は引数が書き換えられることをその場で見て取れます。

  ```cpp
  bool coreFormatOf( wse::ePixFormat* p_format_out, const eCameraPixelFormat format_in ) noexcept
  {
      wse::ePixFormat& format_out = *p_format_out;
      ...
  ```

- 非自明型の`_in`は`const`参照で受け取ります。算術型、列挙、Pointerおよび`sTextureHandle`のような
  不透明Handleは`const`の値渡しのままにします。参照にする利得より費用が上回るためです。
- 例外は4つで、いずれも選択ではなく外部から強制されるものです。
  - Move ConstructorとMove代入はrvalue参照を取り、`X&& other_inout`と命名します。
  - `api/wse/capi/`の平坦C ABIは参照を取れないため、out引数はPointerのままです。順序と接尾辞の規則は
    他の関数と同じく従います。
  - Addressを外部のCallback typedefへ渡す関数は、そのtypedefの引数列をそのまま保ちます。順序を決めるのは
    呼び出す側だからです。引数の方向接尾辞は付けます。Tree内の該当は3つで、Media Foundation Camera
    BackendのCOM `QueryInterface` Overrideと、HTTP Clientのlibcurl Write Callback 2つです。形だけ見ると
    見落としに読めるため、それぞれに理由をCommentで残します。
  - `swap`のOverloadは2つの被演算子を参照のまま保ちます。形を決めるのは標準Libraryだからです。
    `using std::swap`の後の無修飾`swap( a, b )`は、参照を取るOverloadしか見つけません。Pointerへ
    変えると、その型が黙ってこのProtocolから外れます。方向は名前で示すため、宣言は
    `swap( X& obj1_inout, X& obj2_inout )`となります。

## 命名とNamespace

- 公開宣言は`wse`または`wse::<component>`配下に置きます。公開Headerで`using namespace`を使用せず、
  `std`、第三者またはPlatform namespaceへ宣言を追加しません。
- [Coding Rule](CodingRule.md)の型別命名表に従います。Classは`PascalCase`、Structは`s`接頭辞、
  Enum型は`e`接頭辞、Constantは`UPPER_SNAKE_CASE`を使います。Methodは`lowerCamelCase`、
  Data Classの単純Getterは`snake_case`とします。Native code accessorの名前は`nativeCode()`です。
- 規定された接頭辞やGetter表記だけを理由に非推奨化しません。実際に命名を修正する場合は、正規名を先に
  追加し、旧名を非推奨化して非推奨台帳へ登録します。同じ役割の新旧名を無秩序に追加しません。
- File名は主公開型と大文字小文字を一致させます。`stew.h`はComponent aggregate入口だけに使用します。

## Header配置と第三者型

- 公開Headerは`api/<component>/`、共通型は`api/wse/`、Binding共通境界は`api/wse/binding/`に置きます。
  `core/`、`platform/`または`vendor/`を公開HeaderからIncludeしません。
- OS Header／型、Backend object、Native handleおよび第三者Library型は公開APIへ追加しません。Adapterは
  Public WSE valueへCopy／変換して境界を閉じます。
- 第三者字句Baselineは空です。公開Data HeaderはOpenCVへ依存せず、ScanはBaselineへのEntryを禁じます。唯一の公認第三者面はOpt-inのAdapter Directory `api/cv/`
  （`cv/OpenCvAdapter.h`）であり、Scanはこれを明記の上でSkipします。IncludeするConsumerが
  OpenCVを自前で用意します。
- 公開Raw `void*` RatchetのBaselineは空です。公開APIはRaw `void*`を持たず、Boundary Gateが
  新規Entryを禁止します。Ratchetが数えるのは`void*`だけで、型付き`T*`のout引数はDebtではなく
  規約そのものです。
- Optional Componentを無効化したPackageは、そのComponent Header、Symbol、Compile definitionおよび
  第三者Dependency metadataを含めません。

## 機械Gate

`wse.public_api_policy`は公開HeaderのNamespace汚染、Linker directive、内部Include、第三者型Debt、4 Error型の
`nativeCode() const noexcept`、Logging sink外の直接診断および言語別Error mapping重複を検査します。
`wse.public_header_boundary`のOS型／Raw `void*` Ratchet、Component別Ownership GateおよびPackage verifierと
組み合わせます。Baselineを変更するCommitは、Debt削減理由、互換影響および設計書更新を同時に含めます。
`wse.deprecation_ledger`は全`[[deprecated]]`宣言とOUI opaque aliasを台帳へ対応付け、削除版、移行文書および
Removal Gateが無い変更を拒否します。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
