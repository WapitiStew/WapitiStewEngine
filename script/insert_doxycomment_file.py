#!/usr/bin/env python
"""
このスクリプトは、WonderStewEngineプロジェクト内のapi, core, platform
ディレクトリ以下にある.h, .hpp, .cppファイル先頭のDoxygenコメントを更新します。

既存のコメントブロックが存在する場合は完全に削除し、
resource/doxycomment_File.txt のテンプレートを基本としながら、
元のコメントに記載されていた以下のタグの内容（複数行の場合も）を保持して反映します：
  @file, @brief, @date, @author, @note, @copyright, @history

※ @file は常に対象ファイル名に置換され、@dateは元コメントやテンプレートが空の場合、実行時の日付を設定します。
※ @note、@copyright、@historyは元コメントの内容が存在すればそれを、その後インデント付きで出力します。

※ スクリプトはWonderStewEngine/script内に配置する前提です。
"""

import os
import re
import datetime

# 対象ファイルの拡張子
TARGET_EXTENSIONS = (".h", ".hpp", ".cpp", ".hlsl", ".hlsli")
# 対象ディレクトリ（プロジェクトルートからの相対パス）
TARGET_DIRS = ["api", "core", "platform", "resource/hlsl"]
# 保存すべきタグ：元のコメントに記載されていればその内容を反映（@fileは常に置換）
PRESERVE_TAGS = ["@brief", "@date", "@author", "@details", "@note", "@todo", "@bug", "@copyright", "@history"]
# ALIGN対象タグ（タグ行の右側の内容開始位置を固定幅に揃える対象）
ALIGN_TAGS = ["@file", "@brief", "@date", "@author", "@par"]

def parse_preserved_tags(comment_text):
    """
    元のコメントブロックから、PRESERVE_TAGSに含まれる各タグの内容を抽出します。
    行頭のコメントプレフィックス（例："//!"）と共に、タグ行以降に同形式の連続行があれば
    改行で連結して取得します。
    """
    preserved = {}
    line_pattern = re.compile(r'^\s*//[! ]+\s*(@\w+)\s*(.*)$')
    prefix_pattern = re.compile(r'^\s*//[! ]+\s*')
    current_tag = None
    for line in comment_text.splitlines():
        m = line_pattern.match(line)
        if m:
            tag = m.group(1).strip()
            content = m.group(2)
            if tag in PRESERVE_TAGS:
                current_tag = tag
                preserved[tag] = content  # 最初の行（内容が空の場合もそのまま）
            else:
                current_tag = None
        else:
            # タグ行でなく、かつコメントプレフィックスがある場合は前回のタグの継続行と判断
            if current_tag is not None and prefix_pattern.match(line):
                additional = line[prefix_pattern.match(line).end():]
                preserved[current_tag] += "\n" + additional
            else:
                current_tag = None
    return preserved

def format_tag_block(prefix, tag, new_content, use_align=False):
    """
    new_content が複数行の場合、最初の行はタグ行として（use_align=Trueなら固定幅で揃える）出力し、
    2行目以降は同じプレフィックス＋適当なインデントを付加して返します。
    new_content.splitlines() が空の場合は [""] を使います。
    """
    lines = new_content.splitlines() if new_content is not None else []
    if not lines:
        lines = [""]
    formatted_lines = []
    if use_align:
        first_line = prefix + tag.ljust(7) + "  " + lines[0]
        formatted_lines.append(first_line)
        indent = prefix + " " * (7 + 2)
        for sub in lines[1:]:
            formatted_lines.append(indent + sub)
    else:
        first_line = prefix + tag + " " + lines[0]
        formatted_lines.append(first_line)
        indent = prefix + " " * (len(tag) + 1)
        for sub in lines[1:]:
            formatted_lines.append(indent + sub)
    return "\n".join(formatted_lines)

def format_date_tag_block(prefix, tag, new_content):
    """
    @date タグ用のブロックを出力します。
    最初の行は prefix + tag のみを出力し、
    以降の各行は prefix + "  " + 内容 として、必ず2スペースのインデントを付与します。
    
    なお、各内容行について、もし日付（例: Feb-17, 2025）以降の内容が前にある場合は、
    日付とその他の内容の順序を「日付　その他」となるように入れ替えます。
    """
    # 各行の余分な空白、空行を除去
    lines = [line.strip() for line in new_content.splitlines() if line.strip() != ""]
    if not lines:
        lines = [""]
    formatted_lines = []
    formatted_lines.append(prefix + tag)  # タグ行（内容なし）
    # 日付のパターン例：3文字の月、"-", 1~2桁の日、",", 空白、4桁の年
    date_pattern = re.compile(r'^(.*?)\b([A-Za-z]{3}-\d{1,2},\s*\d{4})\b(.*)$')
    for sub in lines:
        trimmed = sub.strip()
        m = date_pattern.match(trimmed)
        if m:
            pre_text = m.group(1).strip()
            date_part = m.group(2).strip()
            post_text = m.group(3).strip()
            # もし日付より前に内容がある場合、順序を入れ替え: 日付 + 3スペース + (pre_text と post_textを連結)
            if pre_text:
                combined = pre_text
                if post_text:
                    combined += "   " + post_text
                new_line = date_part + "   " + combined
            else:
                new_line = trimmed
        else:
            new_line = trimmed
        formatted_lines.append(prefix + "  " + new_line)
    return "\n".join(formatted_lines)

