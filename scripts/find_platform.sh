#!/usr/bin/env bash
set -euo pipefail

target="${1:-}"
requested="${2:-}"

if [[ -n "$requested" && -f "$requested" ]]; then
  printf '%s\n' "$requested"
  exit 0
fi

roots=()
if [[ -n "${XILINX_VITIS:-}" ]]; then
  roots+=("$XILINX_VITIS/base_platforms")
fi
roots+=(/opt/xilinx/platforms)
if [[ -d /tools/Xilinx/Vitis ]]; then
  while IFS= read -r -d '' dir; do
    roots+=("$dir")
  done < <(find /tools/Xilinx/Vitis -maxdepth 2 -type d -name base_platforms -print0 2>/dev/null || true)
fi

patterns=()
if [[ -n "$requested" ]]; then
  base="$(basename "$requested")"
  stem="${base%.xpfm}"
  [[ -n "$stem" ]] && patterns+=("*$stem*.xpfm")
fi
case "$target" in
  u50) patterns+=("*u50*.xpfm") ;;
  u55c) patterns+=("*u55c*.xpfm") ;;
  u200) patterns+=("*u200*.xpfm") ;;
  u250) patterns+=("*u250*.xpfm") ;;
  u280) patterns+=("*u280*.xpfm") ;;
  vck5000) patterns+=("*vck5000*.xpfm") ;;
  zcu102) patterns+=("*zcu102*.xpfm") ;;
  zcu104) patterns+=("*zcu104*.xpfm") ;;
  zcu106) patterns+=("*zcu106*.xpfm") ;;
  kv260) patterns+=("*kv260*.xpfm" "*k26*.xpfm") ;;
  *) [[ -n "$target" ]] && patterns+=("*$target*.xpfm") ;;
esac

for root in "${roots[@]}"; do
  [[ -d "$root" ]] || continue
  for pat in "${patterns[@]}"; do
    match="$(find "$root" -type f -iname "$pat" -print -quit 2>/dev/null || true)"
    if [[ -n "$match" ]]; then
      printf '%s\n' "$match"
      exit 0
    fi
  done
done

printf '%s\n' "$requested"
