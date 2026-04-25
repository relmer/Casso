#!/usr/bin/env python3
"""
Add 80-char comment block headers to My6502 source files.
Ensures 5 blank lines between top-level constructs (after closing }).
"""

import re, os

BASE = '/home/runner/work/My6502/My6502'
DIVIDER = '/' * 80


def comment_block(name, indent=''):
    return [indent + DIVIDER, indent + '//', indent + '//  ' + name, indent + '//', indent + DIVIDER]


def get_indent(line):
    return line[:len(line) - len(line.lstrip())]


def extract_name(line):
    stripped = line.strip()
    m = re.search(r'TEST_(?:CLASS|METHOD|MODULE_INITIALIZE)\s*\(\s*(\w+)\s*\)', stripped)
    if m:
        return m.group(1)
    paren = stripped.find('(')
    if paren < 0:
        return stripped
    before = stripped[:paren].rstrip()
    m2 = re.search(r'(\w+)\s*$', before)
    return m2.group(1) if m2 else before.strip()


def is_separator_comment(line):
    stripped = line.strip()
    if not stripped.startswith('//'):
        return False
    content = stripped[2:].strip()
    return (len(content) > 10 and
            (all(c == '-' for c in content) or all(c == '=' for c in content)))


def is_comment_line(line):
    return line.strip().startswith('//')


def strip_trailing_blanks(out):
    while out and not out[-1].strip():
        out.pop()


def strip_old_comment_block(out):
    """Remove old separator comment blocks. Returns True if removed."""
    if not out or not is_comment_line(out[-1]):
        return False
    temp = []
    while out and is_comment_line(out[-1]):
        temp.append(out.pop())
    if any(is_separator_comment(l) for l in temp):
        strip_trailing_blanks(out)
        return True
    else:
        for l in reversed(temp):
            out.append(l)
        return False


def determine_blank_count(out):
    if not out:
        return 2
    last = out[-1].strip()
    if last in ('}', '};'):
        return 5
    if last.startswith('//'):
        return 0
    return 2


def process_with_detector(content, is_start_fn):
    lines = content.split('\n')
    out = []
    for line in lines:
        result = is_start_fn(line)
        if result:
            name, indent = result
            strip_trailing_blanks(out)
            removed = strip_old_comment_block(out)
            if removed:
                strip_trailing_blanks(out)
            blank_count = determine_blank_count(out)
            out.extend([''] * blank_count)
            out.extend(comment_block(name, indent))
            out.append(line)
        else:
            out.append(line)
    # Normalize trailing: keep exactly one trailing newline
    while len(out) > 1 and not out[-1].strip() and not out[-2].strip():
        out.pop()
    if out and not out[-1].strip():
        pass  # keep one trailing blank
    return '\n'.join(out)


# ─── Detector factories ───────────────────────────────────────────────────────

CTRL_KEYWORDS = ('if ', 'else', 'for ', 'while ', 'switch ', 'case ', 'return ',
                 'auto ', 'break', 'continue', 'throw ', 'Assert', 'cpu.',
                 'result.', 'table.', 'static_assert', 'ASSERT', 'CBRA', 'CBR ',
                 'std::', 'output.', 'lines.', 'info.', 'sorted.', 'symbols.',
                 'referencedLabels', 'lineInfos', 'str.', 'trimmed', 'values.',
                 'current', 'const ', 'int ', 'bool ', 'size_t ', 'Byte ',
                 'Word ', 'HRESULT', 'char ', 'long ', 'va_list', 'va_start',
                 'va_end', 'WCHAR ', 'snprintf', 'opcodes[', 'branchOpcodes[')


def is_top_level_func_line(line):
    """True if line starts a top-level function at col 0."""
    if not line or line[0] in (' ', '\t'):
        return False
    stripped = line.strip()
    if not stripped:
        return False
    if stripped[0] in ('#', '}', '{'):
        return False
    if stripped.startswith('//') or stripped.startswith('/*') or stripped.startswith('*'):
        return False
    skip = ('using ', 'namespace ', 'class ', 'struct ', 'extern ', 'typedef ')
    if any(stripped.startswith(s) for s in skip):
        return False
    if '(' not in stripped:
        return False
    if stripped.endswith(';'):
        return False
    return True


