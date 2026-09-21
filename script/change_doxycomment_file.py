#!/usr/bin/env python
"""
このスクリプトは、指定ディレクトリ（例："api", "core", "platform"）内の .h, .hpp, .cpp
ファイル上部のDoxygenコメントブロックを抽出し、元の内容を保持したまま、固定のフォーマットに変換します。

【入力対象のコメントフォーマット】
  1. ブロック形式:
     /**
      * ここにテキストを書く
      */
  2. ブロック形式（感嘆符付き）:
     /*!
      * ここにテキストを書く
      */
  3. 連続行形式 (///):
     ///
     /// ここにテキストを書く
     /// 
  4. 連続行形式 (//!):
     //!
     //! ここにテキストを書く
     //! 

【出力する固定フォーマット】
  //*****************************************************************************************************************
  //! 
  //! 〈ヘッダー行（元のタグ行。@date は必要に応じて整形）〉
  //! 
  //! @details
  //! 〈自由記述行〉
  //! 
  //*****************************************************************************************************************

※ 元のDoxygenコメントブロックが見つかった場合、その内容は保持したまま、新規ブロックに変換されます。
※ BOM (U+FEFF) が含まれている場合にも対応し、元のコメントブロックは確実に削除されます。
"""

import os
import re

# 対象ファイルの拡張子および対象ディレクトリの設定
TARGET_EXTENSIONS = (".h", ".hpp", ".cpp", ".hlsl")
TARGET_DIRS = ["api", "core", "platform", "resource"]

# 固定フォーマット用の枠部分
BORDER_TOP = "//*****************************************************************************************************************"
BORDER_BOTTOM = "//*****************************************************************************************************************"

def extract_top_doxy_comment(content):
    stripped = content.lstrip()
    # ブロック形式: /** ... */ または /*! ... */
    if stripped.startswith("/**") or stripped.startswith("/*!"):
        m = re.search(r'^/\*[\*!](.*?)\*/', stripped, re.DOTALL)
        if m:
            block = m.group(0)
            end_index = content.find(block) + len(block)
            raw = m.group(1)
            lines = []
            for line in raw.splitlines():
                line = line.lstrip()
                if line.startswith("*"):
                    line = line[1:]
                lines.append(line.lstrip())
            extracted = "\n".join(lines).strip()
            return extracted, end_index
    # 連続行形式: ///
    if stripped.startswith("///"):
        m = re.match(r'^(///.*\n)+', stripped)
        if m:
            block = m.group(0)
            end_index = content.find(block) + len(block)
            lines = []
            for line in block.splitlines():
                line = re.sub(r'^\s*///\s*', '', line).rstrip()
                lines.append(line)
            extracted = "\n".join(lines).strip()
            return extracted, end_index
    # 連続行形式: //!
    if stripped.startswith("//!"):
        m = re.match(r'^(//!.*\n)+', stripped)
        if m:
            block = m.group(0)
            end_index = content.find(block) + len(block)
            lines = []
            for line in block.splitlines():
                line = re.sub(r'^\s*//!\s*', '', line).rstrip()
                lines.append(line)
            extracted = "\n".join(lines).strip()
            return extracted, end_index
    return None, 0

def split_tag_and_note(extracted):
    header_lines = []
    note_lines = []
    for line in extracted.splitlines():
        if line.lstrip().startswith("@"):  # タグ行
            header_lines.append(line.rstrip())
        else:
            if line.strip():
                note_lines.append(line.rstrip())
    return header_lines, note_lines

def get_author(header_lines):
    for line in header_lines:
        if line.lstrip().startswith("@author"):
            parts = line.split(None, 1)
            if len(parts) > 1:
                return parts[1].strip()
    return ""

def adjust_date_tag(header_lines):
    new_header = []
    author = get_author(header_lines)
    date_pattern = re.compile(r'^@date\s+(\d{4}[-/]\d{1,2}[-/]\d{1,2})\s*$')
    for line in header_lines:
        if line.lstrip().startswith("@date"):
            m = date_pattern.match(line.lstrip())
            if m:
                date_str = m.group(1)
                new_header.append("@date")
                new_header.append("   " + date_str + "   Create New " + author)
            else:
                new_header.append(line)
        else:
            new_header.append(line)
    return new_header

def build_new_comment(header_lines, note_lines):
    lines = []
    lines.append(BORDER_TOP)
    lines.append("//! ")
    for line in header_lines:
        lines.append("//! " + line)
    if note_lines:
        lines.append("//! ")
        lines.append("//! @details")
        for line in note_lines:
            lines.append("//! " + line)
    lines.append("//! ")
    lines.append(BORDER_BOTTOM)
    # 必ず末尾に改行を追加
    return "\n".join(lines) + "\n"

def process_file(filepath):
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
    except Exception as e:
        print(f"読み込みエラー: {filepath} ({e})")
        return

    # BOMがある場合は除去
    content = content.lstrip("\ufeff")
    extracted, end_index = extract_top_doxy_comment(content)
    if extracted is None:
        return

    header_lines, note_lines = split_tag_and_note(extracted)
    header_lines = adjust_date_tag(header_lines)
    new_comment_block = build_new_comment(header_lines, note_lines)

    # 先頭のCR/LFを削除し、コード開始行を正しく取得
    remaining_content = content[end_index:].lstrip("\r\n")

    # コメントとコードの間には build_new_comment の末尾の改行を利用
    updated_content = new_comment_block + remaining_content

    try:
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(updated_content)
        print(f"Processed: {filepath}")
    except Exception as e:
        print(f"書き込みエラー: {filepath} ({e})")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(script_dir, ".."))
    for d in TARGET_DIRS:
        target_path = os.path.join(root, d)
        for dirpath, _, filenames in os.walk(target_path):
            for fname in filenames:
                if fname.endswith(TARGET_EXTENSIONS):
                    filepath = os.path.join(dirpath, fname)
                    process_file(filepath)

if __name__ == "__main__":
    main()
