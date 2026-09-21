# WSE Coding Rule

> Canonical source: [English Coding Rule](../en/CodingRule.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 適用範囲

本規約はWSEが所有するC++／C Sourceへ適用します。対象は`api/`、`core/`、`platform/`、
`lang/*/native/`、`test/`および`example/cpp/`です。`vendor/`配下のVendor Code、第三者Header、
生成Codeは対象外であり、本規約へ合わせるためだけに編集しません。

各BindingのManaged Sourceはその言語の慣習に従います。`lang/cs/`はC#、`lang/java/`はJava、
`lang/js/`はJavaScript、`lang/python/wse/`はPythonです。以下のうちDirection、Ownershipおよび
文書化に関する規則は各Layerへも精神として適用しますが、綴りに関する規則は適用しません。

## 書式

- 文字CodeはUTF-8（BOMなし）、改行CodeはLFとします。
- Indentは空白4文字とし、TabでIndentしません。
- BraceはAllman形式とし、開始Braceを次行へ置きます。
- Pointer／Reference記号は型側へ付けます。`const char* text`、`sCameraFrame& frame`。
- Function Callは括弧内へ空白を入れます。`readFrame( timeout_ms_in )`。
- Headerは`#pragma once`とMacro Guardを併用します。GuardはUPPER_SNAKE_CASEとし、Project名と
  Pathを含めて衝突を避けます。
- Header内で`using namespace`を使用しません。

```cpp
if ( frame_in.description.width == 0U )
{
    return CameraStatus::failure( error );
}
```

## FileとNamespace

- Class／Structを主責務とするFileは1主要型1Fileとし、File名を主要型名へ合わせます。
- C++ Headerは `.h`、Sourceは `.cpp` とします。
- 公開宣言は`wse`または`wse::<component>`へ置きます。File単位で無関係なNamespaceを増やしません。

## 命名規則

| 対象 | 規則 | 例 |
|---|---|---|
| Class | `PascalCase` | `WebCamera` |
| Struct | `s` + `PascalCase` | `sCameraFrame` |
| Enum型 | `e` + `PascalCase` | `eCameraPixelFormat` |
| Enum値 | `PascalCase` | `Rgb8` |
| Function／Method | `lowerCamelCase`、動詞開始 | `readAveragedFrame()` |
| Data Classの単純Getter | Member Prefixを除いた`snake_case` | `native_format()` |
| Local Variable | `snake_case` | `frame_index` |
| Function Argument | `snake_case` + Direction Suffix | `timeout_ms_in` |
| out／inout Pointer Argument | `p_` + `snake_case` + Suffix | `p_frame_out` |
| Global Variable | `g_` + `snake_case` | `g_state_mutex` |
| Pointer Variable | `p_` + `snake_case` | `p_buffer` |
| Class Member | `m_` + `snake_case` | `m_impl` |
| Class Member（Pointer） | `m_ptr_` + `snake_case` | `m_ptr_data` |
| Constant | `UPPER_SNAKE_CASE` | `READ_TIMEOUT_MS` |
| Macro | `UPPER_SNAKE_CASE`、`WSE_` Prefix | `WSE_CAPI_ABI_VERSION` |
| Namespace | 小文字 | `wse::tmr` |

処理を行うFunctionは、何を行うか分かる動詞から開始します。`updateState()`とし、`stateUpdate()`、
`stateChecker()`、`doWork()`のような名詞開始または意味の弱い動詞は使用しません。

例外はData Classの単純Getterだけです。Data ClassとはDataの保持と受渡しを主責務とし、I/O、
State Machine、Resource管理、Domain処理を主責務に持たないClassを指します。そのGetterは`m_`を
除いてMemberとして読める形にします。`device_id()`。それ以外のClassは動詞形とし、`getDeviceId()`
または`isConnected()`とします。SetterはClassの種類によらず動詞開始とします。

## Function Argument

Argumentは、名前が用途を、位置が読み手にとっての重要度を、型が書込み可否を示します。三者が一致
するため、宣言を開かずに呼出しを読めます。

- 全ArgumentにDirection Suffixを付けます。入力は`_in`、出力は`_out`、読んでから書くものは
  `_inout`です。Pointer Argumentは`p_` Prefixも付け、方向と受渡し方の両方を名前で示します。
- Argumentは`[in,out]`、`[out]`、`[in]`の順に並べます。Member Functionの`this`は暗黙のin,outで
  あるため、明示Argumentは`[out]`、`[in]`の順になります。Doxygenの`@param`も同じ順です。
- `_out`および`_inout`はPointerで受け取ります。実装は先頭行でReferenceへAliasし、本体は
  Referenceとして読めるようにします。呼出側では書込み対象であることが引数の形から見えます。

  ```cpp
  bool coreFormatOf( wse::ePixFormat* p_format_out, const eCameraPixelFormat format_in ) noexcept
  {
      wse::ePixFormat& format_out = *p_format_out;
      ...
  ```

- `_in`は値渡し、Pointer、Referenceを問わず`const`を明示します。非自明型は`const`参照で受けます。
  算術型、Enum、Pointerおよび`sTextureHandle`のような不透明Handleは`const`値渡しのままとします。
  これらへの参照は、得られるものより負担が大きいためです。
- `_out`および`_inout`は書込み対象そのものであるため、対象自体を`const`にしません。
- 方向が混在するArgumentを複数行に書く場合は、型名の開始位置を`[in]`の型名位置へ揃えます。
  `[in]`には`const`があるため、`[in,out]`と`[out]`は`const `に相当する幅を確保します。

例外は4つで、いずれも選択ではなく外部から強制されるものです。

- Move ConstructorとMove代入はrvalue参照を取り、`X&& other_inout`と命名します。
- `api/wse/capi/`の平坦C ABIは参照を取れないため、out ArgumentはPointerのままです。順序と
  接尾辞の規則は他のFunctionと同じく従います。
- Addressを外部のCallback typedefへ渡すFunctionは、そのtypedefのArgument列をそのまま保ちます。
  順序を決めるのは呼び出す側だからです。Direction Suffixは付け、形が固定である理由をCommentで
  残します。
- `swap`のOverloadは2つの被演算子を参照のまま保ちます。形を決めるのは標準Libraryだからです。
  `using std::swap`の後の無修飾`swap( a, b )`は、参照を取るOverloadしか見つけません。方向は
  名前で示すため、宣言は`swap( X& obj1_inout, X& obj2_inout )`となります。

## 型と値

- 変更しない値は`const`とします。
- 新規C++ Codeは`nullptr`、C Codeは`NULL`を使用します。
- CastはC++ Castを使用し、C Style Castは新規Codeに現れません。
- 固定幅型は`<cstdint>`から取ります。WSEの別名では`double_*`より`float64_*`の綴りを優先します。
- 単位を持つ値は名前またはCommentへ単位を明記します。`timeout_ms`、`interval_us`。
- Protocol値、Port、Timeout、制御値など意味を持つ値はConstant化します。0、1、単純Indexまで
  Constant化する必要はありません。

## Class

- 単一引数ConstructorおよびType変換Constructorは、暗黙変換を意図しない限り`explicit`とします。
- 例外を送出しないMove操作は`noexcept`とします。
- Rule of Zeroを優先します。Resourceを直接所有する型はRule of Fiveを検討します。
- 継承を意図しない型への`final`は、設計意図が明確になる場合に使用します。

## メンバ初期化

- 非staticメンバはConstructorの初期化子リストで初期化する。宣言時の`= value`、`{ value }`を禁止する。Project所有のClass、Struct、Test、Sample、Native Bindingへ適用する。
- メンバ宣言順に1メンバ1行で並べ、先頭は`:`、2行目以降はLeading Commaとする。
- メンバ名の後ろを空白で埋め、同一初期化子リスト内の`(`の列を揃える。最長の名前の後ろにも1文字以上の空白を置く。C++17の組込み配列等は必要な`{`を同じ列へ揃える。
- Constructor本体の代入で初期化を代用しない。Copy／Move、定数式、既存の波括弧呼出し、Binary Layoutを維持する。Copy／Moveの`= default`は使用できる。
- static定数、`static constexpr`、CとしてCompileするC ABI Struct、Vendor、自動生成Codeは対象外とする。

```cpp
Example::Example()
    : m_state      ( State::Created )
    , m_log_sink   ( nullptr )
{
}
```

## 整列

同一論理Block内で、読み手が比較するものを揃えます。

- 宣言はVariable名、`=`、初期値および行末`//!<` Commentを揃えます。
- Struct／Class MemberはMember名と行末Commentを揃えます。
- Enumは1項目1行とし、Identifier、`=`、値およびCommentを揃えます。2項目目以降はLeading Comma
  形式とします。最初のEnum値の前へ`,`を置く形式はC++のSyntaxとして無効なため使用しません。

```cpp
enum class eCameraBackend : std::uint8_t
{
      Automatic = 0U  //!< Platformが選ぶBackend.
    , MediaFoundation  //!< Windows Media Foundation.
};
```

意味のまとまりごとに揃えます。File全体を無理に揃えず、不自然な大量空白を入れてまで整列しません。

## Function CallとAggregate

Function Callは、Argumentが4個以上のとき、同じ型のArgumentが複数あり取り違えやすいとき、数値
Literalが複数あるとき、およびNetwork／Device／Protocolを設定するときに、1 Argument 1行とします。
各行へ何を設定するかを`//` Commentで簡潔に付け、Comment開始位置を揃えます。

TableやDescriptorはLeading Comma形式とし、項目ごとにCommentを付けます。短い固定配列は1行に
まとめます。

## Doxygen Comment

- WSEが所有する全FileはDoxygen File Headerで始めます。`@file`と、`\~japanese`／`\~english`の
  両方の`@brief`を記載します。
- 詳細DoxygenはHeaderの宣言へ置きます。Sourceの定義では重複させません。
- Headerを持たないFunctionは、定義側へ詳細Doxygenを置きます。
- `void` Functionへ`@return`は書きません。
- 単純なFile Scope／Namespace Scope VariableとConstantは宣言行末の`//!<`で説明します。複雑な
  Tableは直前のDoxygen Commentを使用します。

## 通常Comment

- Function内の処理手順、処理Blockの目的、Thread／Network制約および安全上の注意は、Doxygenでは
  なく通常Comment `//`で記載します。
- 本体が概ね20行を超えるFunctionは、処理のまとまりごとにCommentを付けます。空行、Braceおよび
  Doxygen Commentは行数判断から除外します。
- Commentは理由、またはそのBlockが達成することを述べます。直下の行を言い換えません。
- 概ね500行を超えるSource Fileは責務単位でSection Commentを入れます。小さいFileには不要です。
- Comment末尾はASCIIの`.`とします。同一File内で`.`と`。`を混在させません。

## Functionの長さと責務

長さだけで分割は強制しませんが、概ね20行を超える本体は処理Block Commentを持ち、概ね100行規模に
なった場合は分割を検討します。2つの責務を持つFunctionは長さによらず分割候補です。深いNestは
Early Returnで単純化します。

## Error

- Componentが定義するResult／Error契約で失敗を運びます。Component間で単一の戻り値形式へ一律
  統一しません。
- 0除算、範囲外および不正引数は、それを受け取る境界で検証します。
- Errorは呼出し元へ返すか、定義済みの経路で記録します。握り潰しません。

## Compiler Warning

WarningはImplementation側で解消します。抑制が避けられない場合は対象範囲を最小化し、理由を
Commentへ必ず残します。

Core CIは`WSE_WARNINGS_AS_ERRORS`でGCC警告ゼロを維持します。MSVCのSweepでは既存DLL Interface
警告の承認済みFile別Baselineを保持します。警告検査はColumn番号の有無とWindows Drive Letterを扱います。
Baselineは減少だけを許し、新規Fileはゼロから開始します。警告FlagはNative Library Target内に限定します。
Optional ComponentのSweepはこのCore保証とは別です。[Build Guide](../../ja/BuildGuide.md)を参照してください。

## Review時の確認

- [ ] Function名が動詞から始まり、例外はData Classの単純Getterだけになっている.
- [ ] Structが`sPascalCase`、Enum型が`ePascalCase`、ConstantがUPPER_SNAKE_CASEになっている.
- [ ] Globalが`g_`、Memberが`m_`、Pointer Memberが`m_ptr_`になっている.
- [ ] 全Argumentが`_in`／`_out`／`_inout`を持ち、out／inout Pointerが`p_`を持っている.
- [ ] Argument順が`[in,out]`、`[out]`、`[in]`であり、`@param`も同順になっている.
- [ ] 全`[in]`へ`const`が付き、out／inoutはPointerで先頭行にAliasがある.
- [ ] 方向混在の複数行Argumentで型名位置が`[in]`へ揃っている.
- [ ] 宣言、Member、EnumがBlock内で揃っている.
- [ ] 多Argument Function Callが1項目1行になり、各行へ揃ったCommentがある.
- [ ] HeaderとSourceで詳細Doxygenを重複せず、`void` Functionへ`@return`がない.
- [ ] 20行を超えるFunctionへ処理Blockの通常Commentがある.
- [ ] Allman形式、空白4文字、UTF-8（BOMなし）、LFになっている.
- [ ] 単位が明記され、意味を持つ値がConstant化されている.
- [ ] Warningを抑制ではなく実装修正で解消している.
- [ ] Vendor／生成Codeへ本規約を適用していない.

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
