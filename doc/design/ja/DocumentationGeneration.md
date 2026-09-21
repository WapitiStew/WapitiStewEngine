# WSE 文書生成設計

> Canonical source: [English Documentation Generation](../en/DocumentationGeneration.md)
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## 対象者と内容

文書の対象者と公開範囲は独立に選択します。各組合せのVersion 1 JSON Profileには`audience`、`visibility`、
`source_root`を記載し、共通Python Runnerが日英のDoxygen設定へ解決します。`source_root`はProfile Fileからの
相対Pathです。共通の書式はEngineの`doxy/Resource`と`doxy/Style`で管理します。

| Profile | 解析するSource | その他のページ | 描画するSource表示／Private Member |
| --- | --- | --- | --- |
| User Public | Engineの`api/` Header | API入口ページ | 無効 |
| Developer Public | Engineの`api/`、`core/`、`platform/` | 利用Guideと設計文書 | 有効 |
| User Confidential | Engineと選択したOverlayの`api/` Header | API入口ページ | 無効 |
| Developer Confidential | EngineとOverlayの`api/`、`core/`、`platform/` | 両Treeの利用Guideと設計文書 | 有効 |

User版のHTML／LaTeXはSource browser、Inline Source、Header全文、Private MemberとInternal文書を除外します。
DoxygenのXMLには描画対象外のPrivate Symbol情報も残るため、通常のUser版ではXMLを生成しません。
User版の`--check`だけがProgram listingなしの検証用XMLを生成します。このXMLはUser向けSiteではありません。
User版の通常生成前には、過去の実行でその言語の出力先に残ったXMLを削除します。
`--generate-only`は既存の出力を保持します。Header内のInline実装は元のHeaderに存在しますが、Source一覧として
描画しません。Developer版はこれらを有効化します。Test、Vendor Dependency、Build Directoryおよび過去Reportは
解析しません。Overlayの存在や環境変数だけで公開ProfileへOverlayが追加されることはありません。選択Directoryから
入力Fileを列挙し、Include探索やSymbolic link経由でSource範囲が拡張されないようにします。

## 入口と出力

WindowsとShellの起動Fileは[Doxygen Guide](../../../doxy/README.md)を参照してください。共通Runnerは
`--preset user-public`、`--preset developer-public`または明示Profileの`--profile <JSON>`を受け取ります。
既定はDeveloper Publicです。`--language en|ja|both`で言語を選び、既定は両言語です。`--generate-only`は
Doxygenを起動せず確認可能なDoxyfileを保存します。`--check`はHTML・LaTeX・図を生成せずXMLで検査し、
通常生成したSiteの出力を上書きしません。

出力Rootは`<source-root>/doxy/generated/<audience>-<visibility>/<full-or-check>/`です。
各`lang_en`／`lang_ja`がHTML／XML／LaTeX、`warnings.log`および`doxygen.stdout.log`を保持し、
解決済みDoxyfile・Layout・HTML Headerは`config/`へ保存します。言語切替は同じ版で選択した言語だけを参照します。
Titleには対象者と公開範囲を表示します。同一Profile・ModeのCommandは同時実行しないでください。

入力と表示Resourceの設定Pathは絶対Pathです。Doxygenは選択した言語の出力Directoryで実行し、
入れ子のCheckoutでも親ProjectのFileが相対Markdown Linkより優先されないようにします。

Confidential Profileは公開Source tree外のOverlayに置きます。共通Runnerや公開APIを再利用しても、生成物・
設定・Process Logはすべて公開Treeの外に保持します。Overlayがない場合や設定が不正な場合は失敗し、公開版へ
自動で切り替えません。Public Profileが選択できるのはEngine Rootだけです。生成物はGit管理対象外とし、
生成成功を公開やAccess-controlled文書のExport許可とみなしません。

公開SDKのCheckoutはOverlayの`external/`配下にSubmoduleとして配置できます。SDKはOverlayが解析する
`api/`、`core/`、`platform/`、`doc/`の入力Treeと、すべてのControlled出力Directoryの外に置きます。
Public ProfileはSDKだけを選択し、Controlled Profileは明示した2組のSourceを選択します。
Controlled生成物はすべてSDKの外に保持します。

## 検証と出力設定

Doxygen 1.13.2以降を要求し、通常生成にはGraphvizも必要です。DoxygenとGraphvizの準備は明示的な別Stepです。
Windowsのbatは固定版PythonのBootstrap起動処理を再利用し、未準備の検証済みCacheを取得する場合があります。
OfflineではCacheを事前準備するか、既存の処理系からPython／Shell入口を使用します。
Process成功と文書警告Logが空であることの両方を必要とします。回帰Testは4条件の入力、出力分離、
制御対象Sourceの誤選択拒否、User版のSource非表示、言語選択および失敗伝播を検査します。
公開CIはUser／Developerの日英を生成します。Overlay側でもConfidentialの両Profileを検証し、
LogとEvidenceはOverlay内に保持します。

