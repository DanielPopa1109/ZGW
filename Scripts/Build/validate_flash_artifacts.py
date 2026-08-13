#!/usr/bin/env python3
"""Validate ZGW flash artifacts without building or programming hardware."""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


PFLASH_START = 0x80000000
PFLASH_END = 0x805FFFFF
PFLASH_ALIAS_START = 0xA0000000
PFLASH_ALIAS_END = 0xA05FFFFF
UCB_START = 0xAF400000
UCB_END = 0xAF405FFF


@dataclass(frozen=True)
class Range:
    start: int
    end: int

    @property
    def length(self) -> int:
        return self.end - self.start + 1


def fmt_range(r: Range) -> str:
    return f"0x{r.start:08X}-0x{r.end:08X} len=0x{r.length:X}"


def normalize_pflash(addr: int) -> int | None:
    if PFLASH_START <= addr <= PFLASH_END:
        return addr - PFLASH_START
    if PFLASH_ALIAS_START <= addr <= PFLASH_ALIAS_END:
        return addr - PFLASH_ALIAS_START
    return None


def in_ucb(addr: int) -> bool:
    return UCB_START <= addr <= UCB_END


def coalesce(addrs: list[int]) -> list[Range]:
    if not addrs:
        return []
    addrs = sorted(set(addrs))
    ranges: list[Range] = []
    start = prev = addrs[0]
    for addr in addrs[1:]:
        if addr == prev + 1:
            prev = addr
            continue
        ranges.append(Range(start, prev))
        start = prev = addr
    ranges.append(Range(start, prev))
    return ranges


def parse_ihex(path: Path) -> tuple[dict[int, int], list[str]]:
    data: dict[int, int] = {}
    errors: list[str] = []
    base = 0
    seen_eof = False
    lines = path.read_text(errors="replace").splitlines()
    for line_no, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line:
            continue
        if seen_eof:
            errors.append(f"{path}:{line_no}: record after EOF")
        if not line.startswith(":"):
            errors.append(f"{path}:{line_no}: missing ':'")
            continue
        try:
            rec = bytes.fromhex(line[1:])
        except ValueError as exc:
            errors.append(f"{path}:{line_no}: invalid hex: {exc}")
            continue
        if len(rec) < 5:
            errors.append(f"{path}:{line_no}: record too short")
            continue
        size, high, low, rec_type = rec[:4]
        payload = rec[4:-1]
        if len(payload) != size:
            errors.append(f"{path}:{line_no}: byte count mismatch")
            continue
        if (sum(rec) & 0xFF) != 0:
            errors.append(f"{path}:{line_no}: invalid checksum")
        offset = (high << 8) | low
        if rec_type == 0x00:
            for index, value in enumerate(payload):
                addr = base + offset + index
                old = data.get(addr)
                if old is not None and old != value:
                    errors.append(
                        f"{path}:{line_no}: conflicting duplicate byte at 0x{addr:08X}: "
                        f"0x{old:02X} != 0x{value:02X}"
                    )
                data[addr] = value
        elif rec_type == 0x01:
            seen_eof = True
        elif rec_type == 0x02:
            if size != 2:
                errors.append(f"{path}:{line_no}: bad extended segment address length")
            base = int.from_bytes(payload, "big") << 4
        elif rec_type == 0x04:
            if size != 2:
                errors.append(f"{path}:{line_no}: bad extended linear address length")
            base = int.from_bytes(payload, "big") << 16
    if not seen_eof:
        errors.append(f"{path}: missing EOF record")
    return data, errors


def parse_elf_loads(path: Path) -> tuple[list[tuple[int, int, int, int, int, int, int]], list[str]]:
    blob = path.read_bytes()
    errors: list[str] = []
    if blob[:4] != b"\x7fELF":
        return [], [f"{path}: not an ELF file"]
    if blob[4] != 1:
        return [], [f"{path}: only ELF32 is supported"]
    endian = "<" if blob[5] == 1 else ">"
    e_phoff = struct.unpack_from(endian + "I", blob, 28)[0]
    e_phentsize, e_phnum = struct.unpack_from(endian + "HH", blob, 42)
    loads: list[tuple[int, int, int, int, int, int, int]] = []
    for idx in range(e_phnum):
        offset = e_phoff + idx * e_phentsize
        if offset + 32 > len(blob):
            errors.append(f"{path}: program header {idx} is outside file")
            break
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align = struct.unpack_from(
            endian + "IIIIIIII", blob, offset
        )
        if p_type == 1:
            loads.append((idx, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, p_align))
    return loads, errors


