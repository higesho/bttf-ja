# 訳文（UTF-8）→ ゲームに入れるバイト列（EUC-JP、改行できる所に半角空白、本物の空白は 0xA0）
import re
NO_START = set('、。，．・：；？！ー－―…‥）」』】〕〉》］｝ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ々ゝゞヽヾ～〜!?)].,:;%')
NO_END = set('（「『【〔〈《［｛([')
REPL = {'～': '〜', '－': '−', '―': '─', '“': '「', '”': '」', '‘': '『', '’': '』', ' ': ' ', '♪': '〜', '‼': '！！', '⁉': '！？'}
TAG = re.compile(r'^((?:\s*(?:\[[^\]]*\]|\{[^}]*\}))*\s*)')

def wide(c):
    return ord(c) >= 0x3000 or 0xff01 <= ord(c) <= 0xff60

def encode(s):
    """訳文を 1 本のバイト列にする"""
    for a, b in REPL.items(): s = s.replace(a, b)
    m = TAG.match(s)
    head, body = m.group(1), s[m.end():]
    out = []
    for line in body.split('\n'):
        res = []
        prev = ''
        for c in line:
            if c == ' ':
                res.append(' '); prev = c; continue
            # 半角の直後の空白は幅が出る（隙間になる）ので入れない。「第1」「約3」も切らない
            if prev and prev != ' ' and wide(prev) and c not in NO_START and prev not in NO_END and not (c.isdigit() and prev in '第約'):
                res.append(' ')
            res.append(c); prev = c
        out.append(''.join(res))
    body = '\n'.join(out)
    b = head.encode('cp1252') + body.replace(' ', '\x00NBSP\x00').encode('euc_jp', errors='replace').replace(b'\x00NBSP\x00', b'\xa0')
    return b
