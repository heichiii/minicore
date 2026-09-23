#!/usr/bin/env python3
"""Generate a self-contained ELF and QEMU address-space report."""

from __future__ import annotations

import argparse
import datetime as dt
import html
import json
import math
import pathlib
import re
import subprocess
import sys
from dataclasses import dataclass


@dataclass
class Region:
    name: str
    start: int
    size: int
    kind: str = "other"
    detail: str = ""

    @property
    def end(self) -> int:
        return self.start + self.size


def command(args: list[str], *, input_text: str | None = None,
            timeout: float = 10) -> str:
    result = subprocess.run(
        args, input=input_text, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, timeout=timeout, check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(args)}\n"
            f"{result.stdout}"
        )
    return result.stdout


def parse_sections(text: str) -> list[Region]:
    pattern = re.compile(
        r"^\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-fA-F]+)\s+"
        r"[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+(\S*)",
        re.MULTILINE,
    )
    regions = []
    for match in pattern.finditer(text):
        name, section_type, address, size, flags = match.groups()
        if "A" not in flags or int(size, 16) == 0:
            continue
        if "X" in flags:
            kind = "code"
        elif "W" in flags:
            kind = "data"
        else:
            kind = "rodata"
        regions.append(Region(name, int(address, 16), int(size, 16), kind,
                              f"{section_type}, flags {flags}"))
    return sorted(regions, key=lambda region: region.start)


def parse_segments(text: str) -> list[Region]:
    pattern = re.compile(
        r"^\s*LOAD\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+"
        r"(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+"
        r"(0x[0-9a-fA-F]+)\s+(.+?)\s+(0x[0-9a-fA-F]+)$",
        re.MULTILINE,
    )
    segments = []
    for index, match in enumerate(pattern.finditer(text)):
        offset, virtual, physical, file_size, memory_size, flags, align = match.groups()
        segments.append(Region(
            f"LOAD #{index}", int(virtual, 16), int(memory_size, 16), "segment",
            f"PA {physical}; file {file_size}; offset {offset}; {flags.strip()}; align {align}",
        ))
    return segments


def parse_symbols(text: str) -> dict[str, int]:
    wanted = re.compile(
        r"__(?:kernel|text|rodata|data|bss|boot_stack)_(?:start|end|bottom|top)$"
    )
    symbols: dict[str, int] = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and wanted.match(parts[-1]):
            symbols[parts[-1]] = int(parts[0], 16)
    return symbols


def qemu_snapshot(qemu: str, machine: str, memory: str, elf: str) -> tuple[str, str]:
    requests = "\n".join([
        json.dumps({"execute": "qmp_capabilities"}),
        json.dumps({"execute": "human-monitor-command",
                    "arguments": {"command-line": "info mtree"}}),
        json.dumps({"execute": "human-monitor-command",
                    "arguments": {"command-line": "info mem"}}),
        json.dumps({"execute": "quit"}),
        "",
    ])
    output = command([
        qemu, "-machine", machine, "-m", memory, "-smp", "1",
        "-display", "none", "-serial", "none", "-bios", "default",
        "-kernel", elf, "-S", "-qmp", "stdio",
    ], input_text=requests)
    returns: list[str] = []
    for line in output.splitlines():
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(value.get("return"), str):
            returns.append(value["return"])
    if len(returns) < 2:
        raise RuntimeError(f"QEMU did not return monitor snapshots:\n{output}")
    return returns[-2], returns[-1].strip()