def generate_new_comment(template, preserved, filename):
    """
    テンプレート（resource/doxycomment_File.txt）の各タグ行について、以下のルールで値を決定し、
    新たなDoxygenコメントブロックを生成します：
      ・@file      : 常に対象ファイル名（filename）に置換
      ・@date      : まず元コメントの値を確認し、無ければテンプレートの内容を使用；共に空なら実行時日付を設定
                     ※出力は以下の形式になるようにします。
                        //! @date
                        //!   Feb-17, 2025   Create New  WapitiStew
                        //!   Feb-18, 2025   Change
      ・@brief, @author 等（単一行タグ） : 元コメントに値があればそれを優先、無ければテンプレート値を使用
      ・@note, @copyright, @history (複数行想定タグ) :  
             元コメントに内容が存在すれば、タグ行自体は空欄として、その下にインデント付きで内容を出力
      ・ALIGN_TAGSに含まれるタグは、タグ名右側のスペースを固定幅（最低7文字＋2スペース）に整形
    テンプレート中、タグ行以外の行はそのまま出力します。
    """
    current_date = datetime.datetime.now().strftime("%b-%d, %Y")
    new_lines = []
    pattern = re.compile(r'(^\s*//[! ]+\s*)(@\w+)(\s*)(.*)$')
    multi_line_tags = {"@note", "@copyright", "@history"}
    for line in template.splitlines():
        m = pattern.search(line)
        if m:
            prefix, tag, spacing, content = m.groups()
            if tag == "@file":
                new_value = filename
                formatted = format_tag_block(prefix, tag, new_value, use_align=True)
            elif tag == "@date":
                orig = preserved.get(tag)
                if orig is not None and orig.strip() != "":
                    new_value = "\n".join(l.strip() for l in orig.splitlines() if l.strip() != "")
                elif content.strip() != "":
                    new_value = content.strip()
                else:
                    new_value = "Create New " + current_date
                formatted = format_date_tag_block(prefix, tag, new_value)
            elif tag in PRESERVE_TAGS and tag not in multi_line_tags:
                temp_value = content.rstrip()
                new_value = preserved.get(tag)
                if new_value is None or new_value.strip() == "":
                    new_value = temp_value
                formatted = format_tag_block(prefix, tag, new_value, use_align=True)
            elif tag in multi_line_tags:
                orig = preserved.get(tag)
                if orig is not None and orig.strip() != "":
                    new_value = ""
                    first_line = format_tag_block(prefix, tag, new_value, use_align=True)
                    formatted = first_line
                    extra_indent = prefix + "  "
                    for part in orig.strip().splitlines():
                        formatted += "\n" + extra_indent + part.rstrip()
                else:
                    temp_value = content.rstrip()
                    formatted = format_tag_block(prefix, tag, temp_value, use_align=True)
            else:
                formatted = line
            new_lines.append(formatted)
        else:
            new_lines.append(line)
    return "\n".join(new_lines)

def process_file(filepath, template):
    """
    対象ファイルの先頭に存在するDoxygenコメントブロック（ブロック形式またはシングルライン形式）を
    先頭から連続する「//」行全体または「/** … */」形式で抽出し、その内容から各タグを保持後、
    ファイルから削除します。続いて、resource側のテンプレートと保持情報から新規コメントブロックを生成し、
    ファイル先頭に挿入します。
    """
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
    except Exception as e:
        print(f"読み込みエラー: {filepath} ({e})")
        return

    content_lstrip = content.lstrip()
    preserved = {}
    old_comment_block = ""
    
    # ブロック形式の場合："/** ... */"
    if content_lstrip.startswith("/**"):
        m = re.search(r'/\*\*.*?\*/', content_lstrip, re.DOTALL)
        if m:
            old_comment_block = m.group()
    # シングルライン形式の場合：先頭から連続する"//"行を取得
    elif content_lstrip.startswith("//"):
        m = re.match(r'^(//.*\n)+', content)
        if m:
            old_comment_block = m.group()
    
    if old_comment_block:
        preserved = parse_preserved_tags(old_comment_block)
        idx = content.find(old_comment_block)
        if idx != -1:
            content = content[:idx] + content[idx+len(old_comment_block):]
            content = content.lstrip()

    filename = os.path.basename(filepath)
    new_comment = generate_new_comment(template, preserved, filename)
    new_comment = new_comment.rstrip("\n")
    content = content.lstrip("\n")
    new_content = new_comment + "\n\n" + content
    try:
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(new_content)
        print(f"Processed: {filepath}")
    except Exception as e:
        print(f"書き込みエラー: {filepath} ({e})")

def main():
    # スクリプトはscriptディレクトリに配置されている前提。1つ上の階層をプロジェクトルートとする。
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(script_dir, ".."))
    resource_file = os.path.join( "resource", "doxycomment_File.txt")
    try:
        with open(resource_file, "r", encoding="utf-8") as f:
            template = f.read().rstrip()
    except Exception as e:
        print(f"リソースファイルの読み込みに失敗しました: {resource_file} ({e})")
        return

    for d in TARGET_DIRS:
        target_path = os.path.join(root, d)
        for dirpath, _, filenames in os.walk(target_path):
            for fname in filenames:
                if fname.endswith(TARGET_EXTENSIONS):
                    filepath = os.path.join(dirpath, fname)
                    process_file(filepath, template)

if __name__ == "__main__":
    main()
