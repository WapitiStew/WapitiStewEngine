# WSE Result／Status契約

> Canonical source: [English Result and Status Contract](../en/ResultContract.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

本書は現在のC++ Result／Statusの表現、Accessor規則、Component境界を定義します。
実装は[wse_Result.h](../../../api/wse/utility/wse_Result.h)です。

## 厳密Resultの状態

`wse::Result<T, E>`は値を持つ成功またはErrorを持つ失敗を表します。
公開構築APIは`success(T)`と`failure(E)`です。公開の既定Constructor、値だけのConstructor、
値とErrorを受けるConstructorはありません。`failure(E)`は空Error（`E::ok() == true`）を
`std::logic_error`派生の`wse::ResultAccessError`で拒否します。

実装は`std::optional<T>`と`E`を所有します。Optionalに値があるのは成功時だけで、失敗時に`T`は
構築しません。保持するErrorが意味を持ち読み出せるのは失敗時だけです。これは意味上の排他契約であり、
Union配置やBinary直列化を保証しません。Resultは値とErrorを所有し、Accessorの参照にはResultの生存と
格納領域が置き換わらないことが必要です。

`wse::Status<E>`は`Result<void, E>`の別名です。この特殊化は成功FlagとErrorを所有し、Payloadを
持ちません。Factoryは`success()`と`failure(E)`です。Bool Payloadも公開Bool Constructorもありません。
Result、Status、`succeeded()`は`[[nodiscard]]`です。

| 操作 | 成功時 | 失敗時 |
| --- | --- | --- |
| `succeeded()` | true | false |
| `Result<T,E>`の`value()` | 所有値へのConst／変更可能参照 | 保持するCategory／Code／Messageを含む`ResultAccessError` |
| `Result<T,E>`の`valueOr(fallback)` | 値のCopyを返す | 代替値を返す |
| `error()` | `ResultAccessError` | 所有ErrorへのConst参照 |

`valueOr`は失敗Resultを理由とする`ResultAccessError`を投げません。ただし`noexcept`ではなく、
引数構築を含む`T`のCopy／Moveは例外を投げ得ます。Factoryの確保、値／Errorの構築、診断文字列の構築も
例外を投げ得るため、Resultはすべての例外を封じる境界ではありません。

## Component Errorと別名

`E`は`category()`、`code()`、`message()`、`nativeCode()`、`ok()`を提供します。
Categoryは共通の12値分類に従います。分岐契約はCategoryとComponent Codeで、MessageとNative Codeは
診断用です。正規化規則は[公開API Policy](PublicApiPolicy.md)に従います。

| Component | Error | 厳密Result／Status |
| --- | --- | --- |
| Core計算 | `wse::CoreError` | `CoreResult<T>`／`CoreStatus` |
| License | `wse::LicenseError` | `LicenseResult<T>`／`LicenseStatus` |
| GEF | `wse::gef::GefError` | `GefResult<T>`／`GefStatus` |
| XPT | `wse::xpt::TransportError` | `TransportResult<T>`／`TransportStatus` |
| OUI | `wse::oui::RendererError` | `RendererResult<T>`／`RendererStatus` |
| Tmr | `wse::tmr::CameraError` | `CameraResult<T>`／`CameraStatus` |
| Binding Facade | `wse::binding::Error` | `binding::Result<T>`／`binding::Status` |

これらは共通Templateの別名で、独立した状態機械ではありません。
IUIの`KeyboardAccessState`はComponentの観測状態であり、厳密Resultではありません。

## 部分進捗

`wse::PartialResult<T,E>`は値とErrorの両方を所有します。Explicitな値Constructorは空Errorを持ち、
値／Error Constructorは両方を受け取ります。`succeeded()`は`E::ok()`を判定します。
`value()`と`error()`は両状態で読め、失敗時にも進捗や応答が意味を持ちます。

XPTの`TransferResult<T>`は転送数／Dataにこの形を使い、`HttpResult`は完了した4xx/5xx交換を含め、
HTTP応答とErrorを同時に保持します。部分値は全量到達、相手の受領、自動再送の安全性を意味しません。
操作ごとの意味とRetry規則は[XPT](XptTransport.md)に従います。
厳密ResultのAccessor前提条件はPartialResultには適用しません。

## 非同期と多言語境界

Const参照でResultを届ける非同期APIでは、その参照をCallback中だけ借用します。
保持が必要な値はCopyします。実行Thread、取消し、例外、停止はComponent設計と
[Thread Ownership](ThreadOwnership.md)に従います。

`fromXptError`／`fromOuiError`／`fromTmrError`／`fromIuiKeyboardState` Adapterは`binding::Error`へ
正規化します。[C ABI](CAbiContract.md)は`wse_capi_status`とThread単位の
`wse_capi_last_error_message`で報告し、言語Adapterは対応する言語のError契約を公開します。
所有と部分進捗の転送は[Language Bindings](LanguageBindings.md)に従います。

## 検証

`wse.core.result_contract`として登録する[Result契約Test](../../../test/characterization/wse_result_contract.cpp)は、
成功／失敗時のAccessor、空Error拒否、代替値、Status、部分進捗を検査します。
実際の操作結果の意味はXPT／Component Testが検証します。これらのTestは全確保失敗経路や実機動作を
証明するものではありません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