def make_regular_cpp_detector():
    def detect(line):
        if is_top_level_func_line(line):
            return (extract_name(line), '')
        return None
    return detect


def make_test_cpp_detector():
    def detect(line):
        stripped = line.strip()
        if not stripped:
            return None
        indent = get_indent(line)
        indent_len = len(indent)

        # Column 0
        if indent_len == 0:
            if is_top_level_func_line(line):
                return (extract_name(line), '')
            return None

        # 4-space indent: TEST_CLASS or namespace-level static helpers
        if indent_len == 4:
            if stripped[0] in ('}', '{', '#'):
                return None
            if stripped.startswith('//') or stripped.startswith('/*'):
                return None
            if stripped.endswith(';'):
                return None
            if re.match(r'TEST_CLASS\s*\(', stripped):
                return (extract_name(line), '    ')
            if '(' not in stripped:
                return None
            if any(stripped.startswith(c) for c in CTRL_KEYWORDS):
                return None
            # Must look like a function definition (has return type before name)
            # Pattern: optional "static" + type + name + (
            if re.match(r'(?:static\s+)?\w[\w\s:*&<>]*\w\s*\(', stripped):
                return (extract_name(line), '    ')
            return None

        # 8-space indent: TEST_METHOD
        if indent_len == 8:
            if re.match(r'TEST_METHOD\s*\(', stripped):
                return (extract_name(line), '        ')
            return None

        return None
    return detect


def make_header_detector():
    def detect(line):
        # Only process top-level (column 0) class/struct definitions
        if not line or line[0] in (' ', '\t'):
            return None
        stripped = line.strip()
        if not stripped:
            return None
        if not re.match(r'^(?:class|struct)\s+\w+', stripped):
            return None
        if stripped.endswith(';'):
            return None
        m = re.match(r'^(?:class|struct)\s+(\w+)', stripped)
        if not m:
            return None
        name = m.group(1)
        return (name, '')
    return detect


# ─── File processing ──────────────────────────────────────────────────────────

def process_file(path, detector_fn):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    new_content = process_with_detector(content, detector_fn)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(f'Processed: {path}')


def main():
    reg = make_regular_cpp_detector()
    test = make_test_cpp_detector()
    hdr = make_header_detector()

    # My6502Core .cpp files
    for fname in ['Cpu.cpp', 'CpuOperations.cpp', 'Microcode.cpp',
                  'Assembler.cpp', 'Parser.cpp', 'OpcodeTable.cpp',
                  'Ehm.cpp', 'Utils.cpp']:
        process_file(f'{BASE}/My6502Core/{fname}', reg)

    # My6502Core .h files
    for fname in ['Cpu.h', 'CpuOperations.h', 'Microcode.h',
                  'Assembler.h', 'Parser.h', 'OpcodeTable.h']:
        process_file(f'{BASE}/My6502Core/{fname}', hdr)

    # My6502 .cpp files
    process_file(f'{BASE}/My6502/My6502.cpp', reg)
    process_file(f'{BASE}/My6502/CommandLine.cpp', reg)

    # My6502 .h file
    process_file(f'{BASE}/My6502/CommandLine.h', hdr)

    # UnitTest .cpp files
    for fname in ['CpuInitializationTests.cpp', 'CpuOperationTests.cpp',
                  'AddressingModeTests.cpp', 'AssemblerTests.cpp',
                  'ParserTests.cpp', 'OpcodeTableTests.cpp',
                  'IntegrationTests.cpp', 'ModuleSetup.cpp',
                  'EhmTestHelper.cpp']:
        process_file(f'{BASE}/UnitTest/{fname}', test)

    # UnitTest .h files
    for fname in ['EhmTestHelper.h', 'TestHelpers.h']:
        process_file(f'{BASE}/UnitTest/{fname}', hdr)


if __name__ == '__main__':
    main()
