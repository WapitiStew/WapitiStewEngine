# WSE Design Verification

> Canonical source: [English Design Verification](../en/DesignVerification.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的と読み方

設計からWSEの振る舞いを再構成するため、契約と実行可能な根拠、その検証限界を結び付けます。
Testの掲載は読者の環境での成功記録ではありません。実機状態は[Hardware Validation](HardwareValidation.md)、
過去の実行証跡は本書の外に保持します。

[Architecture](Architecture.md)で責務とSourceを確認し、Testの前にComponent契約を読みます。
英語設計と公開Headerが規範です。実装との不一致は調査対象とし、実装をそのまま仕様へ転記して解決しません。

## 契約と根拠の対応

CTest登録は[WseTests.cmake](../../../cmake/WseTests.cmake)にあります。
有効ComponentとRuntimeによって登録されるTestが変わります。
狭い画面では表を横へスクロールすると4列すべてを読めます。

<div class="wse-contract-matrix" style="max-width:100%; overflow-x:auto;">
<div style="min-width:48rem;">

| 契約・主張 | 設計 | Test入口 | 検証限界 |
| --- | --- | --- | --- |
| CORE-DATA-01: Shape、添字 | [Core Data Model](CoreDataModel.md) | `wse.core.map_invariants`、`wse.core.fast_indexing` | 検査なしAccessの不正Indexを保証しない |
| CORE-DATA-02: 値の所有 | [Core Data Model](CoreDataModel.md) | [Map Source Review](../../../api/wse/data/wse_Map.h) | 全Move／View寿命の検証ではない |
| CORE-DATA-03: Pixel配置 | [Core Data Model](CoreDataModel.md) | `wse.core.pixel_storage`、`wse.core.image_channel_order` | Host値は一般の通信形式ではない |
| CORE-DATA-04: 外部行 | [Core Data Model](CoreDataModel.md) | `wse.core.image_interleaved` | 対応Interleaved Formatのみ |
| CORE-ALG-01/02: 共分散・行列式・逆行列 | [Core演算](CoreAlgorithms.md) | `wse.core.matrix_correctness` | 既知値・Pivot・準特異入力。任意の条件数の保証ではない |
| CORE-ALG-03: 点対応 | [Core演算](CoreAlgorithms.md) | [Projection geometry](../../../test/characterization/projection_geometry_contract.cpp) | OUI有効時。全縮退処理の保証ではない |
| CORE-ALG-04/05: 向き・累積 | [Core演算](CoreAlgorithms.md) | `wse.core.image_algorithms` | 矩形固定値・丸め・Shape拒否後の状態。和のOverflow保証なし |
| CORE-ALG-06: Bayer／色の式 | [Core演算](CoreAlgorithms.md) | `wse.core.image_algorithms`、`wse.tmr.camera_frame_ops` | Core固定値はTmr不要。Camera形式はTmr有効時。画質認証ではない |
| CORE-SVC-01/02: Wait・Timer | [Coreサービス](CoreServices.md) | `wse.core.runtime_contract`、`wse.core.worker_controller` | Interval更新時点と終了後待機の順序はSource Reviewも必要。Real-time保証ではない |
| CORE-SVC-03/04: Log配送・Sink | [Coreサービス](CoreServices.md) | `wse.core.runtime_contract` | Filter・自己解除・再入・例外。並行解除の網羅検証ではない |
| CORE-SVC-05: License状態 | [Coreサービス](CoreServices.md) | `wse.core.license_contract` | Free FallbackはHeader／Source Review。任意File・Device Identityの検証ではない |
| CORE-SVC-06: Device照会 | [Coreサービス](CoreServices.md) | `wse.core.runtime_contract`のLinux分岐 | Metadata列挙。実機受入ではない |
| GEF-BIN-01: Byte列 | [GEF](GefFileFormats.md) | `wse.gef.bin_contract` | 固定Byte、切詰め、未知Tag。Big Endian認証ではない |
| GEF-BIN-02: Blockと追記 | [GEF](GefFileFormats.md) | `wse.gef.bin_contract` | 再Write・復旧はGEF-IO-04で検証 |
| GEF-CSV-01/02: Cellと設定 | [GEF](GefFileFormats.md) | `wse.gef.csv_contract` | 単純LF／Comma Fixture。引用CSVではない |
| GEF-IO-04: 上限付き読込・保存復旧 | [GEF](GefFileFormats.md) | `wse.gef.recovery_contract` | 上限一致・超過、Rollback、短Header、再Write／Move、一時Write／Rename／Flush失敗と再試行。電源断・OOM認証ではない |
| GEF-RECON-03: 独立Codec | [GEF](GefFileFormats.md) | `wse.gef.reconstruction` | GEFとPython Interpreter。7 Tag、空／追記、不正入力、CSV Map。初見第三者Reviewではない |
| Worker lifecycle | [Thread Ownership](ThreadOwnership.md) | `wse.core.worker_controller`、`wse.core.runtime_contract` | 任意User Callbackの時間制限ではない |
| 厳密／部分進捗Result | [Result](ResultContract.md)、[XPT](XptTransport.md) | `wse.core.result_contract`、XPT契約 | 部分進捗では値／Error Access前提が異なる |
| XPT-OP-01～XPT-HTTP-06: Context・Transport状態・Retry | [XPT](XptTransport.md) | [TCP](../../../test/characterization/xpt_tcp_loopback.cpp)、[UDP](../../../test/characterization/xpt_udp_loopback.cpp)、[Serial](../../../test/characterization/xpt_serial_port_contract.cpp)、[HTTP](../../../test/characterization/xpt_http_contract.cpp)、[Retry](../../../test/characterization/xpt_retry_policy_contract.cpp) | Loopback／PTYは実機受入ではない。Deadlineの限界はSource確認も必要 |
| IUI-STATE-01～IUI-LIFE-04: 観測、Poll、Owner | [IUI](IuiKeyboard.md) | Lifecycleと`wse.iui.keyboard_devices_contract` | 拒否・再接続・結合・確保巻戻しを注入。Kernel時刻やCallback全競合の証明ではない |
| TMR-OWNER-01 / TMR-OPEN-02: Camera所有・選択 | [Tmr](TmrCamera.md) | `wse.tmr.webcamera_contract`とBackend／Source確認 | Fake Backend。選択Cacheと部分Open失敗を網羅しない |
| TMR-CALLBACK-03: 配信とStreaming | [Tmr](TmrCamera.md) | `wse.tmr.camera_backend_contract`、`wse.tmr.camera_start_stop_contract` | Callable／Thread／Read故障、巻戻し、公開、Read／Stop／Owner／自己Stop競合。全確保点／Native故障の網羅ではない |
| V4L2 Queue lifecycle | [Tmr](TmrCamera.md) | `wse.tmr.v4l2_lifecycle_contract` | 実AdapterにQBUF／STREAMON／STREAMOFF故障を注入し再開と後始末順序を確認。Deviceは開かない |
| TMR-NATIVE-05: Frame返却／待機予算 | [Tmr](TmrCamera.md) | `wse.tmr.v4l2_read_contract`、`wse.tmr.media_foundation_frame_contract` | V4L2 27件・MF 16件の人工故障。全確保・libcamera・実機復旧は網羅しない |
| TMR-LIBCAMERA-06: Request／Mappingトランザクション | [Tmr](TmrCamera.md) | `wse.tmr.libcamera_resources_contract`、`wse.tmr.libcamera_manager_contract`、有効構成の`wse.tmr.camera_contract` | 本番Helperの人工故障と実Adapter Build／列挙。実撮影やlibcamera内部故障注入ではない |
| TMR-MF-INIT-07: 初期化所有権 | [Tmr](TmrCamera.md) | `wse.tmr.media_foundation_resources_contract` | Owner／故障／属性の29ケース。実機、Native factory故障注入、Threadをまたぐ解放の認証ではない |
| TMR-MF-STOP-08: Native所有／Flush完了 | [Tmr](TmrCamera.md) | `wse.tmr.camera_owner_thread_contract`、`wse.tmr.media_foundation_read_state_contract`、Opt-in Camera Smoke | 本番配送／Flush状態、失敗／Timeout、再開・Owner寿命。Native呼出し期限や物理抜去の認証ではない |
| TMR-FRAME-04: 所有Frame byte | [Tmr](TmrCamera.md) | `wse.tmr.camera_contract` | Padding最小量とNV12偶数Extent。復号／画質認証ではない |
| Frame処理 | [Tmr](TmrCamera.md)、[Core](CoreDataModel.md) | `wse.tmr.camera_frame_ops` | 検証入力の数値結果でありCalibration精度ではない |
| Windows Packed取得とError | [Tmr](TmrCamera.md) | `wse.tmr.directshow_format_contract`、`wse.tmr.media_foundation_error_contract` | 人工入力によるInterval／ReadbackとError判断。DriverのRateや実抜去の認証ではない |
| Displayの空間的RGB | [Hardware Validation](HardwareValidation.md) | `wse.oui.frame_contract`、Opt-in `wse.oui.d3d12_display_mode_restoration` | 36画素、RGB許容差2、Alpha除外。黒／Channel交換／反転の拒否をContractで確認し、実機結果は別記録 |
| OUI-OWNER-01 / OUI-RESOURCE-02: 資源識別と寿命 | [OUI](OuiRenderer.md) | `wse.oui.renderer_identity`、`wse.oui.renderer_generation`、Lifetime／所有Source検査 | 別寿命拒否、現行資源保持、Move移譲、全設定項目、並行世代予約と枯渇を検証。偽造Handleや全Native確保失敗の証明ではない |
| OUI-SUBMIT-03 / OUI-SURFACE-04: 送信と表示 | [OUI](OuiRenderer.md) | Dynamic-mesh Golden、Window／Resize gate、Source照合 | Submit／Wait失敗時の解放と物理Display modeには別の根拠が必要 |
| OUI-PROJ-01/02/03: 座標、Pixel、Pass順序 | [Projection](OuiProjection.md) | Geometry／Golden、Binding Scale 2、`wse.oui.projection_cleanup_contract` | 解放故障117条件。Native driver故障や任意のGPU停止は対象外 |
| BIND-STRUCT-01～BIND-DEVICE-04 | [Language Bindings](LanguageBindings.md) | `wse.binding.*`、言語Lifecycle／Stress | 有効言語のみ。Queue済みCallback、Managed例外、並行Closeの限界はAdapter照合が必要 |
| BIND-TRANSFER-05: Transport失敗時の値 | [Binding](LanguageBindings.md)、[XPT](XptTransport.md) | `wse.binding.transfer_failure_contract`、Python／Node XPT、Java契約 | Native人工Countと実UDP切詰め。全HeapやSerial実機の認証ではない |
| CABI-OWNER-01～CABI-CALL-05 | [C ABI](CAbiContract.md) | `wse.capi.abi_snapshot`、`wse.capi.c_layout_snapshot`、`wse.capi.allocation_contract`、`wse.capi.runtime_contract`、`wse.binding.dotnet_contract` | 16 Struct／87 Field、Core確保点注入、Lease解放順序、64回のDispose競合。全Device巻戻しは範囲外 |
| CABI-ADOPT-06 / BIND-DEVICE-04 | [C ABI](CAbiContract.md)、[Bindings](LanguageBindings.md) | `wse.binding.dotnet_ownership_contract`、`wse.capi.camera_delivery_contract` | Managed巻戻し53注入、Native worker Callback 8、Native配信5ケース。人工ProviderでありCLR Heap枯渇や実Cameraではない |
| CABI-TRANSFER-07 | [C ABI](CAbiContract.md)、[XPT](XptTransport.md) | `wse.capi.transfer_contract`、`wse.capi.udp_progress_contract`、`wse.binding.dotnet_transfer_contract`、`wse.binding.dotnet_contract` | Native変換27・Managed25ケース、実UDP／TCP Loopback。非零の送信失敗進捗は人工検証。Serial実機／Heap枯渇ではない |
| PKG-GRAPH-01～PKG-BINDLOAD-06 | [Build Packaging](BuildPackaging.md) | [Package検証](../../../test/package/verify_public_package.py)、[外部Consumer](../../../test/consumer/CMakeLists.txt)、`wse.binding.installed_libcamera` | 選択Install成果物で実行。ManifestのHEAD Tree識別情報に未Commit Byteは含まれない。JNI／C ABIのLoadだけではManaged操作を検証しない |
| WALK-IMAGE-01～WALK-MEASURE-05 | [開発者向け実習](DeveloperWalkthrough.md) | `wse.oui.backend_projection_golden`、`wse.tmr.camera_backend_contract`、`wse.tmr.camera_frame_ops`、Opt-in `wse.bench.indexing` | 手順付きExampleと負荷見積り。初見受入、Peak Memory／FPS保証、実機証拠ではない |

</div>
</div>

この対応表は記載した要求を対象とし、全要求を網羅する主張ではありません。新しい契約には安定IDまたは一意のSectionを設け、
公開Symbolと検証Assertionを対応付けます。Test不足・未確定動作は明示します。

## 再現の受入

限定した変更について、実装を読んでいない開発者が次を行います。

1. 設計からData表現・Owner図・正常呼出し順序を導く。
2. 小さな入力を期待値と数値許容差の範囲で再現する。
3. 必要に応じて不正入力・部分失敗・Cancellation・Shutdownを説明する。
4. 最小の独立Reader、純粋変換、Fake Backendを作る。
5. 仕様のVectorと関連する契約Testを比較する。

観測可能な動作を推測しなければ答えられない質問は文書不足です。独立実装が内部Class名や性能上の選択まで
複製する必要はありません。このChecklistの追加だけで受入完了とはしません。

GEFの限定演習は[独立参照Codec](../../../test/reconstruction/gef_reference.py)と公開API Bridgeに実装し、
`wse.gef.reconstruction`へ登録しています。WSE自身の往復を超え、BIN／CSVの双方向互換を確認します。
作成者はWSE Sourceを確認しているため、初見開発者の受入は別であり、Gate成功をそのReview完了とは報告しません。
Coreでは行添字とPadding付きRGB8 Copy、
続いてCoreAlgorithmsの共分散・矩形回転・非均一Bayer値を再現します。Libraryの契約Testを実行することと
独立再実装は異なり、数値・Camera・Rendererの再現は別の受入作業です。
TmrではTimeout／Error／Callback例外の状態表とPadding付きFrame sizeを再現します。OUIでは2 CellのIndex列、
合成例、Scale 2の量子化を再現し、各解放失敗を説明してからAdapterを実装します。

### 実施順序と受入記録

HTML表示確認は詳細設計変更と同じ作業単位で、日英編集・文書自動検証の後に実施します。
Developer Publicの完全HTMLを両言語で生成し、BrowserでNavigation、Code／Text図、長い表、数値例を
確認します。`--check`成功や警告なし生成だけでは表示受入としません。

限定した設計整備の完了前に、Ownerがその範囲のGateを明示的に免除した場合を除き、初見開発者の再現を行います。
許可する設計書・公開Header・
入力Vector・準備済みPackageをHash付きで固定します。対象実装を読んでいない担当者へ、実装・Test・解答を
渡さず課題と観測可能な判定条件を渡します。担当者、既知知識、環境、成果物、質問、推測、結果、未解決事項を
記録します。著者の自己ReviewとAI演習は別の証拠で、人間の初見受入の代わりにはしません。
担当未割当は明示免除がなければ保留であり、合格ではありません。Ownerの免除は日付・範囲・理由・残す証拠・
未検証範囲を受入記録へ記載し、「Owner判断による省略」とします。AIの結果を人間の合格へ読み替えません。
免除は文書／Example Gate、未解決不具合、別途許可する実機Gateまで免除するものではありません。

対象は前述のGEF／Core／Projection、C ABIの2段階取得・TLS・所有Client、Install済みCore Consumerです。
観測動作の不足が判明した質問は対象範囲の受入を止めます。日英修正、HTML再生成・再確認、影響課題の再試験を行い、
解答を開示した場合は未読担当者で再確認します。物理Deviceは別途許可されたGateのままです。

## 文書・Exampleの自動検証

`wse.documentation.contract`は対訳Metadata、必須文書、Local File Link、Option／Preset、5言語Quickstartの
存在を検査します。翻訳意味の一致、Anchorの有効性、数値結果、文章の無矛盾までは証明しません。
`wse.documentation.quickstart_*`は各Runtime有効時にExampleをBuild／実行します。
設計変更は関連契約Testと`git diff --check`も必要です。

実行方法は既存[Build Guide](../../ja/BuildGuide.md)と[Doxygen Guide](../../../doxy/README.md)に従います。
ConfigureはOffline、前提Toolの準備は別操作です。PortableなTest／Code・文書Commandの変更はWindows／Linux、
Header・所有・Package・Platform変更はSHARED／STATICで検証します。実機はOpt-inのままで、
SkipをHardware成功へ変えてはいけません。

## 保守

英語設計、日本語版、Manifest、索引、変更履歴を同時に更新します。Result・所有・実機の共通規則は詳細の
正本を1つにしてComponentから参照し、例外を明示します。過去のTest実行件数を規範節へ重複記載しません。
執筆構成は[Documentation Generation](DocumentationGeneration.md#ja-detailed-design-authoring)に従います。

詳細設計を保守するときは、次の変更影響を確認します。

| 変更する契約 | 同時に照合する対象 | 関連する根拠 |
| --- | --- | --- |
| Layout・形式・演算 | 公開宣言、Core／Component設計、固定Vector、言語変換 | 配置／数値／形式契約、許容差と不正入力 |
| 所有・Callback・Shutdown | Component状態表、Thread Ownership、Managed OwnerとError境界 | Lifecycle／並行性契約、未試験の失敗経路の明示 |
| 公開ABI・Package利用 | C ABI／Language Bindings、Build Packaging、利用Guide、Install済みConsumer | ABI Snapshot／Runtime、SHARED／STATIC、Package／Consumer Gate |
| Example・導線・表示 | 日英、Manifest、索引、生成HTML | 文書／Quick-start Gate、Browser表示確認 |

動作変更を別作業へ送る場合は、現行の限界、対象契約、次の対応、必要な将来の検証を追跡記録に残します。
文書整備完了は、すべての実装上の限界を修正した証明ではありません。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
