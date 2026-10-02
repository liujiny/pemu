#!/usr/bin/env python3
"""Check native loader mappings and optionally preserved LLVM section contents."""
import argparse
from pathlib import Path
import struct
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def elf(path):
    data = Path(path).read_bytes()
    require(data[:7] == b"\x7fELF\x02\x01\x01", "expected little-endian ELF64")
    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data)
    require(header[2] == 62, "expected x86-64")
    require(header[9] == 56, "unexpected program header size")
    require(header[5] + header[10] * 56 <= len(data), "truncated program headers")
    programs = [struct.unpack_from("<IIQQQQQQ", data, header[5] + i * 56)
                for i in range(header[10])]
    return data, header, programs


def dynamic(data, programs):
    entries = [p for p in programs if p[0] == 2]
    require(len(entries) == 1, "expected one PT_DYNAMIC")
    p = entries[0]
    require(p[5] % 16 == 0 and p[2] + p[5] <= len(data), "bad dynamic table")
    tags = {}
    for tag, value in struct.iter_unpack("<QQ", data[p[2]:p[2] + p[5]]):
        if tag == 0:
            break
        tags[tag] = value
    return tags


def verify(path, source=None):
    data, header, programs = elf(path)
    loads = [p for p in programs if p[0] == 1]
    require(loads, "no PT_LOAD segments")
    for p in loads:
        _, flags, offset, address, _, filesz, memsz, align = p
        require(filesz <= memsz, "PT_LOAD file size exceeds memory size")
        require(offset + filesz <= len(data), "PT_LOAD exceeds file")
        require(align <= 1 or align & (align - 1) == 0, "non-power-of-two alignment")
        require(align <= 1 or offset % align == address % align,
                f"PT_LOAD mapping is not congruent: offset={offset:#x}, VA={address:#x}, align={align:#x}")
    require(any(p[1] & 1 and p[3] <= header[4] < p[3] + p[5] for p in loads),
            "entry point is not in executable file-backed memory")

    def mapped(address, size):
        matches = [p for p in loads if p[3] <= address and address + size <= p[3] + p[5]]
        require(len(matches) == 1, f"ambiguous or missing mapping at {address:#x}")
        p = matches[0]
        return p[2] + address - p[3]

    params = [p for p in programs if p[0] == 0x61000001]
    require(len(params) == 1, "expected one process parameter")
    param = params[0]
    require(param[5] >= 0x18, "process parameter too short")
    require(mapped(param[3], param[5]) == param[2], "process parameter file mapping mismatch")
    require(data[param[2] + 8:param[2] + 12] == b"ORBI", "bad process parameter magic")
    tags = dynamic(data, programs)
    plt_got = tags.get(3, 0)
    if tags.get(2, 0):
        require(plt_got != 0, "PLT relocations have no DT_PLTGOT")
    if plt_got:
        mapped(plt_got, 24)
        size = tags.get(8, 0)
        require(size % 24 == 0 and tags.get(9) == 24, "bad RELA table")
        offset = mapped(tags[7], size)
        for target, info, _ in struct.iter_unpack("<QQQ", data[offset:offset + size]):
            if info & 0xffffffff:
                require(not (target < plt_got + 24 and target + 8 > plt_got),
                        f"DT_PLTGOT reserved slots overlap application relocation at {target:#x}")
    count = 0
    if source:
        original, sh, source_programs = elf(source)
        source_tags = dynamic(original, source_programs)
        require(plt_got == source_tags.get(3, 0),
                f"DT_PLTGOT changed: {source_tags.get(3, 0):#x} -> {plt_got:#x}")
        require(sh[11] == 64 and sh[6] + sh[12] * 64 <= len(original), "bad source sections")
        sections = [struct.unpack_from("<IIQQQQIIQQ", original, sh[6] + i * 64)
                    for i in range(sh[12])]
        names = sections[sh[13]]
        strings = original[names[4]:names[4] + names[5]]
        for s in sections:
            name = strings[s[0]:].split(b"\0", 1)[0].decode()
            if not s[2] & 2 or s[1] == 8 or not s[5]:
                continue
            if name in (".dynstr", ".dynsym", ".dynamic", ".hash", ".gnu.hash") or name.startswith(".rela."):
                continue
            offset = mapped(s[3], s[5])
            require(data[offset:offset + s[5]] == original[s[4]:s[4] + s[5]],
                    f"section content mapping mismatch: {name}")
            count += 1
    print(f"PASS {path}: {len(loads)} load mappings; {count} preserved source sections; DT_PLTGOT={plt_got:#x}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf")
    parser.add_argument("--compare-llvm")
    args = parser.parse_args()
    try:
        verify(args.elf, args.compare_llvm)
    except (ValueError, OSError, struct.error, IndexError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
