#!/usr/bin/env python3
"""
generate_doxyfiles.py

This script reads a base Doxyfile and generates copies with different
OUTPUT_LANGUAGE and OUTPUT_DIRECTORY settings.

Usage:
    python generate_doxyfiles.py path/to/Doxyfile

Edit the 'language_configs' list below to match your target languages.
"""
import sys
import os
import csv
from typing import List, Union
import xml.etree.ElementTree as ET


def read_csv_range(
    file_path: str,
    start_row: int,
    end_row: int,
    start_col: int,
    end_col: int,
    encoding: str = 'utf-8',
) -> List[List[str]]:
    """
    CSVファイルの指定範囲（行と列）を読み取って2次元リストで返す。

    Parameters:
    - file_path: 読み込むCSVファイルのパス
    - start_row: 読み取り開始行番号（0始まり、ヘッダー含む）
    - end_row: 読み取り終了行番号（0始まり、inclusive）
    - start_col: 読み取り開始列番号（0始まり）
    - end_col: 読み取り終了列番号（0始まり、inclusive）
    - encoding: ファイルの文字エンコーディング

    Returns:
    - 指定範囲のセルを含む2次元リスト
    """
    data: List[List[str]] = []
    with open(file_path, newline='', encoding=encoding) as f:
        reader = csv.reader(f)
        for row_index, row in enumerate(reader):
            if row_index < start_row:
                continue
            if row_index > end_row:
                break
            # 列のスライス
            data.append(row[start_col:end_col + 1])
    return data

