// ① 定義部はファイル先頭にまとめる
const I18N = {
  "Main Page":     { en:"Main Page",    ja:"メインページ" },
  "Namespaces":    { en:"Namespaces",   ja:"名前空間"     },
  "Classes":       { en:"Classes",      ja:"クラス"       },
  "Files":         { en:"Files",        ja:"ファイル"     },
  // …必要な固定文言を追加…
};
const CUSTOM_TABS = {
  "md_doc_2_h_o_w_t_o.html": { en:"How to", ja:"使い方" },
  "md_doc_2_s_e_t_u_p.html":  { en:"Setup",  ja:"環境設定" },
  "md_doc_2_l_i_c_e_n_s_e.html":{en:"License",ja:"ライセンス"}
};

// 各タブのIDまたはURLをキーに使って、言語ごとのパスを定義
const TAB_URLS = {
  // ここでキーは「現在 layout.xml で設定している href の共通部分」
  // たとえば layout.xml に url="md_docs_OtherTab.html" としているなら "OtherTab"
  "OtherTab": {
    ja: "md_docs_OtherTab_ja.html",
    en: "md_docs_OtherTab_en.html"
  },
  "Setup": {
    ja: "md_docs_Setup_ja.html",
    en: "md_docs_Setup_en.html"
  },
  // …同様にすべてのカスタムタブを列挙…
};

// ② applyLang 関数を正しく閉じる
function applyLang(lang) {
  Object.keys(I18N).forEach(key => {
    document.querySelectorAll(".tabs a, .navpath a").forEach(a => {
      if (a.textContent.trim() === key) {
        a.textContent = I18N[key][lang];
      }
    });
  });
  Object.keys(CUSTOM_TABS).forEach(url => {
    document.querySelectorAll(`.tabs a[href="${url}"]`).forEach(a => {
      a.textContent = CUSTOM_TABS[url][lang];
    });
  });

  // さらに URL の切り替えを行う
  Object.entries(TAB_URLS).forEach(([key, urlMap]) => {
    // タイトルではなく href にマッチさせたい場合は
    // 元の href の一部 (たとえば "_OtherTab.html") を使って選択しても OK
    document.querySelectorAll(`.tabs a[href*="${key}"]`).forEach(a => {
      a.href = urlMap[lang];
    });
  });

  // 保存もお忘れなく
  localStorage.setItem("doxy_ui_lang", lang);

}  // ←ここで必ず閉じる

// ③ ページロード時・セレクト変更時に applyLang を実行
window.addEventListener('DOMContentLoaded', () => {
  const select = document.getElementById("langsel");
  if (!select) return;

  // 前回の選択を復元（デフォルト ja）
  const saved = localStorage.getItem("doxy_ui_lang") || "ja";
  select.value = saved;
  applyLang(saved);

  // 言語切替 UI にもバインド
  select.addEventListener("change", () => {
    applyLang(select.value);
  });
});