def describe_file(path: Path) -> str:
    digest = hashlib.sha256(path.read_bytes()).hexdigest().upper()
    return f"{path} size={path.stat().st_size} sha256={digest}"


def validate_hex(path: Path, allow_ucb: bool, errors: list[str]) -> dict[int, int]:
    data, parse_errors = parse_ihex(path)
    errors.extend(parse_errors)
    ranges = coalesce(list(data))
    print(describe_file(path))
    for r in ranges:
        print(f"  HEX {fmt_range(r)}")
    ucb_addrs = [addr for addr in data if in_ucb(addr)]
    if ucb_addrs and not allow_ucb:
        for r in coalesce(ucb_addrs):
            errors.append(f"{path}: unexpected UCB/BMHD data {fmt_range(r)}")
    for addr in data:
        pflash = normalize_pflash(addr)
        if pflash is None and not in_ucb(addr):
            errors.append(f"{path}: byte outside PFLASH/UCB at 0x{addr:08X}")
    return data


def validate_elf(path: Path, allow_ucb: bool, errors: list[str]) -> list[tuple[int, int, int, int, int, int, int]]:
    loads, parse_errors = parse_elf_loads(path)
    errors.extend(parse_errors)
    print(describe_file(path))
    ranges: list[Range] = []
    for idx, _off, _vaddr, paddr, filesz, memsz, _flags, _align in loads:
        size = max(filesz, memsz)
        if size:
            ranges.append(Range(paddr, paddr + size - 1))
        if filesz:
            start = paddr
            end = paddr + filesz - 1
            if any(in_ucb(addr) for addr in (start, end)) or (start <= UCB_END and end >= UCB_START):
                if not allow_ucb:
                    errors.append(f"{path}: file-backed PT_LOAD[{idx}] targets UCB/BMHD 0x{start:08X}-0x{end:08X}")
            elif normalize_pflash(start) is None or normalize_pflash(end) is None:
                errors.append(f"{path}: file-backed PT_LOAD[{idx}] outside PFLASH 0x{start:08X}-0x{end:08X}")
    for r in coalesce([addr for rg in ranges for addr in (rg.start, rg.end)]):
        pass
    print(f"  ELF PT_LOAD count={len(loads)} file_backed={sum(1 for x in loads if x[4])}")
    for idx, _off, _vaddr, paddr, filesz, memsz, flags, align in loads:
        if filesz and (in_ucb(paddr) or normalize_pflash(paddr) is None):
            print(f"  ELF PT_LOAD[{idx}] paddr=0x{paddr:08X} filesz=0x{filesz:X} memsz=0x{memsz:X} flags=0x{flags:X} align=0x{align:X}")
    return loads


def check_combined_hex(name_to_data: dict[str, dict[int, int]], errors: list[str]) -> None:
    owners: dict[int, tuple[str, int]] = {}
    for name, data in name_to_data.items():
        for addr, value in data.items():
            phys = normalize_pflash(addr)
            if phys is None:
                continue
            old = owners.get(phys)
            if old and old[1] != value:
                errors.append(
                    f"combined HEX conflict at PFLASH+0x{phys:X}: {old[0]}=0x{old[1]:02X}, {name}=0x{value:02X}"
                )
            owners[phys] = (name, value)
    ranges = coalesce([PFLASH_START + addr for addr in owners])
    print("combined edited HEX PFLASH coverage:")
    for r in ranges:
        print(f"  {fmt_range(r)}")
    for a in range(PFLASH_START, PFLASH_END + 1):
        if normalize_pflash(a) not in owners:
            # Report sector-scale gap only, not every byte.
            pass


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--allow-elf-ucb", action="store_true", help="allow file-backed ELF PT_LOAD data in UCB/BMHD")
    parser.add_argument("--allow-hex-ucb", action="store_true", help="allow HEX records in UCB/BMHD")
    parser.add_argument("--elf", action="append", default=[], type=Path)
    parser.add_argument("--hex", action="append", default=[], type=Path)
    args = parser.parse_args()

    errors: list[str] = []
    hex_data: dict[str, dict[int, int]] = {}

    for path in args.elf:
        validate_elf(path, args.allow_elf_ucb, errors)
    for path in args.hex:
        data = validate_hex(path, args.allow_hex_ucb, errors)
        hex_data[str(path)] = data
    if len(hex_data) > 1:
        check_combined_hex(hex_data, errors)

    if errors:
        print("\nFAIL:")
        for error in errors:
            print(f"  {error}")
        return 1
    print("\nPASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