def indent(elem, level=0):
    """ElementTree を使った XML のインデント整形関数"""
    i = "\n" + level*"  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        for e in elem:
            indent(e, level+1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i


def get_last_dir(path: str) -> str:
    # 後ろに余分な「/」があると空文字を返すことがあるので正規化
    normalized = os.path.normpath(path)
    # 最後の要素（ファイル名でもディレクトリ名でも同じ）
    return os.path.basename(normalized)

def generate_doxyfiles( 
         base_path : str, 
         doxy_data: List[List[str]],
         contents_data: List[List[str]],
         overrides=None
        ):
    #ファイルオープン.
    with open(base_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    generated_files = []

    #国ごとにファイルを生成する.
    for country in contents_data:
        updated = []

        for line in lines:
            stripped = line.strip()
            is_set = False
            for param in doxy_data[1:] :
                tag = param[0]
                value = country[ int( param[1] ) ]
                if stripped.partition("=")[0].strip() == tag.strip():
                    updated.append( f'{tag} = {value}\n' )
                    is_set = True
            if is_set == False : updated.append(line)

        for key, value in (overrides or {}).items():
            updated.append(f"{key} = {value}\n")
        out_file =country[ int( doxy_data[0][1] ) ]
        with open(out_file, 'w', encoding='utf-8') as f:
            f.writelines(updated)
        print(f'Generated {out_file!a}')
        generated_files.append(out_file)

    return generated_files

def generate_headerfiles( 
         base_path : str, 
         param_list: List[List[str]],
         contents_data: List[List[str]]
        ):
    #ファイルオープン.
    with open(base_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    generated_files = []

    '''
    #国ごとにファイルを生成する.
    for country in contents_data:
        updated = []

        for line in lines:
            stripped = line.strip()
            is_set = False
            for param in param_list[1:] :
                print("test")
            if is_set == False : updated.append(line)

        out_file =country[ int( param_list[0][1] ) ]
        with open(out_file, 'w', encoding='utf-8') as f:
            f.writelines(updated)
        print(f'Generated {out_file!a}')
        generated_files.append(out_file)
    '''
    return generated_files

def generate_layoutfiles( 
         base_path : str, 
         param_list: List[List[str]],
         contents_data: List[List[str]],
         output_directory: str = "."
        ):
  
    #国ごとにファイルを生成する.
    for country in contents_data:    

        # 1. XML を読み込む
        if not os.path.isfile(base_path):
            print( "XML ファイルが見つかりません:"+ base_path )

        tree = ET.parse(base_path)
        root = tree.getroot()
        
        # 2. <navindex> 要素を取得
        nav = root.find('navindex')
              
        # 3. 挿入ポイントを検索する関数
        def find_tab(type_val, visible_val=None):
            for i, tab in enumerate(nav.findall('tab')):
                if tab.get('type') == type_val:
                    if visible_val is None or tab.get('visible') == visible_val:
                        return i, tab
            return None, None

        # 4. 日本語タブ生成の関数 (Setup/HowTo License)
        def make_xml_group( parent_url, sub_urls, parent_title, sub_titles ):
            tab = ET.Element('tab', {'type':'usergroup','visible':'yes','url':f"{parent_url}",'title':parent_title})
            for ( url, title ) in zip( sub_urls, sub_titles ):
                # Empty catalog slots are not pages; Doxygen turns an empty URL into "user".
                if not url.strip() or not title.strip():
                    continue
                ET.SubElement(tab, 'tab', {'type':'user','visible':'yes','url':f"{url}",'title':title })
            return tab
        
        # Setup と HowTo は <tab type="mainpage"> の直後
        idx_main, _ = find_tab('mainpage', 'yes')
        if idx_main is not None:
            # Setup
            setup_url        = country[ int( param_list[0][1] ) ]
            setup_sub0_url   = country[ int( param_list[1][1] ) ]
            setup_sub1_url   = country[ int( param_list[2][1] ) ]
            setup_sub2_url   = country[ int( param_list[3][1] ) ]
            setup_sub3_url   = country[ int( param_list[4][1] ) ]
            setup_sub4_url   = country[ int( param_list[5][1] ) ]
            setup_title      = country[ int( param_list[13][1] ) ]
            setup_sub0_title = country[ int( param_list[14][1] ) ]
            setup_sub1_title = country[ int( param_list[15][1] ) ]
            setup_sub2_title = country[ int( param_list[16][1] ) ]
            setup_sub3_title = country[ int( param_list[17][1] ) ]
            setup_sub4_title = country[ int( param_list[18][1] ) ]
            ja_setup = make_xml_group(
                setup_url,   [ setup_sub0_url, setup_sub1_url, setup_sub2_url, setup_sub3_url, setup_sub4_url],
                setup_title, [ setup_sub0_title, setup_sub1_title, setup_sub2_title, setup_sub3_title, setup_sub4_title]
                )
            nav.insert(idx_main+1, ja_setup)
            # HowTo
            howto_url        = country[ int( param_list[6][1] ) ]
            howto_sub0_url   = country[ int( param_list[7][1] ) ]
            howto_sub1_url   = country[ int( param_list[8][1] ) ]
            howto_sub2_url   = country[ int( param_list[9][1] ) ]
            howto_sub3_url   = country[ int( param_list[10][1] ) ]
            howto_sub4_url   = country[ int( param_list[11][1] ) ]
            howto_title      = country[ int( param_list[19][1] ) ]
            howto_sub0_title = country[ int( param_list[20][1] ) ]
            howto_sub1_title = country[ int( param_list[21][1] ) ]
            howto_sub2_title = country[ int( param_list[22][1] ) ]
            howto_sub3_title = country[ int( param_list[23][1] ) ]
            howto_sub4_title = country[ int( param_list[24][1] ) ]
            ja_howto = make_xml_group(
                howto_url,   [ howto_sub0_url  , howto_sub1_url,   howto_sub2_url,   howto_sub3_url,   howto_sub4_url],
                howto_title, [ howto_sub0_title, howto_sub1_title, howto_sub2_title, howto_sub3_title, howto_sub4_title]
                )
            nav.insert(idx_main+2, ja_howto)

        # License は <tab type="examples"> の直後
        idx_ex, _ = find_tab('examples', 'yes')
        if idx_ex is not None:
            licenseurl        = country[ int( param_list[12][1] ) ]
            licensetitle      = country[ int( param_list[25][1] ) ]
            ja_license = ET.Element('tab', {'type':'user','visible':'yes','url':licenseurl,'title':licensetitle})
            nav.insert(idx_ex+1, ja_license)

        # インデント整形して保存
        indent(root)
        filename = os.path.join(output_directory, "layout_" + country[0] + ".xml")
        tree.write(filename, encoding='UTF-8', xml_declaration=True)
        print(f"Generated {filename!a}")


def generate_scriptfiles( 
         base_path : str, 
         param_list: List[List[str]],
         contents_data: List[List[str]]
        ):
    #ファイルオープン.
    with open(base_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    generated_files = []
    
    '''
    #国ごとにファイルを生成する.
    for country in contents_data:
        updated = []

        for line in lines:
            stripped = line.strip()
            is_set = False
            for param in param_list[1:] :
                print("test")
            if is_set == False : updated.append(line)

        out_file =country[ int( param_list[0][1] ) ]
        with open(out_file, 'w', encoding='utf-8') as f:
            f.writelines(updated)
        print(f'Generated {out_file!a}')
        generated_files.append(out_file)
    '''
    return generated_files


def generate_open_batch( 
        param_list  : List[List[str]] ,
        contents_data    : List[List[str]] 
        ):
    """
    Creates a batch file that runs doxygen for each provided Doxyfile.
    """

    for country in contents_data:
        out_dir        =country[ int( param_list[0][1] ) ]
        open_file      =country[ int( param_list[1][1] ) ]
        with open(open_file, 'w', encoding='utf-8') as f:
            f.write("@echo off\n")
            f.write("start " + get_last_dir( out_dir ) + "\\html\\index.html\n")
            print(f'Generated open batch file: {open_file}')


def generate_run_batch(batch_path, doxyfiles):
    """
    Creates a batch file that runs doxygen for each provided Doxyfile.
    """
    with open(batch_path, 'w', encoding='utf-8') as f:
        f.write("REM ──────────────────────────────────────────────────────\n")
        f.write("REM バッチファイルのあるディレクトリ（Doxyfile が置いてある場所）\n")
        f.write("set \"SCRIPT_DIR=%~dp0\"\n")
        f.write("REM Doxyfile の１つ上の階層をプロジェクトルートとして移動\n")
        f.write("pushd \"%SCRIPT_DIR%..\"\n")

        for df in doxyfiles:
            f.write(f"echo Running doxygen on {df}...\n")
            f.write(f"doxygen \"%SCRIPT_DIR%{df}\"\n")
            f.write("if errorlevel 1 (popd & exit /b 1)\n")
        f.write("echo All builds complete.\n")
        f.write("REM 元のディレクトリに戻す\n")
        f.write("popd\nexit /b 0\n")


        #f.write("exit /b 0\n")

    print(f'Generated run batch file: {batch_path}')



def generate_remove_batch(batch_path :str, doxyfiles, contents_data, param):
    """
    Creates a batch file that runs doxygen for each provided Doxyfile.
    """
    with open(batch_path, 'w', encoding='utf-8') as f:
        f.write("REM ──────────────────────────────────────────────────────\n")
        for df in doxyfiles:
            f.write(f"echo Remove doxygen on {df}...\n")
            f.write(f"DEL /F /Q \"{df}\"\n")

        for country in contents_data:
            f.write(f"echo Remove layout.xml on {get_last_dir( country[ int( param[ 5 ][1] ) ] )}...\n")
            f.write(f"DEL /F /Q \"{ get_last_dir( country[ int( param[ 5 ][1] ) ] ) }\"\n")

        f.write("exit /b 0\n")
    print(f'Generated remove batch file: {batch_path}')

def main():

    csv_path = 'Resource/transrate.csv'
    header_path = 'Resource/header.html'
    layout_path = 'Resource/layout.xml'
    script_path = 'Resource/filter-tree.js'
    doxy_path = 'Resource/Doxyfile'
    
    # csv読み込み
    csv_head_size = read_csv_range( csv_path, 1, 4, 1, 1 )
    for row in csv_head_size: print(row)
    # csv読み込み
    csv_header = read_csv_range( 
              csv_path
            , int( csv_head_size[0][0] ), int( csv_head_size[1][0] )
            , int( csv_head_size[2][0] ), int( csv_head_size[3][0] ) 
          )
    for row in csv_header: print(row)
    
    
    # 内容読み込み
    print( "\r\n Contents Read Range",  csv_header[28][0],  csv_header[29][0], csv_header[30][0], csv_header[31][0] )
    start_row, end_row = int( csv_header[28][0] ), int( csv_header[29][0] )
    start_col, end_col = int( csv_header[30][0] ), int (csv_header[31][0] )
    contents_data = read_csv_range(csv_path, start_row, end_row, start_col, end_col)
    for row in contents_data:print(row)
    
    if len(sys.argv) != 3:
        print("Usage: python GenerateDoxyfile.py <run_batch> <cleanup_batch>")
        sys.exit(1)

    # doxy setting
    print( "\r\n Doxy Read Range",  csv_header[4][0],  csv_header[5][0], csv_header[6][0], csv_header[7][0] )
    start_row, end_row = int( csv_header[4][0] ), int( csv_header[5][0] )
    start_col, end_col = int( csv_header[6][0] ), int (csv_header[7][0] )
    doxt_param = read_csv_range(csv_path, start_row, end_row, start_col, end_col)
    for row in doxt_param:print(row)
    # Doxyfile を生成
    doxyfiles = generate_doxyfiles(doxy_path, doxt_param, contents_data)

    # layout setting
    print( "\r\n layout Read Range",  csv_header[8][0],  csv_header[9][0], csv_header[10][0], csv_header[11][0] )
    start_row, end_row = int( csv_header[ 8][0] ), int( csv_header[ 9][0] )
    start_col, end_col = int( csv_header[10][0] ), int (csv_header[11][0] )
    layout_param = read_csv_range(csv_path, start_row, end_row, start_col, end_col)
    for row in layout_param:print(row)
    # Layout.xml を生成
    generate_layoutfiles(layout_path, layout_param, contents_data)

    """
    # header setting
    print( "\r\n Header Read Range", csv_header[12][0],  csv_header[13][0], csv_header[14][0], csv_header[15][0] )
    start_row, end_row = int( csv_header[12][0] ), int( csv_header[13][0] )
    start_col, end_col = int( csv_header[14][0] ), int (csv_header[15][0] )
    header_param = read_csv_range(csv_path, start_row, end_row, start_col, end_col)
    for row in header_param:print(row)
    # header.html を生成
    headerfiles = generate_headerfiles(header_path, header_param, contents_data)

    # custom script setting
    print( "\r\n Script Read Range", csv_header[16][0],  csv_header[17][0], csv_header[18][0], csv_header[19][0] )
    start_row, end_row = int( csv_header[16][0] ), int( csv_header[17][0] )
    start_col, end_col = int( csv_header[18][0] ), int (csv_header[19][0] )
    script_param = read_csv_range(csv_path, start_row, end_row, start_col, end_col)
    for row in script_param:print(row)
    # custom script.html を生成
    scriptfiles = generate_scriptfiles(script_path, script_param, contents_data)
    """
    
    # OpenDoxyHTMLバッチを生成.
    print( "\r\n OpenBatch Read Range", csv_header[20][0],  csv_header[21][0], csv_header[22][0], csv_header[23][0] )
    start_row, end_row = int( csv_header[20][0] ), int( csv_header[21][0] )
    start_col, end_col = int( csv_header[22][0] ), int (csv_header[23][0] )
    open_batch_param = read_csv_range(csv_path, start_row, end_row, start_col, end_col)
    for row in open_batch_param:print(row)
    generate_open_batch( open_batch_param, contents_data )


    # 実行用バッチファイルを生成    
    batch_name = sys.argv[1]
    generate_run_batch(batch_name, doxyfiles)

    # 実行用生成    
    batch_name = sys.argv[2]
    generate_remove_batch(batch_name, doxyfiles, contents_data, doxt_param)
    

if __name__ == '__main__':
    main()
