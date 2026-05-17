"""Generate HLS C++ depfiles without globbing include directories."""
from __future__ import annotations

import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile


def _parse_depfile(text: str) -> list[str]:
    # GCC/Clang with -MP appends dummy `header:` rules after the primary
    # target.  Only parse the first continued rule; otherwise those dummy
    # targets become bogus prerequisites of our stamp.
    primary_lines: list[str] = []
    for line in text.splitlines():
        if not primary_lines:
            primary_lines.append(line)
        elif primary_lines[-1].endswith("\\"):
            primary_lines.append(line)
        else:
            break
    text = "\n".join(primary_lines).replace("\\\n", " ")
    if ":" not in text:
        return []
    _, deps = text.split(":", 1)
    result: list[str] = []
    current = ""
    escaped = False
    for ch in deps.strip():
        if escaped:
            current += ch
            escaped = False
        elif ch == "\\":
            escaped = True
        elif ch.isspace():
            if current:
                result.append(current)
                current = ""
        else:
            current += ch
    if current:
        result.append(current)
    return result


def _quote_make(path: str) -> str:
    return path.replace(" ", "\\ ")


def generate_depfile(
    *, compiler: str, sources: list[str], target: str, output: str,
    includes: list[str], defines: list[str], std: str, extra_args: list[str]
) -> list[str]:
    all_deps: set[str] = set()
    out = Path(output)
    out.parent.mkdir(parents=True, exist_ok=True)
    for src in sources:
        with tempfile.NamedTemporaryFile("r", suffix=".d", delete=False) as tmp:
            tmp_path = Path(tmp.name)
        cmd = [compiler, f"-std={std}", "-MMD", "-MP", "-MF", str(tmp_path), "-MT", target]
        for inc in includes:
            cmd.extend(["-I", inc])
        for define in defines:
            cmd.append(f"-D{define}")
        cmd.extend(extra_args)
        cmd.extend(["-fsyntax-only", src])
        try:
            subprocess.run(cmd, check=True, text=True)
            deps_text = tmp_path.read_text(encoding="utf-8")
        except subprocess.CalledProcessError as exc:
            raise SystemExit(f"HLS dep scan failed for {src}: {' '.join(shlex.quote(x) for x in cmd)}") from exc
        finally:
            try:
                tmp_path.unlink()
            except FileNotFoundError:
                pass
        all_deps.update(_parse_depfile(deps_text))
    deps = sorted(all_deps)
    out.write_text(_quote_make(target) + ": " + " ".join(_quote_make(d) for d in deps) + "\n", encoding="utf-8")
    return deps


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--compiler", required=True)
    ap.add_argument("--source", action="append", required=True)
    ap.add_argument("--target", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--include", action="append", default=[])
    ap.add_argument("--define", action="append", default=[])
    ap.add_argument("--std", default="c++14")
    ap.add_argument("--extra-arg", action="append", default=[])
    ns = ap.parse_args(argv)
    generate_depfile(
        compiler=ns.compiler, sources=ns.source, target=ns.target, output=ns.output,
        includes=ns.include, defines=ns.define, std=ns.std, extra_args=ns.extra_arg,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
