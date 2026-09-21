# WSE License

> Canonical source: [English WSE License](../en/LICENSE.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-21

公開WonderStewEngine SDKにはApache License, Version 2.0を適用します。
正式なLicense本文はRepository rootの[`LICENSE`](../../LICENSE)、Projectの帰属表示は
[`NOTICE`](../../NOTICE)にあります。本ページはその要約です。

## 適用範囲と許諾

別のThird-party licenseが明示されたFileを除き、公開SDKのSource、Header、Language binding、
文書、Example、Projectの素材に適用します。Licenseの条件に従って、商用・非商用の利用、
改変、再配布ができます。利用Applicationや別LicenseのExtensionはProprietaryのままにできます。
Apache-2.0は、それらのSourceやSDK改変部分のSourceの公開を要求しません。

再配布時は第4条に従ってLicenseを提供し、該当する表示を保持し、変更Fileを明示して、
適用されるNOTICEの帰属表示を再掲します。LicenseはContributorの特許許諾を含みますが、
一般的な商標利用権は許諾せず、Softwareは無保証です。既存のSource license識別子は`Apache-2.0`です。

## Packageと依存物の境界

公開SDKのInstall packageは`LICENSE`と`NOTICE`を同梱し、ManifestとSBOMにApache-2.0を記録します。
Third-party componentには各自のLicenseと表示が適用され、該当するSource提供や再Linkなどの条件も
維持されます。SDKのLicenseはそれらを置き換えません。選択したPackageの依存物一覧とLicense fileを確認してください。

別途供給されるAccess-controlled Extensionは、このLicenseの対象外です。Extensionを含むPackageは
`LICENSE.extension`も同梱し、組合せのLicense expressionを記録します。公開SDKのApache許諾は、
Extensionに対する権利を許諾しません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-21 | 公開SDKにApache-2.0を適用し、Extensionと依存物の別条件を記載した。 |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
