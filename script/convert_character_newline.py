import subprocess
import sys

# chardetがインストールされていない場合にインストールを試みる
try:
    import chardet
except ImportError:
    print("chardetが見つかりません。インストールを試みます...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "chardet"])

# その後、chardetを使用するコードを続けます
import chardet
import os

# 対象ファイルの拡張子
TARGET_EXTENSIONS = (".h", ".hpp", ".cpp", ".hlsl", ".hlsli")
# 対象ディレクトリ（プロジェクトルートからの相対パス）
TARGET_DIRS = ["api", "core", "platform", "resource/hlsl"]

def detect_encoding(filepath):
    """
    ファイルのエンコーディングを自動的に判定します。
    """
    with open(filepath, "rb") as f:
        raw_data = f.read()
        result = chardet.detect(raw_data)
        return result['encoding']

def convert_to_utf8n_lf(filepath):
    """
    指定されたファイルをUTF-8N文字コードとLF改行コードで保存します。
    """
    try:
        # 実行中のファイルの絶対パスをログ出力
        print(f"Processing file: {os.path.abspath(filepath)}")

        # エンコーディングの自動判定
        file_encoding = detect_encoding(filepath)
        print(f"Detected encoding for {filepath}: {file_encoding}")

        # 判定したエンコーディングでファイルを開く
        with open(filepath, "r", encoding=file_encoding) as f:
            content = f.read()

        # 改行コードをLFに統一
        content = content.replace("\r\n", "\n").replace("\r", "\n")

        # ファイルをUTF-8NとLF改行で再保存
        with open(filepath, "w", encoding="utf-8", newline="") as f:
            f.write(content)
        
        print(f"Converted: {os.path.abspath(filepath)}")
    except Exception as e:
        print(f"Error processing {filepath}: {e}")

def process_directory(root_directory):
    """
    指定したディレクトリ内のすべてのサブディレクトリとファイルに対してconvert_to_utf8n_lfを適用します。
    """
    for target_dir in TARGET_DIRS:
        target_path = os.path.join(root_directory, target_dir)
        
        # 指定ディレクトリ下のサブディレクトリも含めて再帰的にファイルを処理
        for dirpath, _, filenames in os.walk(target_path):
            for fname in filenames:
                if fname.endswith(TARGET_EXTENSIONS):  # 対象ファイルの拡張子が一致する場合
                    filepath = os.path.join(dirpath, fname)
                    # 実行中のディレクトリとファイルパスをログ出力
                    print(f"Found file: {os.path.abspath(filepath)}")
                    convert_to_utf8n_lf(filepath)


if __name__ == "__main__":
    # プロジェクトのルートディレクトリ（変更が必要な場合はここを調整）
    root_directory = "../"  # ここにプロジェクトルートを指定
    process_directory(root_directory)