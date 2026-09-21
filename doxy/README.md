# WSE Doxygen

One runner combines an audience profile with the shared Doxyfile, language settings, layout and style.
Doxygen 1.13.2+ and Python 3.8+ are required; full generation also needs Graphviz `dot` on PATH.
Install Doxygen and Graphviz separately. Windows batch entries use the existing pinned-Python
launcher, which provisions its verified runtime in the bootstrap cache when missing. For an offline
run, prepare that cache first or invoke the Python runner with an existing interpreter.

| Edition | Profile | Windows entry | Linux/macOS shell entry |
| --- | --- | --- | --- |
| User, public | `profiles/user-public.json` | `RunUserPublic.bat` | `sh doxy/RunUserPublic.sh` |
| Developer, public | `profiles/developer-public.json` | `RunDeveloperPublic.bat` | `sh doxy/RunDeveloperPublic.sh` |

Access-controlled overlays keep their own User/Developer profiles and launchers. Pass their JSON
path explicitly with `--profile`; their configuration, logs and generated documentation stay in
that overlay. Public profiles read only this engine checkout, even when an overlay is installed.

From the engine root:

```sh
python doxy/RunDoxygen.py --preset user-public
python doxy/RunDoxygen.py --preset developer-public --check
python doxy/RunDoxygen.py --profile /path/to/overlay/doxy/profiles/user-confidential.json
```

The `.bat` and `.sh` entries accept the same optional flags:

- `--language en`, `--language ja`, or `--language both` (default).
- `--check`: XML validation without HTML, LaTeX or graph rendering.
- `--generate-only`: write the resolved Doxyfiles, layout and header without running Doxygen.

`RunDoxygen.bat`, `RunDoxygen.sh`, and the Python runner default to `developer-public` when no
profile is selected. `WSE_GRAPHVIZ_BIN` may point to an existing Graphviz bin directory on Windows.

Output is `<source-root>/doxy/generated/<audience>-<visibility>/<full-or-check>/lang_<language>/`.
Open `html/index.html` after a full run. User sites contain HTML/LaTeX; their raw XML is
limited to `--check` diagnostics because it can include private symbol metadata. `warnings.log` and `doxygen.stdout.log` are kept beside
that language's output; the resolved Doxyfiles, layout and header are in the sibling `config/`.
These replace the previous `doxy/lang_en`, `doxy/lang_ja` and shared warning-log locations.
All four editions and check/full outputs are separate. Do not run the same edition/mode concurrently.
Any failed Doxygen process or nonempty document-warning log makes the runner fail.

Edit the small profile JSON files for selection, and `Resource/Doxyfile`, `Resource/layout.xml`,
`Resource/transrate.csv` and `Style/` for common presentation. Generated configuration is not edited
by hand. `project.json` is the version 2 catalog of public profiles and the runner, not a duplicate
of all generated Doxygen options. The [documentation generation design](../doc/design/en/DocumentationGeneration.md)
defines input boundaries, output isolation and verification.

For shared CSS changes, finish generation before checking every public HTML page in both editions
and languages at 320, 375 and 720 CSS pixels. Wide API tables, signatures and graphs scroll locally;
the document itself must fit. Keep automated geometry results and representative visual screenshots
as separate evidence. See the generation design for the complete inspection scope.

日本語: 共通設定とProfileを組み合わせ、User／DeveloperとPublic／Confidentialを選択します。
UserはAPI Headerと短い入口ページ、DeveloperはAPI・Core・Platformと利用／設計文書を生成対象にします。
公開版は上表の起動Fileから実行できます。Confidential版の設定・起動File・生成物はOverlay側に置きます。
各起動Fileへ`--language ja`、`--check`、`--generate-only`を渡せます。指定がない共通入口はDeveloper Publicです。
出力は`doxy/generated/<audience>-<visibility>/<full-or-check>/lang_<language>/`で、HTML入口は`html/index.html`です。
User版の通常出力はHTML／LaTeXです。内部Symbol情報を含むXMLは`--check`の検証用出力に限定します。
同じProfile・Modeを同時実行しないでください。警告Logが空でない場合は失敗します。
DoxygenとGraphvizは別途準備します。Windowsのbatは既存の固定版Python起動処理を使い、
未準備の場合は検証済みRuntimeをCacheへ取得します。OfflineではCacheを事前準備するか、既存PythonでRunnerを実行してください。
詳細は上記の文書生成設計を参照してください。

共通CSS変更時は生成完了後、公開両版・日英の全HTML Pageを320／375／720 CSS pxで検査します。
広いAPI表・宣言・図は個別にScrollし、文書全体は幅内へ収めます。自動寸法検査と代表Screenshotの目視を
別々の証跡として保持します。検査範囲の詳細は文書生成設計を参照してください。