def probe_guest(qemu: str, machine: str, memory: str, elf: str,
                timeout: float) -> tuple[list[Region], str]:
    process = subprocess.Popen([
        qemu, "-machine", machine, "-m", memory, "-smp", "1",
        "-nographic", "-monitor", "none", "-bios", "default",
        "-kernel", elf, "-no-reboot",
    ], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            output, _ = process.communicate(timeout=1)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate()

    regions: list[Region] = []
    range_pattern = re.compile(
        r"^(ram|reserved|uart|plic|virtio): base=(0x[0-9a-fA-F]+) "
        r"size=(0x[0-9a-fA-F]+)$", re.MULTILINE,
    )
    kinds = {"ram": "ram", "reserved": "firmware", "uart": "mmio",
             "plic": "mmio", "virtio": "mmio"}
    for match in range_pattern.finditer(output):
        name, start, size = match.groups()
        regions.append(Region(f"guest: {name}", int(start, 16), int(size, 16),
                              kinds[name], "Reported by MiniCore's DTB parser"))

    dtb_address = re.search(r"^dtb\s*=\s*(0x[0-9a-fA-F]+)$", output, re.MULTILINE)
    dtb_size = re.search(r"^dtb size\s*=\s*(0x[0-9a-fA-F]+)$", output, re.MULTILINE)
    if dtb_address and dtb_size:
        regions.append(Region("guest: flattened device tree",
                              int(dtb_address.group(1), 16),
                              int(dtb_size.group(1), 16), "dtb",
                              "Boot-time address and validated size"))
    return sorted(regions, key=lambda region: (region.start, -region.size)), output


def parse_mtree(text: str) -> list[Region]:
    in_memory = False
    regions: list[Region] = []
    pattern = re.compile(
        r"^    ([0-9a-fA-F]{16})-([0-9a-fA-F]{16}) "
        r"\(prio .*?, ([^)]+)\): (.+)$"
    )
    for line in text.splitlines():
        if line == "address-space: memory":
            in_memory = True
            continue
        if in_memory and (line.startswith("address-space:") or
                          line.startswith("memory-region:")):
            break
        if not in_memory:
            continue
        match = pattern.match(line)
        if not match:
            continue
        start, end, access, name = match.groups()
        start_value, end_value = int(start, 16), int(end, 16)
        kind = "ram" if access == "ram" else "rom" if access in {"rom", "romd"} else "mmio"
        regions.append(Region(name, start_value, end_value - start_value + 1,
                              kind, access))
    return sorted(regions, key=lambda region: region.start)


def fmt_address(value: int) -> str:
    return f"0x{value:016x}"


def fmt_size(value: int) -> str:
    units = [(1 << 30, "GiB"), (1 << 20, "MiB"), (1 << 10, "KiB")]
    for divisor, suffix in units:
        if value >= divisor and value % divisor == 0:
            return f"{value // divisor} {suffix}"
        if value >= divisor:
            return f"{value / divisor:.2f} {suffix}"
    return f"{value} B"


def escape(value: object) -> str:
    return html.escape(str(value))


def region_rows(regions: list[Region]) -> str:
    largest = max((math.log2(max(region.size, 1)) for region in regions), default=1)
    rows = []
    for region in regions:
        width = 12 + 88 * math.log2(max(region.size, 2)) / largest
        rows.append(f"""
          <div class="region-row">
            <div class="addresses"><code>{fmt_address(region.start)}</code><br>
              <code>{fmt_address(region.end - 1)}</code></div>
            <div class="region-body">
              <div class="bar {escape(region.kind)}" style="width:{width:.1f}%">
                <span>{escape(region.name)}</span><b>{escape(fmt_size(region.size))}</b>
              </div>
              <small>{escape(region.detail)}</small>
            </div>
          </div>""")
    return "".join(rows)


def kernel_rows(sections: list[Region], symbols: dict[str, int]) -> str:
    if not sections:
        return "<p>No allocated ELF sections found.</p>"
    kernel_start = symbols.get("__kernel_start", sections[0].start)
    kernel_end = symbols.get("__kernel_end", max(section.end for section in sections))
    pieces: list[Region] = []
    cursor = kernel_start
    for section in sections:
        if section.start > cursor:
            pieces.append(Region("alignment gap", cursor, section.start - cursor, "gap"))
        pieces.append(section)
        cursor = section.end
    if cursor < kernel_end:
        pieces.append(Region("alignment gap", cursor, kernel_end - cursor, "gap"))

    stack_bottom = symbols.get("__boot_stack_bottom")
    stack_top = symbols.get("__boot_stack_top")
    rows = region_rows(pieces)
    if stack_bottom is not None and stack_top is not None:
        rows += f"""
          <div class="annotation"><span>↳ boot stack inside .bss</span>
          <code>{fmt_address(stack_bottom)}–{fmt_address(stack_top - 1)}</code>
          <b>{fmt_size(stack_top - stack_bottom)}</b></div>"""
    return rows


def ram_rows(qemu_regions: list[Region], sections: list[Region],
             symbols: dict[str, int]) -> str:
    ram = next((region for region in qemu_regions if region.kind == "ram"), None)
    if ram is None or not sections:
        return "<p>RAM or ELF sections were not found.</p>"
    kernel_start = symbols.get("__kernel_start", sections[0].start)
    kernel_end = symbols.get("__kernel_end", max(section.end for section in sections))
    regions = []
    if ram.start < kernel_start:
        regions.append(Region("pre-kernel linker gap", ram.start,
                              kernel_start - ram.start, "firmware",
                              "Contains OpenSBI, its reservations, and unused padding; see the runtime panel."))
    regions.append(Region("kernel image", kernel_start, kernel_end - kernel_start,
                          "kernel", "Expanded in the ELF panel below."))
    if kernel_end < ram.end:
        regions.append(Region("remaining RAM", kernel_end, ram.end - kernel_end,
                              "free", "Includes runtime data such as the QEMU DTB."))
    return region_rows(regions)


def table_segments(segments: list[Region]) -> str:
    rows = []
    for segment in segments:
        rows.append(
            f"<tr><td>{escape(segment.name)}</td><td><code>{fmt_address(segment.start)}</code></td>"
            f"<td>{escape(fmt_size(segment.size))}</td><td>{escape(segment.detail)}</td></tr>"
        )
    return "".join(rows)


def build_html(*, elf: str, machine: str, memory: str, sections: list[Region],
               segments: list[Region], symbols: dict[str, int],
               qemu_regions: list[Region], runtime_regions: list[Region],
               mtree: str, translation: str, guest_log: str) -> str:
    generated = dt.datetime.now().astimezone().isoformat(timespec="seconds")
    entry = symbols.get("__kernel_start", sections[0].start if sections else 0)
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MiniCore memory layout</title>
<style>
:root{{--bg:#0b1020;--panel:#121a2d;--ink:#e7edf8;--muted:#91a0b8;--line:#293653;
--code:#4f8cff;--rodata:#8e6cff;--data:#ec5f8a;--gap:#35415b;--ram:#25a18e;
--rom:#d19a4a;--mmio:#65748f;--firmware:#d19a4a;--kernel:#ec5f8a;--free:#25a18e}}
*{{box-sizing:border-box}} body{{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,sans-serif}}
main{{max-width:1180px;margin:auto;padding:40px 24px 80px}} h1{{font-size:32px;margin:0 0 6px}}
h2{{margin:0 0 18px;font-size:21px}} .lede,.muted,small{{color:var(--muted)}}
.summary{{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px;margin:26px 0}}
.card,.panel{{background:var(--panel);border:1px solid var(--line);border-radius:12px}}
.card{{padding:14px 16px}} .card b{{display:block;font-size:18px}} .panel{{padding:22px;margin-top:18px}}
.region-row{{display:grid;grid-template-columns:230px 1fr;gap:18px;padding:9px 0;border-top:1px solid #202c46}}
.region-row:first-child{{border-top:0}} .addresses{{color:var(--muted);font-size:12px}}
.region-body{{min-width:0}} .bar{{min-width:180px;border-radius:5px;padding:7px 10px;display:flex;justify-content:space-between;gap:12px}}
.bar b{{white-space:nowrap}} .bar.code{{background:var(--code)}} .bar.rodata{{background:var(--rodata)}}
.bar.data,.bar.kernel{{background:var(--data)}} .bar.gap{{background:var(--gap)}} .bar.ram,.bar.free{{background:var(--ram)}}
.bar.rom,.bar.firmware{{background:var(--rom)}} .bar.mmio,.bar.segment{{background:var(--mmio)}}
.bar.dtb{{background:#18a9c7}}
.annotation{{margin:8px 0 4px 248px;padding:9px 12px;border-left:3px solid var(--data);display:flex;gap:18px;flex-wrap:wrap}}
code{{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}} table{{width:100%;border-collapse:collapse}}
th,td{{text-align:left;padding:9px;border-bottom:1px solid var(--line)}} th{{color:var(--muted)}}
details{{margin-top:16px}} pre{{overflow:auto;background:#090e1a;padding:16px;border-radius:8px;color:#b8c5da}}
.pill{{display:inline-block;padding:3px 9px;border-radius:99px;background:#203052;color:#bfd2ff}}
@media(max-width:700px){{.region-row{{grid-template-columns:1fr}}.annotation{{margin-left:0}}.bar{{min-width:140px}}}}
</style></head><body><main>
<h1>MiniCore memory layout</h1>
<p class="lede">Generated from the linked ELF and a live, paused QEMU monitor snapshot.</p>
<div class="summary">
  <div class="card"><span class="muted">ELF</span><b>{escape(elf)}</b></div>
  <div class="card"><span class="muted">Entry</span><b><code>{fmt_address(entry)}</code></b></div>
  <div class="card"><span class="muted">QEMU</span><b>{escape(machine)} · {escape(memory)}</b></div>
  <div class="card"><span class="muted">Generated</span><b>{escape(generated)}</b></div>
</div>
<section class="panel"><h2>1. RAM overview</h2>
<p class="muted">A semantic zoom of the RAM region. Bar lengths use a logarithmic scale so the kernel remains visible.</p>
{ram_rows(qemu_regions, sections, symbols)}</section>
<section class="panel"><h2>2. Kernel ELF in memory</h2>
<p class="muted">Only allocated sections are shown. Addresses are half-open internally; displayed end addresses are inclusive.</p>
{kernel_rows(sections, symbols)}</section>
<section class="panel"><h2>3. ELF load segments</h2><table>
<thead><tr><th>Segment</th><th>Virtual address</th><th>Memory size</th><th>Loader view</th></tr></thead>
<tbody>{table_segments(segments)}</tbody></table></section>
<section class="panel"><h2>4. Guest-observed runtime regions</h2>
<p class="muted">QEMU was booted and MiniCore's console output was parsed. These ranges come from the guest's DTB parser, including the actual boot reservations and DTB location.</p>
{region_rows(runtime_regions) if runtime_regions else '<p>No guest runtime ranges were reported before the probe timeout.</p>'}
<details><summary>Raw guest boot log</summary><pre>{escape(guest_log)}</pre></details></section>
<section class="panel"><h2>5. QEMU physical address space</h2>
<p class="muted">Top-level regions from <code>info mtree</code>. Bar lengths are logarithmic; address order is exact.</p>
{region_rows(qemu_regions)}</section>
<section class="panel"><h2>6. Address translation</h2>
<p><span class="pill">QEMU monitor: {escape(translation or 'no result')}</span></p>
<p class="muted">This snapshot is taken with the CPU paused at reset. For the current pre-MMU kernel, linked, virtual and physical addresses are identical. Once MiniCore enables paging, add a kernel page-table dump at the point of interest for an authoritative runtime view.</p>
<details><summary>Raw QEMU <code>info mtree</code></summary><pre>{escape(mtree)}</pre></details>
</section>
</main></body></html>"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True)
    parser.add_argument("--readelf", default="readelf")
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--qemu", default="qemu-system-riscv64")
    parser.add_argument("--machine", default="virt")
    parser.add_argument("--memory", default="256M")
    parser.add_argument("--probe-timeout", type=float, default=3.0)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    try:
        sections_text = command([args.readelf, "-SW", args.elf])
        segments_text = command([args.readelf, "-lW", args.elf])
        symbols_text = command([args.nm, "-n", args.elf])
        mtree, translation = qemu_snapshot(
            args.qemu, args.machine, args.memory, args.elf
        )
        runtime_regions, guest_log = probe_guest(
            args.qemu, args.machine, args.memory, args.elf, args.probe_timeout
        )
        sections = parse_sections(sections_text)
        segments = parse_segments(segments_text)
        symbols = parse_symbols(symbols_text)
        qemu_regions = parse_mtree(mtree)
        if not sections:
            raise RuntimeError("no allocated ELF sections were parsed")
        if not qemu_regions:
            raise RuntimeError("no QEMU physical regions were parsed")
        report = build_html(
            elf=args.elf, machine=args.machine, memory=args.memory,
            sections=sections, segments=segments, symbols=symbols,
            qemu_regions=qemu_regions, runtime_regions=runtime_regions,
            mtree=mtree, translation=translation, guest_log=guest_log,
        )
        output = pathlib.Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(report, encoding="utf-8")
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"layout: {error}", file=sys.stderr)
        return 1

    print(f"layout: wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
