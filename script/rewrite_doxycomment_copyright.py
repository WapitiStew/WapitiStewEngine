#!/usr/bin/env python
"""
このスクリプトは、対象ファイル（.h, .hpp, .cpp）の先頭にある Doxygen コメントブロック内で、
@copyright タグの内容を一度削除し、resource/copywrite.txt に記載された内容に置換します。
さらに、生成される doxy コメントブロックの最後に必ず2行分の空行（コメントプレフィックスのみの行）が入るように調整します。

※ 対象ディレクトリは、プロジェクトルートからの相対パス "api", "core", "platform", "resource/hlsl" として処理します。
"""

import os
import re

# 対象ファイルの拡張子
TARGET_EXTENSIONS = (".h", ".hpp", ".cpp", ".hlsl", ".hlsli")
# 対象ディレクトリ（プロジェクトルートからの相対パス）
TARGET_DIRS = ["api", "core", "platform", "resource/hlsl"]


def update_comment_block(comment_block: str, new_copyright: str) -> str:
    """
    与えられた Doxygen コメントブロック内で、@copyright タグの行およびその継続行を削除し、
    new_copyright（改行区切り）を置換します。
    また、最後に2行の空行（コメントプレフィックスのみの行）を追加します。
    """
    lines = comment_block.splitlines()
    result = []
    i = 0
    prefix = "//!"
    # コメントブロック内を走査
    while i < len(lines):
        line = lines[i]
        if "@copyright" in line:
            # 接頭辞とインデントを抽出
            m = re.match(r'^(\s*//[! ]+)', line)
            prefix = m.group(1).rstrip() if m else "//!"
            # 新しいブロックを追加
            result.append(f"{prefix} @copyright")
            for part in new_copyright.splitlines():
                result.append(f"{prefix}   {part}")
            # copyright とその継続行をスキップ
            i += 1
            while i < len(lines) and re.match(r'^\s*//', lines[i]) and not re.search(r'@\w+', lines[i]) and not re.match(r'^\s*//\*+', lines[i]):
                i += 1
        else:
            result.append(line)
            i += 1
    # --- 終端前の空行調整 ---
    # 閉じる境界行 pattern
    boundary_idx = None
    for idx in reversed(range(len(result))):
        if re.match(r'^\s*//\*+', result[idx]):
            boundary_idx = idx
            break
    if boundary_idx is not None:
        # 直前の空行を掃除
        while boundary_idx >= 2 and re.match(r'^\s*//[! ]*$', result[boundary_idx-1]):
            result.pop(boundary_idx-1)
            boundary_idx -= 1
        # 空行2行挿入
        result.insert(boundary_idx, f"{prefix}")
        result.insert(boundary_idx, f"{prefix}")
    else:
        # 境界行がなければ末尾に空行2行
        result.append(f"{prefix}")
        result.append(f"{prefix}")
    return "\n".join(result) + "\n"


def extract_comment_block(content: str) -> str:
    """
    ファイル先頭の Doxygen コメントブロックを抽出。
    ブロック形式（/** ... */ または /*! ... */）および先頭から連続する "//" 行をサポート。
    """
    # ブロックコメント
    m = re.match(r'^(?:\ufeff)?(/\*![\s\S]*?\*/)', content)
    if m:
        return m.group(1)
    # シングルライン行コメント
    m2 = re.match(r'^(?:\ufeff)?(?://.*\n)+', content)
    return m2.group(0) if m2 else ""


def process_file(filepath: str, new_copyright: str) -> None:
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    comment_block = extract_comment_block(content)
    if not comment_block or '@copyright' not in comment_block:
        return
    updated = update_comment_block(comment_block, new_copyright)
    new_content = content.replace(comment_block, updated, 1)
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(f"Processed: {filepath}")


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, '..'))
    # リソースファイルをプロジェクトルートから読み込む
    resource_file = os.path.join(script_dir, 'resource', 'copywrite.txt')
    try:
        with open(resource_file, 'r', encoding='utf-8') as rf:
            new_copyright = rf.read().rstrip()
    except FileNotFoundError:
        print(f"リソースファイルが見つかりません: {resource_file}")
        return
    # 各ディレクトリを再帰処理
    for d in TARGET_DIRS:
        base = os.path.join(project_root, d)
        for root, _, files in os.walk(base):
            for fname in files:
                if fname.endswith(TARGET_EXTENSIONS):
                    process_file(os.path.join(root, fname), new_copyright)

if __name__ == '__main__':
    main()