共通入口の既定はDeveloper Publicです。出力先は上記の版別Directoryです。
選択した版の`html/index.html`を開きます。`doxy/project.json` Version 2は生成Optionを重複保持せず、公開Profileの一覧を管理します。

<a id="ja-detailed-design-authoring"></a>
## 詳細設計の執筆

Developer文書は生成Symbol参照と、責務・状態・Algorithmの説明を組み合わせます。Source表示だけでは
Systemの再現方法は定義できません。[Architecture](Architecture.md)から読み始め、
各詳細契約を[Design Verification](DesignVerification.md)へ結び付けます。

新設・大幅拡張するComponent設計は以下を扱います。非該当は理由を示し、未決定の内容を推測で埋めません。

| 項目 | 必要な内容 |
| --- | --- |
| 範囲と責務 | 利用場面、境界、呼出側責任、対象外 |
| 構造と依存 | Facade・Owner・Adapter・Helper図、Source対応、論理依存とLinkの区別 |
| Data表現 | 単位、Default、Byte／要素配置、座標、不変条件、借用／所有 |
| 操作契約 | 前提、成功、失敗、部分出力、状態保持 |
| 状態と順序 | 正常・不正遷移、失敗、Cancellation、再開、Shutdown |
| 並行性と寿命 | 実行Thread、Lock、Callback再入・解除、Join、資源解放 |
| Algorithm・File形式 | 数式・擬似Code、丸め、制限、代表入力と期待値 |
| Platform／Runtime差 | Backend選択、未対応能力、Marshaling、ABI |
| 根拠と限界 | 契約ID／Section、公開Symbol、Test、条件、未検証事項 |
| 設計理由 | 現在の構造の選定理由、拡張境界、互換性の制約 |
| 変更履歴 | 変更内容・理由・日付を設計本文とは分けて記録 |

観測可能な要求と現在の内部選択を区別します。発見した不具合を望ましい仕様へ変えません。
設計本文にはSDKの現在の構造・動作・対応API・制約を現在形で記載します。
移行の経緯、変更前後の比較、実装を追加・変更・修正・削除したという記録は変更履歴だけに置きます。
現在利用できる互換APIや非推奨規則は現行契約として説明し、完了した作業の報告にしません。
実行報告・作業計画は規範本文の外に保持し、英語・日本語・Manifest日付・変更履歴を同時に更新します。

責務・所有・状態・Sequenceごとに小さな図を作り、矢印の意味を示します。現Markdown／Doxygen経路では
Fenced Text図とMarkdown表を共通の基準とします。別の図Rendererは生成後の日英表示を検証してから採用します。
ExampleはSourceへLinkして重複Copyの乖離を避け、新設文書をManifestと索引へ登録します。

詳細設計の各作業単位で、同期後にDeveloper Public HTMLを日英生成してBrowser上の実表示を確認します。
入口Navigation、文書間Link、所有図、Code、広い表、数式を対象に、Profile／Tool版、警告結果、確認Pageと
Screenshotを受入記録へ残します。修正後は影響Pageを再確認します。初見再現はこの表示Gateの後に実施し、
[設計と検証](DesignVerification.md)に従います。

共通Styleの変更では公開User／Developer両版の日英について、全最上位HTML Pageを320／375／720 CSS pxで
検査します。API詳細、全Member一覧、Navigation、Sourceを含め、検索用FragmentとCrawler Fileを除きます。
読込み成功、本文と画像の存在、文書全体の横方向はみ出しがないことを確認します。広い宣言・表・Code・図は
個別Container内でScrollできるものとし、Codeの空白と図のImage Map座標を維持します。代表Screenshotの目視は
自動寸法検査と分けて記録します。生成完了後に検査を始め、PageやStyleの読込みが不完全な測定は無効とします。

Developer完全生成後、Runnerは選択済みDoxygen XMLのFile対応から相対Source Linkを解決します。
解析済みFileは生成Source／参照Pageへ接続し、入力対象外のBuild Script・Test・言語SourceはCheckout内の
Pathとして表示します。この処理でSource入力を追加・Copy・公開することはありません。User版には適用しません。

同じ選択済みXML対応で、DoxygenがMarkdown URLのまま残した文書Linkを生成Pageへ解決し、明示された節Anchorを
保持します。参照先の見出しには安定したHTML Anchorを付け、Rendererが自動生成する見出しIDへ依存しません。
未解決のMarkdown URLを含む全種類のLocal File Linkを調べ、Fragmentが生成先Pageに存在することを確認します。

## 変更履歴


| 日付 | 変更内容 |
| --- | --- |
| 2026-09-20 | 公開初版の仕様と手順を記載。 |
