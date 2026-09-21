# WSE OUI Renderer設計

> Canonical source: [English OUI Renderer Design](../en/OuiRenderer.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 目的と範囲

本書は`wse::oui`が所有するRenderer／Display境界の正式設計書である。Native GPU handleを公開しない
Backend非依存`Renderer` facadeがWindows Direct3D 12とLinux Vulkan 1.2を接続する。Offscreen、Window、
Direct Display、Display／Mode列挙、Resize、Fullscreen、Hotplugおよび終了時復元を同じPortable Contractで
扱い、Texture upload、Indexed mesh描画およびReadbackを提供する。Rendererは`Renderer`だけであり、
いずれのAPIもNative Display／DXGI handleを露出しない。

## 現在のSupport matrix

| Target | Renderer状態 | Surface状態 | Library形式 |
| --- | --- | --- | --- |
| Windows x86-64／MSVC | D3D12 clear、upload、Indexed textured mesh、readbackおよびWARP Goldenを検証済み | Offscreen、Win32 Window、対話Resize、Borderless／Display-mode Fullscreen、Hotplug、Direct Display、復元を実装。実機結果は記録した機材とModeに限定する | Shared、Static |
| Linux x86-64／GCC | Vulkan 1.2 Offscreen／Projection GoldenをWSL2 Mesa llvmpipeで検証済み | WSLg Wayland Window／Fullscreen／Resize／Presentを検証済み。DRM/KMS実装済み、実Displayは未検証 | Shared、Static |
| Linux ARM64／Raspberry Pi | Pi 4 Native Vulkan Offscreenの根拠を受理済み | 物理Presentationは未実施。下記の限定された実機記録を参照 | Shared、Static |

Pi 4ではShared／Static Native buildとGPUを使用したOffscreen Vulkan Projectionの実行結果を受理済みである。
HDMI、Window presentationおよびDRM/KMSのGateは`HARDWARE_NOT_RUN`のままで、Pi 5も`HARDWARE_NOT_RUN`である。
受理範囲は[Hardware Validation](HardwareValidation.md)を参照する。Windows WARP結果をPhysical GPU、Linuxまたは
Raspberry PiのRenderer認証として扱ってはいけない。

## Packageと公開Header境界

- `WSE::Oui`はOptionalであり、`WSE::Core`へ依存する。
- Windows／LinuxではOUI-only Shared／Static Install packageを提供する。
- `Renderer`はMove-only PIMPL facadeであり、公開HeaderにWindows、DXGI、D3D12、WRL、COM型または
  `platform/oui/win`実装Headerを露出しない。
- Texture、Mesh、SurfaceおよびFenceは世代番号付きOpaque handleで表し、Native handleとして利用できない。
- 1つの`Renderer` instanceは呼出Thread側で直列化する。現時点ではThread-safeを保証しない。
- 現行公開APIは型付きOpaque handleを使用し、Native `void*` GPU handleを公開しない。

## Lifecycle契約

- Default constructor直後は未初期化である。
- 再初期化は要求された5項目すべて、Backend、Software adapter、Prefer display adapter、Validation、
  `adapter_name`の文字列完全一致で比較する。同一要求は冪等で成功し、異なる有効要求は
  `AlreadyInitialized`でBackendと資源を保持する。Adapter名の正規化やAutomatic／明示Backendの同一視はしない。
  不正引数の検査が先行する。選択変更の前にShutdownする。Facadeは確保を伴う設定Copyを
  Backend初期化前に済ませ、成功後に保持ConfigurationへMoveする。
- `shutdown()`は未初期化時も安全かつ冪等であり、Resourceを破棄する。終了後は再初期化できる。
- Copyは禁止する。MoveはBackend所有権を移譲し、Move元は安全な未初期化状態となる。
- 引数が有効な未初期化Operationは`NotInitialized`を返す。Facadeの引数検査が先行する場合があり、
  空Handleなら先に`InvalidArgument`となり得る。
- `Automatic`はWindowsでD3D12、LinuxでVulkan 1.2を選択する。Windowsでの`Vulkan12`要求は
  `UnsupportedBackend`を返し、暗黙にD3D12へFallbackしない。

`Renderer`はCopyできない。Initialize、使用、Shutdownの順で扱い、`shutdown()`は冪等である。
Shutdown後の`Renderer`は再Initializeできる。

### 実装と所有者: OUI-OWNER-01

| 層 | Source | 責務 |
| --- | --- | --- |
| Facade | [Renderer.cpp](../../../core/oui/renderer/Renderer.cpp) | Unique Impl／Backend、引数／Lifecycle検査、Platform選択と委譲 |
| 値 | [RendererTypes.cpp](../../../core/oui/renderer/RendererTypes.cpp) | 純粋なDescription検査とFrame size計算。GPUを所有しない |
| 内部境界 | [RendererBackend.h](../../../core/oui/renderer/RendererBackend.h) | Backend interface。Installする拡張Interfaceではない |
| Windows | [D3D12RendererBackend.cpp](../../../platform/oui/win/renderer/D3D12RendererBackend.cpp) | COM owner、Resource map、Queue／Fence、未完了Submission、Win32 surface |
| Linux | [VulkanRendererBackend.cpp](../../../platform/oui/linux/renderer/VulkanRendererBackend.cpp) | Vulkan確保、Resource map、Queue／Command、Wayland／DRM surface adapter |
| Linux送信Owner | [VulkanSubmission.h](../../../platform/oui/linux/renderer/VulkanSubmission.h) | Native呼出し注入境界とCommand／Fence・一時資源のRAII owner。非公開・Install対象外 |
| Frame変換 | [RendererFrameOps.cpp](../../../core/oui/renderer/RendererFrameOps.cpp) | 所有Core imageへの変換。Device寿命は持たない |

```text
Renderer unique Impl -- owns --> backend
                                  +-- owns --> texture / mesh maps
                                  +-- owns --> surface map -- owns --> surface textures
                                  +-- owns --> queue / completion tracking
                                                 +-- retains --> submitted resources
caller opaque handle -- non-owning lookup --> resource / surface maps
```

Moveはこの所有Graphを移譲する。Move元RendererはInitializeで新しいImplを作成でき、Move代入先の旧Backendは
解放される。同一Instanceの操作は利用側で直列化する。Windows objectはComPtrが所有し、VulkanのNative確保は
Adapter内に留める。WindowsのOwnership scanは資源解放手段としての手動COM Release／Detachを拒否する。

### Handle識別と寿命: OUI-RESOURCE-02

Handleの`valid()`はValueとGenerationが非零かだけを確認し、Resourceの存在やRendererへの所属を証明しない。
1回のBackend寿命内ではResource mapとGenerationを照合し、破棄済み／未知の識別子には`ResourceNotFound`を返す。
IDはResource生成に従って増加する。Surface所有Textureの直接破棄は`ResourceInUse`で拒否する。

Backend初期化ごとに内部の[RendererIdentity.h](../../../core/oui/renderer/RendererIdentity.h)から
非零32-bitの寿命Generationを予約する。読み込まれた1つのWSE Runtime内のRenderer間で共有し、Shutdownでも
予約元を初期化し直さない。Relaxed AtomicのCompare／Exchangeは番号の一意性だけを保証し、Renderer状態の公開には使用しない。
初期化失敗でも世代を消費し得る。`UINT32_MAX`回予約した後は枯渇を保持し、古い値へ周回する代わりに
Native確保前に`Resource/ResourceExhausted`で初期化を拒否する。

Texture、Mesh、Surface、そのAttachment、Fenceの全HandleがこのGenerationを持ち、Backend照会は
Local Valueを受理する前に世代を照合する。別Rendererまたは以前の初期化の `{value=1, generation=A}` が
現行の `{value=1, generation=B}` を指すことはない。初期化済みで操作可能なBackendでは別寿命のHandleを
`ResourceNotFound`で拒否する。引数検査、Lifecycle、未完了SubmissionのError優先順位に従う。
拒否した識別子は対応する現行資源を変更しない。Surface Textureも所有Surfaceと同じ世代を保持する。

Move構築／代入は資源とともにBackend世代を移譲し、Move元のHandleはMove先で使用できる。
Move代入先の旧資源は終了し、そのHandleを拒否する。Move元を再初期化すると新しい世代を予約する。
これらはOpaque識別子であり、偽造不能なSecurity Tokenではない。Fieldの合成、永続保存、別々に読み込んだ
WSE Copy間での共有、Runtime Unload後の使用は禁止する。誤使用を検出できてもShutdown時にはHandleを破棄する。
既存の64-bit Resource／Fence Counter枯渇経路は、32-bit Generation境界Testの認証対象ではない。

ShutdownはSurface／Swapchain、Resource map、Pipeline／Descriptor、Native Device／Window objectをAdapterの
依存順に解放する。D3D12は内部Timeout付きSignal／Waitを行うが、Shutdownは待機失敗を報告しない。
Vulkanは`vkDeviceWaitIdle`が成功またはDeviceLostを返すまで未完了資源を保持する。それ以外のErrorは
資源を解放せず1 ms休止して再試行する。Native待機には有限Timeout引数がなく、継続的なErrorでは
Shutdownが戻らない場合がある。voidのShutdownはGPU完了認証でも共通の停止時間上限でもない。
繰返しShutdownは安全である。

## Data／Resource契約

### Frame

- `sRendererFrameDescription`はExtent、Portable Pixel formatおよびByte単位のRow pitchを持つ。
- Row pitch 0は最小Packed pitchを選択する。明示Pitchは最小値以上でなければならない。
- 無効Format、空Extent、Pitch不足またはSize overflowはValidation Errorとなる。
- Frame byte数は最終行Paddingを含む`effectiveRowPitch() * height`と完全一致する必要がある。
  R8は1 Byte／Pixel、RGBA8／BGRA8は4、RGBA16は8。BGRA8の3 x 2、Pitch 16には正確に32 Byteが必要で、
  Cameraの最小量条件と異なり31も33も失敗する。有効な格納形式でも各BackendのRender target対応を意味しない。
- `readTexture()`はBackend paddingを除去したPacked CPU frameを返す。

<a id="ja-frame-and-core-image"></a>
### FrameとCoreのImage

`RendererFrameOps.h`がFrameとCoreの`wse::Image_`を相互に変換し、`Renderer`も`uploadTexture()`と
`readTexture()`に同じ変換を持つため、利用側はImage型のまま作業できます。宛先の型がFormatを指名し、
これがChannel順序を境界の向こうへ運ぶ仕組みです。

| Renderer Format | Core Format | Image型 |
| --- | --- | --- |
| `R8Unorm` | `CH1D8` | `img1c08_t` |
| `Rgba8Unorm` | `CH4D8` | `img4c08_t` |
| `Bgra8Unorm` | `BGRA4D8` | `img4c08_bgra_t` |
| `Rgba16Unorm` | `CH4D16` | `img4c16_t` |
| `Rgba16Float` | なし | なし |

ここのColor Formatはすべて4 Channelを保つため、この経路でAlphaが捨てられることはありません。
`Bgra8Unorm` FrameはBGRAのままのImageになり、黙って並べ替えられることもありません。
2つの順序の間を明示的に移る経路は`wse::convertChannelOrder()`です。

`Rgba16Float`にImage型が無いのは、Coreの`Pixel_`が整数Channelに限られ、半精度値がそれに当たらない
ためです。Bit列が別の数を表すため`Rgba16Unorm`として読み替えることはせず、
`Unsupported`／`UnsupportedFormat`を返します。そのTextureは`sRendererFrame`版で読み出します。

読み出しではRow Pitchを畳み込みます。書き出しではRow Pitchを0にし、詰まった行の選択をBackend自身の
整列規則に委ねます。16bit SampleはLittle Endianで運ばれます。変換はすべて`RendererStatus`または
`RendererResult`を返します。CoreのImage型は確保失敗を例外で報告するため、各変換はそれを捕らえて
`Resource`／`ResourceExhausted`として返し、例外はOUIの境界を越えません。

### Texture

- DescriptorはExtent、Format、Usage、初期StateおよびMip数を持つ。
- Stateは対応Usageと整合しなければならない。破棄済み／未知のHandleは`ResourceNotFound`となる。
- 現D3D12 adapterはRGBA8／BGRA8、1 mip、RenderTargetおよび／またはSampled用途のTextureを生成する。
  R8は非RenderTarget用途で生成でき、Projection Alpha mapではRed channelを使用する。
- `uploadTexture()`は`Sampled`と`TransferDestination`用途を必要とする。FrameのExtent／Formatは完全一致し、
  Byte数も検証済みRow pitchと一致しなければならない。Upload完了はFenceで表し、呼出しから戻った後は
  Source CPU dataを解放できる。
- Offscreen SurfaceのTextureはSurfaceが所有する。直接破棄は`ResourceInUse`となり、Surface破棄時に解放する。

### Mesh

- `sMeshDescription`はTriangleList／TriangleStrip、2D Position＋UV VertexおよびIndex配列を定義する。
- Vertex値は有限、IndexはVertex範囲内、Topologyごとの最小数／個数条件を満たす必要がある。
- D3D12 adapterはIndexed meshを生成し、内部Position／UV Shader、Nearest／Linear clamp sampler、
  RGBA8／BGRA8 Replace／Straight-alpha Pipelineで描画する。PositionはNormalized Device Coordinate、
  UVは正規化Texture座標とする。
- `updateMesh()`は有効な同一Opaque handleのVertex／Index／Topologyを一括更新する。Replacementを完全に
  生成してから交換し、失敗時は旧Meshを維持する。破棄済み／未知のHandleは`ResourceNotFound`となる。
- Callerが送信直後にOpaque mesh／texture handleを破棄した場合も、BackendはFence完了まで参照Resourceを保持する。
- Mesh更新前の送信も旧ResourceをFence完了まで保持し、更新後の送信は同じHandleから新Resourceを参照する。

### Render passとFence

- 現ContractはColor attachment 1個のRender passで、全域または矩形領域のLoad／Clear、Indexed textured-mesh
  Draw commandおよび最終Stateを指定する。
- Clear colorは有限かつ各要素0～1、領域はAttachment範囲内でなければならない。
- Draw commandには有効なMesh／Sampled texture handleが必要である。任意Alpha texture、Nearest／Linear sampling、
  Replace／Source-alpha blend、正規化Opacityおよび四辺Linear／Smoothstep Edge blendも指定できる。現在のColor
  attachment自身のSamplingは拒否する。
- Projection固有Semanticsは[OUI Projection設計](OuiProjection.md)を正本とする。
- Discardは未対応である。`Present`は表示可能なWindow／DirectDisplayのSwap chainが所有する現在の
  Backbufferの最終Stateに限り有効である。Offscreenと単独Textureは拒否する。
- 送信成功時はFence handleを返す。Backendは完了までCommand allocator、Command listおよび参照Resourceを保持する。
- Timeout 0はPollである。`UINT32_MAX`は「無限待機」を表さず、有限Timeout契約違反として拒否する。

### 送信順序とBackend差: OUI-SUBMIT-03

Facadeは委譲前にDescription、Handle、有限Timeout引数を検査する。Backendは識別子を解決し、Usage／Extent／
Formatを確認してからCommandを記録する。UploadはFrame／Texture記述の一致を要求し、CPU rowをStagingへ詰めて
GPUへCopyし、TextureをShader読取状態にする。Readbackには`TransferSource`が必要で、Staging／Readbackへの
CopyしてPacked CPU rowを返す。Vulkanは元のLayoutを復元するが、D3D12はCopySource状態にし、
後続操作が必要に応じて遷移する。Render passはAttachmentとDraw resourceを解決し、
Clear／Load、順序付きDraw、指定最終Stateへの遷移を行う。

| 段階 | D3D12 | 現Vulkan |
| --- | --- | --- |
| 記録 | Allocator／Listと一時Upload／Readback／Descriptor object | Command bufferとStaging確保 |
| 送信 | Command listをExecuteし、増加するFence値をSignal | Command終了、VkFence作成、Queue submit。Readbackは利用側Timeout、それ以外は内部30,000 msまで待機 |
| 成功返却 | 未完了の可能性あり | `submitAndWait`で完了を確認済み。完了記録されたFenceを返す |
| 保持 | Pending recordがCommand、Descriptor、参照ComPtr resourceを完了まで保持 | 未解決のCommand／Fence／一時資源を単一Pending RAII ownerで保持。完了までResource map変更を拒否 |
| Wait | Native queue fenceを待機／Poll | 完了Fence集合を照合 |

Portable fence契約は非同期実行を許容するが、送信のNonblockingを保証しない。
Vulkanの`readTexture(timeout_ms)`は有限の利用側値を64bit nsへ変換し
（`uint64_t(timeout_ms) * 1,000,000`）、Native Fence待機へ渡す。0は送信後に一度だけ完了をPollし、
CPU Frameを返さず`Timeout/TimedOut`になる場合がある。`UINT32_MAX`は引き続きFacadeで拒否する。
確保、Command記録、CPU Copyは待機予算の外であり、呼出全体のDeadlineではない。
その他のVulkan送信は内部30,000 ms待機を使用する。

成功送信後のHandle破棄は検索用識別子を消すが、D3D12の未完了処理に必要な資源を解放しない。Mesh更新は
置換資源を生成してからMap entryを交換し、既存のPending submissionは旧版を保持する。
Vulkanは成功した送信が既に完了している。

Vulkanの失敗時State遷移は次の通りである。

```text
owned recording -> end/create fence -> queue submit -> wait
  pre-submit failure: release local owner                 |
  queue OOM: no enqueue, release local owner              +-- success: release local owner after CPU use
  queue DeviceLost/unknown: retain owner, require shutdown +-- timeout/other error: move owner to pending
pending -> next mutating API polls fence with timeout 0
  timeout: ResourceInUse, no mutation -> later success: release once, execute the requested operation
  other wait error: retain, report native error; DeviceLost remains terminal until shutdown
shutdown -> device idle success/DeviceLost -> release pending -> surfaces/maps -> device
```

`sVulkanSubmission`はNative処理前に確保し、確保を伴わないMoveで既存`unique_ptr`スロットへ移す。
Command buffer、Fenceと、その処理のUpload／Readback Buffer・Memory、Descriptor pool／Framebuffer、
または未公開TextureのImage／View／Memoryを所有する。参照Texture、Mesh、Pipeline、取得Semaphoreは
Backend mapが保持する。未解決の間は、資源生成、Upload、Draw、Readback、破棄、Mesh更新、Surfaceの
Resize／Mode変更、Present、Event処理の**すべて**で先にPending FenceをPollする。Timeoutなら
Handle失効や資源変更なしに`Lifecycle/ResourceInUse`を返すため、その拒否された操作を後で再試行する。
Getterと以前返した完了FenceのWaitは利用可能で、Pending資源の回収は行わない。失敗した操作自体は
Fenceを返さず、遅れて完了したReadback Frameも破棄する。復帰後のReadbackは新たなCopyを実行する。

Queue送信成功時は、待機失敗やResult確保例外があっても、Portable Resultの構築前にLayout／Stateと
取得Semaphoreの消費を記録する。送信前の失敗では状態を変えない。Command終了／Fence生成失敗、
QueueSubmitのHost／Device OOMはその場で資源を解放する。未知のQueueSubmit失敗とDeviceLostでは
Shutdown／再初期化が必要である。WaitのHost／Device OOMなど非終端ErrorはOwnerを保持し、後のPoll成功で
復帰できる。これはVulkanの[QueueSubmit失敗規則](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit.html)と
[Device喪失時の寿命規則](https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#devsandqueues-lost-device)に従う。

送信失敗は原子的な巻戻しではない。D3D12のSignalはExecuteCommandLists後に失敗し得て、VulkanのWaitも
Queue送信後に失敗し得る。失敗からGPU未実行を推測せず、描画Transactionを自動再実行しない。
Result factoryの値／Error排他性は、全Adapter操作の例外／確保失敗の完全巻戻しを保証しない。
Error構築で`std::bad_alloc`が発生した場合もPending ownerは資源を保持する。

### Surface

通常のPresentation順序、OUI-SURFACE-04は次の通りである。

```text
createSurface -> processSurfaceEvents -> getSurfaceTexture
  -> executeRenderPass(final_state=Present) -> presentSurface
  -> processSurfaceEvents -> reacquire current texture -> next frame
```

BufferはSurfaceが所有する。Present後、およびExtent変更を伴うResize／Mode遷移後には現在のTextureを再取得する。
Application所有のSource textureは別に保持する。

- `Offscreen`、`Window`、`DirectDisplay`の種類をContractで区別する。
- 現D3D12 adapterはOffscreenと内部所有Win32 Window Surfaceを実装する。Window Surfaceは初期Extent、
  UTF-8 Title、Visibility／Vertical-sync設定および2または3個のFlip-discard Backbufferを持つ。Callerは
  `processSurfaceEvents()`でEventを処理し、現在のBackbufferを`Present`へ遷移後、`presentSurface()`を呼ぶ。
- `resizeSurface()`はWindowed SurfaceのWindowとSwap chainを同期Resizeする。`setSurfaceWindowMode()`は
  `enumerateDisplays()`のIDを指定してBorderless Fullscreenへ移動し、保存済みStyle、位置、可視状態および
  Render extentへWindowed復元する。`getSurfaceState()`は現在Extent、Modeおよび対象Display IDを返す。
- D3D12ではResize／Mode切替前にGPU queueを有限時間で完了させ、Extent変更時はSwap chain Bufferを再構築する。
  成功したExtent変更遷移では旧Surface texture handleを失効させるため、Callerは`getSurfaceTexture()`を
  再実行しなければならない。同じExtent／Mode／Displayの再要求は冪等であり、Backbuffer handleを変更しない。
- Borderless FullscreenはWindow Styleと配置だけを変更し、OS Display mode、Refresh rate、Rotationおよび
  Display電源を変更しない。Native遷移またはBackbuffer再構築失敗時は元状態へのRollbackを実行し、Rollback
  自体の失敗も`BackendFailure`として隠さない。
- Windowsは対話的なWindow枠Resize、CloseおよびDisplay topology変更を`pollSurfaceEvents()`で通知する。
  `DisplayModeFullscreen`は変更前Native modeを保存し、ModeをTestしてから適用する。Windowed復帰、Surface破棄、
  Renderer終了および途中失敗では保存値を復元する。`DirectDisplay`も同じ復元Transactionを所有する。
- Linux Window SurfaceはWayland／xdg-shellを使い、Configure eventへSwapchain extentを同期する。
  Borderless Fullscreen／Windowed復帰、Close、Output hotplugおよびPresentを同じSurface APIへ接続する。
- Linux Direct DisplayはDRM connector／CRTCの元状態を保存し、`VK_EXT_acquire_drm_display`と
  `VK_KHR_display`でSurfaceを作る。破棄・失敗時はVulkan Displayを解放して元CRTC／Modeへ復元する。
  DRM node／権限／必要Extensionがない場合は成功を偽装せず明示Errorまたは実機Test Skipとする。
- 外部／Native Window注入と複数Rendererからの同一Display同時所有は未対応である。
- Windows ShaderはWindows SDKの`d3dcompiler` System componentでRuntime compileする。これはStatic consumerへ
  ExportするSystem link要件であり、Vendor管理する外部Libraryではない。

### Display列挙

- `enumerateDisplays()`は全Hardware AdapterのDesktopへ接続中のActive Outputを読み取り専用Snapshotとして返す。
- `sDisplayDescription::id`はBackend-qualified Adapter IDとOS Display名から作る現在Topology内IDである。
  同一Topology内の再列挙で安定するが、EDID、Serial numberまたは再起動／Hotplugをまたぐ永続機器IDではない。
- Hotplug、Adapter再構成またはDisplay設定変更後は旧Snapshotを破棄して再列挙する。
- DescriptorはAdapter ID／名称、Display名、負値を許すDesktop位置、Desktop座標上のOutput extent、Current
  Mode、対応Mode一覧、Rotation、Primaryおよび現在Renderer Adapterとの互換性を持つ。
- Refresh rateは分子／分母で表す。分母は非0、分子0はBackendがRefresh rateを取得できないことを表す。
- Current ModeはMode一覧へ必ず含める。DXGIから完全なMode一覧を取得できない場合はCurrent Modeだけを残し、
  `modes_complete=false`とする。空一覧や暗黙の推定値を成功扱いしない。
- DisplayはID順、ModeはExtent／Refresh／Scan方式／Format順にSort／重複除去する。
- WARP RendererもSystem Hardware Outputを列挙できるが、全項目の`renderer_compatible`はfalseとなる。
- WaylandはCompositor outputを`wayland:` ID、DRM/KMSは接続済みConnectorを`drm:` IDで列挙する。
  いずれも現在Topology内IDであり、永続Hardware IDではない。
- 本APIは列挙中にMode、Fullscreen状態、Window位置またはDisplay電源状態を変更しない。

## Error契約

Portable分岐は`eRendererErrorCategory`と`eRendererErrorCode`を使用する。CategoryはValidation、Lifecycle、
Resource、Execution、Timeout、Unsupported、Backendを区別する。`native_code`とMessageは診断専用であり、
Backendをまたぐ制御分岐に使用してはいけない。未対応機能は空Renderer、Dummy frameまたは暗黙Fallbackで
成功を返してはいけない。

## 決定論的Renderer baseline

Windows基準Testは公開`Renderer` APIからD3D12 WARPを選び、10 x 10 RGBA8 Offscreen Surfaceを生成する。
黒いBorderと赤／緑／青／白の4 x 4 Quadrantを5回のRender passでclearし、Fence待機後にCPUへReadbackする。
結果は`test/golden/oui_d3d12_quadrants.ppm`とRGBが完全一致し、Alphaは全Pixel 255、期待RGBA FNV-1a値は
`0xde58c7e12a2fca45`である。

Mesh Goldenは2 x 2の赤／緑／青／白RGBA TextureをUploadし、Point samplingでIndexed Fullscreen quadを
8 x 8 Offscreen targetへ描画する。Quadrantの完全一致とRGBA FNV-1a `0x5854021a152e5ba5`を要求し、
送信直後にSource／Mesh handleを破棄してBackend保持も検証する。

Projection GoldenはLegacy meshを変換し、2 x 2 RGBA Textureを4 x 4へLinear拡大し、不均一Alpha map、
Opacity 0.75およびStraight-alpha合成を適用する。CPU参照とRGBA8 1 LSB以内で比較し、WARPのRGBA
FNV-1a完全一致値を`0xce4b43253436392e`とする。

Supersample Goldenは単Channel R8 Alpha mapを使って同じProjectionを8 x 8中間RGBA8 Targetへ描画し、
4 x 4へLinear縮小する。中間量子化を含むCPU二段参照と全Channel 1 LSB以内で比較し、WARPのRGBA
FNV-1a完全一致値を`0xa3ccfe45a48e2d20`とする。

Edge／Multi-source Goldenは8 x 4 TargetへRed右FadeとBlue左Fadeを順に描画し、Smoothstep、Opacity、
Draw順序およびRGBA8量子化を含むCPU参照と全Channel 1 LSB以内で比較する。WARPのRGBA FNV-1a
完全一致値を`0x17b97761907effd5`とする。

Dynamic Mesh Goldenは同一Handleを左半分から右半分の形状へ、先行Fenceの待機前に更新する。別々の
8 x 8 Targetへ送信し、Handle破棄後に両Fenceを待つ。旧左形状はRGBA FNV-1a
`0xbbb816f5e80169e5`、新右形状は`0xb39c749492b617e5`で完全一致する。

Hidden Window契約は16 x 12 Swap chainを作成してClear／Present／Fence待機を行い、Backbuffer進行とLifecycleを
検証する。これらはWARPだけのTestであり、Color conversion、Exclusive Fullscreen／Direct Display、実Monitorまたは
Physical GPUを認証しない。

Display列挙契約はWARP RendererからSystem Hardware Adapterを読み取り、同一Topologyの2回のSnapshot、
ID一意性、順序、Mode整合性およびWARP非互換表示を検証する。Active Display数は環境に依存し、
固定のGolden値ではない。Headless環境の0台も正常なSnapshotである。

Resize／Borderless契約は非表示WARP Windowを160 x 120から320 x 180へ変更し、旧Backbuffer失効、新Extentでの
Clear／Readback、同一要求の冪等性、無効Display IDの無変更Failureを検証する。Displayが存在するHostではPrimary
DisplayのDesktop extentへBorderless化してから320 x 180 Windowed状態へ復元し、前後のDisplay mode、位置および
Desktop extentが一致することを確認する。Headless HostではBorderless正経路だけをSkipし、Window Resizeと
Failure契約は実行する。

Backend共通Projection GoldenはD3D12 WARPとUbuntu Vulkanへ同一の2 x 2 RGBA Source、Fullscreen mesh、
Nearest samplingおよび4 x 4 Targetを渡す。両Backendは全Channelを0 LSB差で一致させ、RGBA FNV-1a
`0xe0f9517b5bf10dc5`を返さなければならない。

## 検証契約

| 契約 | Test／照合入口 | 限界 |
| --- | --- | --- |
| OUI-OWNER-01 / OUI-RESOURCE-02 | [識別](../../../test/characterization/oui_renderer_identity_contract.cpp)、[世代境界](../../../test/characterization/oui_renderer_generation_contract.cpp)、[Lifetime](../../../test/characterization/oui_renderer_lifetime_contract.cpp)、`wse.oui.resource_ownership` | D3D12／Vulkan Offscreenで別寿命のTexture／Mesh／Surface／Attachment／Fence、Move、現行画素／寸法の保持、全設定項目を確認。並行世代予約と終端枯渇も検証するが、偽造耐性や全Native確保失敗の証明ではない |
| Description／Frame検査 | [Renderer値](../../../test/characterization/oui_renderer_contract.cpp)、[Frame image](../../../test/characterization/oui_renderer_frame_image_contract.cpp) | 格納／形式Vector。Native確保失敗の網羅ではない |
| OUI-SUBMIT-03 | `wse.oui.projection_dynamic_mesh_golden`、[Vulkan送信故障注入](../../../test/characterization/oui_vulkan_submission_contract.cpp)（`wse.oui.vulkan_submission_contract`） | 旧／新Mesh画素、Native Timeout引数、送信前後失敗、Owner保持、変更拒否、復帰、確保失敗、Shutdown。DeviceLost注入は物理GPU障害の認証ではない |
| OUI-SURFACE-04 | D3D12 Window／Resize test、`wse.oui.vulkan_wayland_surface` | 環境依存Presentation。物理Display mode／DRMは別のOpt-in |

SHARED／STATIC gateを使用し、Package変更ではInstall後Consumerも実行する。Test件数は実行記録であり
規範の定数ではない。[設計検証](DesignVerification.md)はComponent横断の受入れを対応付け、
[Hardware Validation](HardwareValidation.md)をPi 4と物理Gateの正本とする。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
