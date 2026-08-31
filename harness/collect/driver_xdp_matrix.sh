#!/usr/bin/env bash
# Which NIC drivers support native XDP and AF_XDP zero-copy?
#
# Reads it out of the kernel source rather than testing, so it can be answered
# for hardware you do not have yet. Produces the table in notes/target-host.md.
#
#   .ndo_bpf         native XDP program attach
#   .ndo_xdp_xmit    XDP_REDIRECT egress (being a redirect target)
#   .ndo_xsk_wakeup  AF_XDP zero-copy
#
# Usage: ./driver_xdp_matrix.sh [kernel-ref]     default: master

set -u
REF="${1:-master}"
D=$(mktemp -d)
trap 'rm -rf "$D"' EXIT
B="https://raw.githubusercontent.com/torvalds/linux/$REF"

declare -A F=(
  [ena-aws]="drivers/net/ethernet/amazon/ena/ena_netdev.c"
  [gve-gcp]="drivers/net/ethernet/google/gve/gve_main.c"
  [mana-azure]="drivers/net/ethernet/microsoft/mana/mana_en.c"
  [virtio_net]="drivers/net/virtio_net.c"
  [hv_netvsc]="drivers/net/hyperv/netvsc_drv.c"
  [ixgbe]="drivers/net/ethernet/intel/ixgbe/ixgbe_main.c"
  [i40e]="drivers/net/ethernet/intel/i40e/i40e_main.c"
  [ice]="drivers/net/ethernet/intel/ice/ice_main.c"
  [igb]="drivers/net/ethernet/intel/igb/igb_main.c"
  [igc]="drivers/net/ethernet/intel/igc/igc_main.c"
  [mlx5]="drivers/net/ethernet/mellanox/mlx5/core/en_main.c"
  [mlx4]="drivers/net/ethernet/mellanox/mlx4/en_netdev.c"
  [bnxt]="drivers/net/ethernet/broadcom/bnxt/bnxt.c"
  [nfp]="drivers/net/ethernet/netronome/nfp/nfp_net_common.c"
  [stmmac]="drivers/net/ethernet/stmicro/stmmac/stmmac_main.c"
  [otx2]="drivers/net/ethernet/marvell/octeontx2/nic/otx2_pf.c"
  [veth]="drivers/net/veth.c"
  [r8169]="drivers/net/ethernet/realtek/r8169_main.c"
)
ORDER="ena-aws gve-gcp mana-azure virtio_net hv_netvsc ixgbe i40e ice igb igc \
       mlx5 mlx4 bnxt nfp stmmac otx2 veth r8169"

echo "kernel ref: $REF"
echo
printf '%-12s %-9s %-14s %-16s %s\n' driver ndo_bpf ndo_xdp_xmit ndo_xsk_wakeup verdict
printf '%s\n' "---------------------------------------------------------------------------"
for k in $ORDER; do
  out="$D/$k.c"
  code=$(curl -sS -o "$out" -w '%{http_code}' --max-time 45 "$B/${F[$k]}" 2>/dev/null || echo 000)
  if [ "$code" != "200" ]; then
    printf '%-12s %-9s %-14s %-16s %s\n' "$k" "?" "?" "?" "FETCH FAILED http=$code"
    continue
  fi
  bpf=$(grep -c '\.ndo_bpf' "$out")
  xmit=$(grep -c '\.ndo_xdp_xmit' "$out")
  xsk=$(grep -c '\.ndo_xsk_wakeup' "$out")
  b=$([ "$bpf"  -gt 0 ] && echo yes || echo no)
  x=$([ "$xmit" -gt 0 ] && echo yes || echo no)
  z=$([ "$xsk"  -gt 0 ] && echo YES || echo no)
  if   [ "$xsk" -gt 0 ]; then v="native XDP + AF_XDP zero-copy"
  elif [ "$bpf" -gt 0 ]; then v="native XDP, copy-mode AF_XDP only"
  else                        v="no native XDP"; fi
  printf '%-12s %-9s %-14s %-16s %s\n' "$k" "$b" "$x" "$z" "$v"
done

echo
echo "declared xdp_features:"
for k in $ORDER; do
  [ -f "$D/$k.c" ] || continue
  f=$(grep -o 'NETDEV_XDP_ACT_[A-Z_]*' "$D/$k.c" | sort -u | tr '\n' ' ')
  [ -n "$f" ] && printf '  %-12s %s\n' "$k" "$f"
done
