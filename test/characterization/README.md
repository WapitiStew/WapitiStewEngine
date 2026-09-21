# WSE Characterization Tests

Phase 1で既存挙動を記録し、Refactoring／Linux移植時の意図しない変更を検出するCTest群です。

- `core_characterization.cpp`: Core value type、例外および文字列表現の非実機契約。
- `core_runtime_contract.cpp`: Wait／Timeout／Timer Thread lifecycle／Structured Log契約。
- `binding_facade_contract.cpp`: Binding ABI、所有Frame copy、CancellationおよびXPT共通Error mapping契約。
- `../binding/node_api_contract.js`: Node-API Runtime情報、Promise、Buffer／TypedArray、Callback／Cancel／Close契約。
- `../binding/node_api_lifecycle_contract.js`: Workerの繰返しLoad／Unloadと未完了Timer／Promise cleanupの有限終了契約。
- `../binding/node_stress_contract.js`: 所有Buffer 2,000回、Timer／Tmr Session反復、OUI全フレームおよびRSS／External／ArrayBuffer bounded-growth契約。
- `../binding/python_contract.py`: Python Runtime情報、Context manager、所有Read-only Buffer、構造化Error、GILおよびCancel契約。
- `../binding/python_lifecycle_contract.py`: Python Process内の繰返し初期化／CloseとSubprocess load／unloadの有限終了契約。
- `../binding/python_stress_contract.py`: Runtime／所有Buffer 2,000回、Cross-thread Close、Tmr Session、OUI全フレームおよびRSS bounded-growth契約。
- `../binding/java/io/wapitistew/wse/BindingContract.java`: Java 17／21／25で共通ABI、所有Buffer、Error、Cancellation、TmrおよびOUI全フレームを検証する契約。
- `../binding/java/io/wapitistew/wse/BindingStress.java`: 子JVMの繰返しLoad／Unload、AutoCloseable owner、Timer／Close競合、Tmr／OUI反復の有限終了契約。
- `oui_frame_contract.cpp`: Projection境界で使うFrame descriptionの非GPU契約。
- `projection_geometry_contract.cpp`: ProjectionのHomography／矩形MeshとLegacy meshからPortable meshへの変換契約。
- `oui_gpu_lifetime_contract.cpp`: Windows Legacy `InterfaceGPU`のCopy／Move／Release／再Initializeと、
  `RenderTexture2D`の共有RAII owner／Move／冪等`finalize()`契約。
