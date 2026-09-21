# WSE Security／Privacy設計

> Canonical source: [English Security and Privacy Design](../en/SecurityPrivacy.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Trust境界

Network Payload、Serial入力、Camera Metadata／Frame、File内容、環境PathおよびBinding ArgumentはUntrustedです。
公開AdapterはSize、Range、Encoding、Timeout、Stateを検査してからBackendへ渡します。ErrorはPortable
Category／Codeと診断用Native codeを保持し、危険な操作をSilent retryしません。

XPTはTLSを既定で検証し、Response sizeを制限してRequest lifetimeを所有します。Camera Extension Unit操作には
明示Control ID、Bounded bufferおよびCapability確認が必要です。Frame／Binding Bufferは所有Copyであり、Backend
再利用によってConsumer Dataが変化しません。

## Dependency完全性

Bootstrapは固定Source identityまたはSHA-256を検査し、License／Platform／Architecture Metadataを記録します。
Offline modeはNetworkへ接続しません。Release stagingは署名、NOTICE、SBOM、Provenance、Archive展開検査を独立Gate
として実行します。

## 公開／Private分離

Extension名称と互換Source、Device Profile、Protocol詳細および識別MetadataはAccess-controlledの場合があります。
公開Header、言語Binding、Package、文書、Log、Sanitized historyから除外し、Public exportはPrivate denylist scanと人手Reviewを
要求します。Repository Visibility／公開ReleaseはOwnerだけが決定します。

公開検査Toolには構造Ruleと架空のTest識別子を置きます。具体的な非公開識別子は、公開Checkoutの外にある
辞書を`WSE_EXPORT_DENYLIST`で指定します。辞書を指定しないPackage／Candidate検査は構造検査だけです。
公開前には完全な非公開辞書による内容検査と人手Reviewを必須とし、辞書と証跡を公開Sourceや生成物に含めません。

Camera／DisplayのBuildまたはMock Testを物理Device認証として表示しません。実機証跡は
[Hardware Validation](HardwareValidation.md)に従います。

## 変更履歴

| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 外部辞書による公開前検査の境界を含む公開初版を記載。 |
