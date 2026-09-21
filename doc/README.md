# WSE Documentation Index

WonderStewEngine（WSE）の文書入口です。

## User Documentation

- [日本語](ja/README.md)
- [日本語 Getting Started](ja/GettingStarted.md)
- [日本語 Build／Install Guide](ja/BuildGuide.md)
- [日本語 How-to](ja/HOWTO.md)
- [日本語 API Reference](ja/API_REFERENCE.md)
- [日本語 WebCamera移行Guide](ja/WebCameraMigration.md)
- [日本語 非推奨API・移行Guide](ja/DeprecationMigration.md)
- [日本語 Error Handling Cookbook](ja/ErrorHandlingCookbook.md)
- [日本語 Support Matrix](ja/SupportMatrix.md)
- [日本語 License](ja/LICENSE.md)
- [English](en/README.md)
- [English Getting Started](en/GettingStarted.md)
- [English Build and Install Guide](en/BuildGuide.md)
- [English How-to](en/HOWTO.md)
- [English API Reference](en/API_REFERENCE.md)
- [English WebCamera Migration Guide](en/WebCameraMigration.md)
- [English Deprecation and Migration Guide](en/DeprecationMigration.md)
- [English Error Handling Cookbook](en/ErrorHandlingCookbook.md)
- [English Support Matrix](en/SupportMatrix.md)
- [English License](en/LICENSE.md)

## Design

開発者はArchitecture → Core Data Model → 対象Component → Thread／Result →
Design Verificationの順で読むと、構造・データ・処理・検証を追えます。
Start with Architecture, then Core Data Model, the component design, Thread/Result, and Design Verification.

GEFには独立Codecとの相互読込検証、IUI／XPTにはOwner・状態・停止・失敗経路とTest対応を記載しています。
GEF includes an independent-code interoperability exercise; IUI/XPT detail owners, states, shutdown and failure paths.

TmrはCameraの選択・Callback・Frame配置、OUIはResource寿命・Backend処理順序・Projectionの計算例を記載しています。
Tmr details camera selection, callback states and frame layout; OUI covers resource lifetime, backend sequences and projection vectors.

- [Core data model (English)](design/en/CoreDataModel.md)
- [Coreデータモデル（日本語）](design/ja/CoreDataModel.md)
- [Core algorithms (English)](design/en/CoreAlgorithms.md)
- [Core演算設計（日本語）](design/ja/CoreAlgorithms.md)
- [Core services (English)](design/en/CoreServices.md)
- [Core共通サービス設計（日本語）](design/ja/CoreServices.md)
- [GEF file formats (English)](design/en/GefFileFormats.md)
- [GEF File形式（日本語）](design/ja/GefFileFormats.md)
- [Design verification (English)](design/en/DesignVerification.md)
- [設計と検証の対応（日本語）](design/ja/DesignVerification.md)
- [Developer walkthrough (English)](design/en/DeveloperWalkthrough.md)
- [開発者向け実習（日本語）](design/ja/DeveloperWalkthrough.md)
- [Coding rule (English)](design/en/CodingRule.md)
- [Coding Rule（日本語）](design/ja/CodingRule.md)

- [Documentation generation (English)](design/en/DocumentationGeneration.md)
- [文書生成設計（日本語）](design/ja/DocumentationGeneration.md)

- [Architecture (English)](design/en/Architecture.md)
- [Architecture（日本語）](design/ja/Architecture.md)
- [Portable Core design (English)](design/en/PortableCore.md)
- [Portable Core設計（日本語）](design/ja/PortableCore.md)
- [Public API policy (English)](design/en/PublicApiPolicy.md)
- [公開API共通Policy（日本語）](design/ja/PublicApiPolicy.md)
- [XPT Transport design (English)](design/en/XptTransport.md)
- [XPT Transport設計（日本語）](design/ja/XptTransport.md)
- [IUI Keyboard design (English)](design/en/IuiKeyboard.md)
- [IUI Keyboard設計（日本語）](design/ja/IuiKeyboard.md)
- [OUI Renderer design (English)](design/en/OuiRenderer.md)
- [OUI Renderer設計（日本語）](design/ja/OuiRenderer.md)
- [OUI Projection design (English)](design/en/OuiProjection.md)
- [OUI Projection設計（日本語）](design/ja/OuiProjection.md)
- [Tmr Camera design (English)](design/en/TmrCamera.md)
- [Tmr Camera設計（日本語）](design/ja/TmrCamera.md)
- [Language Binding design (English)](design/en/LanguageBindings.md)
- [多言語Binding設計（日本語）](design/ja/LanguageBindings.md)
- [Flat C ABI contract (English)](design/en/CAbiContract.md)
- [平坦C ABI契約（日本語）](design/ja/CAbiContract.md)
- [Thread and ownership model (English)](design/en/ThreadOwnership.md)
- [Thread／Ownership Model（日本語）](design/ja/ThreadOwnership.md)
- [Build and packaging design (English)](design/en/BuildPackaging.md)
- [Build／Packaging設計（日本語）](design/ja/BuildPackaging.md)
- [Version and compatibility design (English)](design/en/VersionCompatibility.md)
- [Version／Compatibility設計（日本語）](design/ja/VersionCompatibility.md)
- [Result and status contract (English)](design/en/ResultContract.md)
- [Result／Status契約（日本語）](design/ja/ResultContract.md)
- [Security and privacy design (English)](design/en/SecurityPrivacy.md)
- [Security／Privacy設計（日本語）](design/ja/SecurityPrivacy.md)
- [Hardware validation design (English)](design/en/HardwareValidation.md)
- [Hardware Validation設計（日本語）](design/ja/HardwareValidation.md)

Paired English/Japanese documents, documentation versions, and synchronization dates are tracked by
[DocumentationManifest.tsv](DocumentationManifest.tsv). English design is canonical.

## Reports

調査・移行計画・仕様検討記録は、統制対象の実装と同じ別Repositoryで管理します。
参照するには、そのRepositoryへのAccessが要ります。

`report/`配下はNon-Normativeです。確定仕様は、正式な設計書、公開API仕様、
Build手順および各言語の利用Guideを参照してください。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