- `oui_renderer_contract.cpp`: Backend非依存Frame／Texture／Mesh／Render pass／Surface／Error／Lifecycle契約。
- `oui_binding_projection_contract.cpp`: C++ Binding Projectionの4x4 packed RGBA8全64 byte、FNV-1a hashおよびError mapping契約。
- `oui_d3d12_offscreen_golden.cpp`: Portable `Renderer` APIから実行するWindows D3D12 WARPのclear／transition／fence／readbackとRGBA Golden契約。
- `oui_d3d12_mesh_golden.cpp`: Texture upload、Indexed textured mesh、Point sampling、送信中Resource保持および8 x 8 RGBA Golden契約。
- `../../example/cpp/oui/portable_projection.cpp`: D3D12／Vulkan共通の公開`ProjectionPipeline` ExampleとNearest Golden契約。
- `oui_projection_scale_alpha_golden.cpp`: D3D12／Vulkan共通Legacy mesh変換、Linear scale、不均一Alpha map、Layer opacity、Straight-alpha合成およびCPU参照契約。
- `oui_projection_supersample_golden.cpp`: D3D12／Vulkan共通Legacy 2倍中間描画、Linear縮小、R8 Alpha map、内部Resource保持およびCPU二段参照契約。
- `oui_projection_edge_multisource_golden.cpp`: D3D12／Vulkan共通四辺Smoothstep Edge blend、順序付きMulti-source Straight-alpha合成およびCPU参照契約。
- `oui_projection_dynamic_mesh_golden.cpp`: D3D12／Vulkan共通の同一Opaque handle原子的Mesh更新、更新前後の描画分離、Fenceをまたぐ旧／新Resource保持および左右形状Golden契約。
- `oui_d3d12_window_surface.cpp`: 内部所有Win32 Window、Swap chain、Event処理、Present／FenceおよびBackbuffer進行契約。
- `oui_d3d12_display_enumeration.cpp`: Active Display／Modeの読み取り専用Snapshot、Topology内ID、順序、互換性および再列挙安定性契約。
- `oui_d3d12_window_resize_fullscreen.cpp`: Window／Swap chain Resize、Backbuffer失効、Borderless Fullscreen、Windowed復元、冪等性およびDisplay設定不変契約。
- `oui_vulkan_wayland_surface.cpp`: Vulkan Projection、Wayland Window／Borderless Fullscreen／Window復元／Resize、PresentおよびSwapchain handle失効契約。
- `../golden/oui_d3d12_quadrants.ppm`: D3D12 Offscreen Testが比較する10 x 10のRGB基準画像。
- `../golden/binding_projection_full_frame.properties`: C++／Python／Node.js／Javaが共有する2x2入力、4x4期待RGBAおよびFNV-1a hash。
- `xpt_serial_port_contract.cpp`: Windows／Linux共通のPortable SerialPort契約。LinuxではPTY双方向送受信、Timeout、CancellationおよびTransactional reopenも検証。
- `xpt_tcp_loopback.cpp`: Windows／Linux共通のXPT TCP loopback、Timeout、Cancellation、切断および構造化Error契約。
- `xpt_udp_loopback.cpp`: Windows／Linux共通のXPT UDP loopback、0 byte／最大Datagram、切詰め、TimeoutおよびCancellation契約。
- `xpt_retry_policy_contract.cpp`: 明示的な冪等性／Failure分類、試行上限、固定／指数Backoffおよび安全側停止契約。
- `gef_csv_contract.cpp`: CSV読込／DataMap変換／書出しround-trip契約。
- `iui_keyboard_lifecycle.cpp`: 物理入力値に依存しないKeyboard定数／Thread lifecycle smoke。
- `tmr_data_contract.cpp`: 実Camera非接続のParameter／ColorMatrix契約。
- `tmr_camera_contract.cpp`: Backend非依存Camera型、Lifecycle Error、Frame ownership／Strideおよび明示Unsupported Backend契約。
- `tmr_camera_backend_contract.cpp`: Mock backendでControl変更中のStream、停止／再開、複数Sessionおよび切断Error契約。
- `tmr_calibration_contract.cpp`: Device I/O非依存のIntrinsics、Distortion、Color matrix、Lens shadingおよびValidation契約。
- `tmr_windows_camera_smoke.cpp`: 明示的なHardware Gateで実行するWindows Cameraの共通Control照会／設定／Readback／復元、Frame、停止／再開およびCallback Smoke。Camera未接続時はSkipする。
- `../hardware/tmr_webcamera_hardware_smoke.cpp`: Linux USB Web Cameraを公開`WebCamera`だけで列挙し、Capability、読取可能Control、1 Frame、停止／Closeを検証するOpt-in Hardware Gate。Device identityは出力しない。
- `../cmake/verify_tmr_public_headers.cmake`: Tmr公開HeaderへのOS固有Camera Header／型の漏出を禁止する。
- `../cmake/verify_oui_resource_ownership.cmake`: Windows Legacy OUI Sourceの手動COM `Release()`と
  `ComPtr::Detach()`を禁止し、RAII所有へ固定する。
- `../cmake/verify_disabled_exports.cmake`: core-only Shared DLLに任意ComponentのSymbolが混入しないことを検査。

Standalone Buildでは`WSE_BUILD_TESTING=ON`が既定です。親Projectの`add_subdirectory`経由で組み込む場合は既定OFFです。
非Hardware Testの成功はCustom／機器固有Edge curve、Calibration精度、物理Display上のExclusive
Fullscreen／Direct Display／切断復元、物理Camera、LAN映像転送、物理Serial通信または
Access-controlled extensionの実機認証を意味しません。現作業環境ではC922のWindows Media Foundation
SmokeをShared／Staticで通過しています。Pi 4 USB Web CameraはLinux V4L2の列挙、Capability、読取可能Control、
1 Frame、停止／CloseをDebug／Release × Shared／Staticで通過しました。Pi 5、Pi CSI Camera、長時間運転および
抜去／再接続は未認証です。
