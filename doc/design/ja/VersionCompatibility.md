# WSE Version／Compatibility設計

> Canonical source: [English Version and Compatibility Design](../en/VersionCompatibility.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

WSEはSemantic Versioningを使用します。現在のVersionは1.0.0で、初回公開まで1.0.0を維持します。WSEが未公開の間、Ownerは非推奨サイクルの代わりに審査済みのその場変更を
承認できます。そのような変更はすべて根拠とともに
[非推奨・移行Guide](../../ja/DeprecationMigration.md)へ記録します。

## 変更区分

- Patch: 互換Fix、文書、Test、公開Source挙動を意図的に変えない実装変更
- Minor: AdditiveなAPI、Component、Backend、Binding Capability。既存名の意味は維持
- Major: Migration／Release Gate合格後に承認された非互換削除または意味変更

## 互換性保証

C++ APIとFlat C ABIは越える境界が異なるため、異なる約束をします。

### C++ API

- WSE 1.0.0内ではSource互換を維持します。削除の前にReplacementと移行経路を用意し、削除自体は
  文書化されたSemVer-major Gateを必要とします。
- Binary互換は、Compiler family、Toolset、標準Library、Architecture、Library形式が一致するBuild間で
  だけ成立します。公開Headerは標準Library型を公開するため、Compiler非依存のC++ ABIは約束しません。
  Install PackageはToolsetとLibrary形式を記録し、Package構成は不一致ConsumerをSilent loadせず拒否します。

### C ABI（`api/wse/capi`）

Flat C ABIはRuntime間の安定境界であり、以下の保証はすべて契約の一部です。

- `WSE_CAPI_ABI_VERSION`が契約に名前を付けます。呼出側は実際にLoadしたLibraryの
  `wse_capi_abi_version()`を読み、Compile時の値と照合します。
- 境界を越えるのはOpaque handleと固定幅整数型だけで、全関数は`wse_capi_status`を値で返します:
  Portableな`category`、Component固有の`code`、生の`native_code`。
- 呼出規約は明示宣言します（Windowsでは`__cdecl`）。
- Handleはすべて呼出元所有で、対応する`*_destroy`関数で解放します。
- `wse_capi_last_error_message()`の失敗Messageは呼出Threadごとに保持され、そのThreadの次の
  C ABI呼出まで有効です。
- これらへの非互換変更（Field配置、所有権規則、数値再解釈）は`WSE_CAPI_ABI_VERSION`を増やします。

## 非推奨と削除

非推奨登録は削除予定ではありません。削除にはOwner承認、Replacementを記載した1 Minor release以上の
期間、該当する公開／Private／Hardware Consumer 0件、警告Error化したPackage ConsumerおよびRelease
互換審査が必要です。機械台帳と移行詳細は
[非推奨Guide](../../ja/DeprecationMigration.md)を参照してください。

## Package Identity

Install PackageはWSE Version、Library形式、Platform、Architecture、Compiler／Toolset、有効Component、
Binding ABI、Dependency Identityを記録し、不一致PackageをSilent loadせず拒否します。Binding ABI 1は
Node／Python／Java共通で、.NET BindingはC ABIを使用します。非互換Field、Ownershipまたは数値Error
再解釈には新しいBinding ABIが必要です。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
