# Build requirements, on paper

Nothing in this file was executed. Every command sequence is transcribed from
the repository's own documentation or build files at the pinned SHA in
`repos.md`. Complexity statements are what the docs imply, quoted or
paraphrased, not an assessment.

---

## katran

Pinned at `e6f781e09144641967487f696a9a9f2e2975f4ef`.

### Documented build sequence

`README.md:44-48` gives one instruction:

> To build and install katran library and thrift/gRPC examples - you need to run
> `build_katran.sh` script. It should take care of all the required dependencies.

`DEVELOPING.md` splits it into three separately runnable pieces:

```
# BPF forwarding plane
./build_bpf_modules_opensource.sh
# produces deps/linux/bpfprog/bpf/balancer.bpf.o
#          and deps/linux/bpfprog/bpf/healthchecking_ipip.o

# C++ library
mkdir build && cd build && cmake ..

# tests
cd build/katran/lib/tests && ctest
sudo ./os_run_tester.sh          # BPF tests, needs root
```

CI does not use `build_katran.sh` at all. `.github/workflows/getdeps_linux.yml`
drives Meta's `getdeps.py`:

```
sudo python3 build/fbcode_builder/getdeps.py --allow-system-packages \
     install-system-deps --recursive katran
python3 build/fbcode_builder/getdeps.py --allow-system-packages \
     query-paths --recursive --src-dir=. katran
python3 build/fbcode_builder/getdeps.py --allow-system-packages \
     fetch --no-tests <each dependency>
python3 build/fbcode_builder/getdeps.py --allow-system-packages \
     build --build-type RelWithDebInfo --src-dir=. katran \
     --project-install-prefix katran:/usr/local
python3 build/fbcode_builder/getdeps.py --allow-system-packages \
     test --build-type RelWithDebInfo --src-dir=. katran \
     --project-install-prefix katran:/usr/local
```

Runner: `ubuntu-24.04`. Job timeout: 60 minutes. sccache is enabled.

### Declared dependencies

| dependency | constraint | file |
|---|---|---|
| Linux kernel | 5.6+ | `README.md:40` |
| clang | 6.0+ | `README.md:41,51` |
| CMake | >= 3.9 | `CMakeLists.txt:1` |
| C++ standard | C++17, required | `CMakeLists.txt:22-23` |
| fmt | CONFIG mode, REQUIRED | `CMakeLists.txt:30` |
| folly | CONFIG mode, REQUIRED | `CMakeLists.txt:31` |
| Glog, Gflags | REQUIRED | `CMakeLists.txt:32-33` |
| PkgConfig | REQUIRED | `CMakeLists.txt:35` |
| libzstd | REQUIRED via `pkg_check_modules(ZSTD ...)` | `CMakeLists.txt:36` |
| libevent | `release-2.1.11-stable` (pinned branch) | `build_katran.sh:161` |
| gflags | `v2.2.2` (pinned branch) | `build_katran.sh:193` |
| fast_float | `--depth 1`, unpinned | `build_katran.sh:221` |
| folly | `--depth 1`, unpinned | `build_katran.sh:285` |
| libbpfcc-dev | apt package | `build_katran.sh:117` |
| fbthrift, gRPC | only for the examples | `README.md:54` |
| glog / gtest / gflags / elf | listed for non-Ubuntu distros | `README.md:52` |

Full CI dependency fetch list, `.github/workflows/getdeps_linux.yml`, in the
order the workflow fetches them:

```
ninja  cmake  zlib  zstd  fmt  boost  double-conversion  fast_float  gflags
glog  googletest  libaio  libdwarf  libevent  lz4  openssl  snappy  liboqs
autoconf  automake  libtool  libmnl  libiberty  libsodium  libunwind  xz
folly  fizz  libelf  libbpf
```

No version pins appear in the workflow; `getdeps.py` resolves them from
`build/fbcode_builder/manifests/`.

### Kernel, BTF, and privilege requirements

- kernel 5.6+ (`README.md:40`)
- XDP driver mode for full performance; generic XDP works "with some performance
  degradation" (`README.md:133-137`)
- the BPF build wants a Linux source tree at `deps/linux/`, which
  `build_katran.sh` installs there (`DEVELOPING.md`)
- `os_run_tester.sh` requires root (`DEVELOPING.md`)
- no explicit BTF or CO-RE requirement is stated in the docs

### DPDK

Not applicable. katran does not use DPDK.

### Hugepages / IOMMU / NIC / root

Docs mention no hugepages and no IOMMU. NIC relevance is XDP driver-mode
support. Root is needed for BPF map and program operations and for the test
runner.

### Complexity as the docs imply

`README.md:48` says the script "should take care of all the required
dependencies"; `README.md:56` adds that Meta runs CI on CentOS and does "our
best to support OSS build on recent Ubuntu versions". The CI job fetches and
builds 30 dependencies from source with a 60 minute timeout.

---

## cilium

Pinned at `fa4c8b8e192e5d18d0baed648d977c766afea60a`, `VERSION` = `1.21.0-dev`.

### Documented build sequence

The BPF datapath is built through `bpf/Makefile`, whose targets include
`build_all`. The toolchain comes from the builder image rather than the host.
`images/builder/Dockerfile:84-90`:

> clang compiles the BPF datapath in the builder (e.g. 'make -C bpf build_all',
> [...] llvm-objcopy/llvm-strip are used by the BPF Go tests (pkg/bpf/testdata).
> llc is intentionally not copied: the BPF build is clang-only, and the datapath
> verifier tests extract the toolchain from the cilium-llvm image (see
> contrib/scripts/extract-llvm.sh), not from this image.

### Declared dependencies

| dependency | constraint | file |
|---|---|---|
| Go | `go 1.26.0` | `go.mod:3` |
| Go toolchain image | `docker.io/library/golang:1.27.0` (digest-pinned) | `images/builder/Dockerfile:8` |
| LLVM/clang image | `quay.io/cilium/cilium-llvm:19.1.7-1785833026-d8383c5` (digest-pinned) | `images/builder/Dockerfile:9` |
| base image | `docker.io/library/ubuntu:26.04` (digest-pinned) | `images/builder/Dockerfile:6` |
| test image | `quay.io/cilium/image-tester:1787138071-3b8cd9a` | `images/builder/Dockerfile:7` |
| clang, llvm-objcopy, llvm-strip | copied from the LLVM image | `images/builder/Dockerfile:90` |
| CGO | `CGO_ENABLED ?= 0`, forced to 1 for `-race` | `Makefile.defs:205,232-233` |
| delve | `go install github.com/go-delve/delve/cmd/dlv@latest` | `images/builder/Dockerfile:61` |

The BPF objects built are `bpf_lxc.o bpf_overlay.o bpf_sock.o bpf_host.o
bpf_wireguard.o bpf_xdp.o bpf_alignchecker.o` (`bpf/Makefile:6-7`), with
`KERNEL ?= netnext` (`bpf/Makefile:9`).

`bpf/Makefile` also enumerates the compile-tested `-D` option combinations under
`LB_OPTIONS`, and a `MAX_BASE_OPTIONS` set described in the file as "intended to
max out the BPF program complexity", load tested.

### Kernel version, kernel config, BTF

From `Documentation/operations/system_requirements.rst`:

- line 23 and line 40: `Linux kernel >= 5.10 or equivalent (e.g., 4.18 on RHEL 8.10)`

Required config, lines 157-170:
```
CONFIG_BPF=y            CONFIG_BPF_EVENTS=y     CONFIG_BPF_SYSCALL=y
CONFIG_NET_CLS_BPF=y    CONFIG_BPF_JIT=y        CONFIG_NET_CLS_ACT=y
CONFIG_NET_SCH_INGRESS=y                        CONFIG_DEBUG_INFO_BTF=y
CONFIG_CRYPTO_SHA1=y    CONFIG_CRYPTO_USER_API_HASH=y
CONFIG_CGROUPS=y        CONFIG_CGROUP_BPF=y
CONFIG_PERF_EVENTS=y    CONFIG_SCHEDSTATS=y
```

Further optional groups in the same file: iptables/ipset (lines 181-184),
tunnelling `CONFIG_VXLAN=y CONFIG_GENEVE=y CONFIG_FIB_RULES=y` (195-197),
L7/proxy `NETFILTER_XT_TARGET_TPROXY/MARK/CT`, `XT_MATCH_MARK/SOCKET`
(225-229), and IPsec `CONFIG_XFRM*`, `CONFIG_INET{,6}_ESP` etc. (259-268).

`CONFIG_DEBUG_INFO_BTF=y` is explicitly required, so BTF is a hard requirement.

### DPDK

Not applicable.

### Hugepages / IOMMU / NIC / root

No hugepage or IOMMU requirement documented. XDP acceleration is gated on driver
support (`ENABLE_NODEPORT_ACCELERATION` in `bpf/Makefile` option sets). Cilium
runs as a privileged agent.

### Complexity as the docs imply

The build is containerised end to end, with every toolchain image pinned by
digest. The documented path assumes Docker/BuildKit is available.

---

## dpvs

Pinned at `4582d20dc6cc16ab123c3b72e245e3ec9a47965b`, tagged **v1.10.2**.

### Documented build sequence

Verbatim from `README.md`.

DPDK, lines 93-95 and 119-127:
```
$ wget https://fast.dpdk.org/rel/dpdk-24.11.tar.xz
$ tar xf dpdk-24.11.tar.xz

$ cd dpdk-24.11
$ mkdir dpdklib                 # user desired install folder
$ mkdir dpdkbuild               # user desired build folder
$ meson -Denable_kmods=true -Dprefix=dpdklib dpdkbuild
$ ninja -C dpdkbuild
$ cd dpdkbuild; ninja install
$ export PKG_CONFIG_PATH=$(pwd)/../dpdklib/lib64/pkgconfig/
```

Patches, lines 104-110:
```
$ cd <path-of-dpvs>
$ cp patch/dpdk-24.11/*.patch dpdk-24.11/
$ cd dpdk-24.11/
$ patch -p1 < 0001-pdump-add-cmdline-packet-filters-for-dpdk-pdump-tool.patch
$ patch -p1 < 0002-debug-enable-dpdk-eal-memory
$ patch -p1 < 0003-ixgbe_flow-patch-ixgbe-fdir-rte_flow-for-dpvs.patch
$ ...
```
Line 113 adds: "It's advised to patch all if your are not sure about what they
are meant for."

DPVS itself, lines 183-188:
```
$ export PKG_CONFIG_PATH=<path-of-libdpdk.pc>  # normally dpdklib/lib64/pkgconfig/
$ cd <path-of-dpvs>
$ make              # or "make -j" to speed up
$ make install
```

`scripts/dpdk-build.sh` is offered as a helper (`README.md:129`); its default is
`dpdkver=24.11` (`scripts/dpdk-build.sh:8`).

CI, `.github/workflows/build.yaml`, is just:
```
PKG_CONFIG_PATH: /data/dpdk/24.11/dpdklib/lib64/pkgconfig
run: make -j
```
and `.github/workflows/run.yaml` adds `make install`. The LTS workflows
(`build-lts.yaml`, `run-lts.yaml`) use `PKG_CONFIG_PATH: /data/dpdk/20.11.10/dpdklib/lib64/pkgconfig`.

### Declared dependencies

| dependency | constraint | file |
|---|---|---|
| **DPDK** | **24.11** recommended for v1.10; nothing earlier than 20.11 supported | `README.md:81,85` |
| DPDK (CI, current) | `24.11` | `.github/workflows/build.yaml:27`, `run.yaml:27` |
| DPDK (CI, LTS) | `20.11.10` | `.github/workflows/build-lts.yaml:22`, `run-lts.yaml:22` |
| DPDK (helper script default) | `24.11` | `scripts/dpdk-build.sh:8` |
| meson, ninja | unversioned | `README.md:123-125` |
| pkg-config | implied by `PKG_CONFIG_PATH` and `src/dpdk.mk` | `README.md:117,126` |
| GCC | 8.5 in the verified environment | `README.md:65` |

Version compatibility table, `README.md:83-88`:

| DPVS version | DPDK version |
|---|---|
| v1.10 | 24.11 |
| v1.9 | 20.11 |
| v1.8 | 18.11 |
| v1.7 or earlier | 17.11 or earlier |

### Kernel version and modules

`README.md:63-68` states DPVS "relies very little on operating system, kernel
versions, compilers" and lists verified environments:

- Anolis 8.6, 8.8, 8.9
- GCC 8.5
- Kernel 3.10.0, 4.18.0, 5.10.134
- NIC: Intel IXGBE, NVIDIA MLX5
- CentOS 7.x and GCC 4.8 for versions earlier than v1.10

No BTF requirement (DPVS is userspace DPDK, not BPF).

`rte_kni.ko` was required for DPDK 18.11 through 23.11 and must be loaded with
`carrier=on` (`README.md:175`). KNI was removed in DPDK 23.11, and DPVS replaced
it with virtio-user since v1.10, enabled via `CONFIG_KNI_VIRTIO_USER` in
`config.mk` (`README.md:161,176`).

### Hugepages, IOMMU, NIC drivers, root

Hugepages, `README.md:135-136`:
```
$ echo 8192 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
$ echo 8192 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
```
`README.md:141-148` expects hugetlbfs at `/dev/hugepages`, else mount it
manually. `README.md:153` recommends, for production, kernel cmdline
`isolcpus=1-9 default_hugepagesz=1G hugepagesz=1G hugepages=32`.

NIC binding, `README.md:158,164-169`:
```
$ modprobe uio_pci_generic
$ ./usertools/dpdk-devbind.py --status
$ ifconfig eth0 down
$ ./usertools/dpdk-devbind.py -b uio_pci_generic 0000:06:00.0
```
`vfio-pci`, `igb_uio`, and `uio_pci_generic` are all named as options
(`README.md:158`). Mellanox uses a bifurcated driver and must **not** be bound,
but needs MLNX_OFED (`README.md:174`).

`rte_flow` support is required for FNAT and SNAT on multiple cores unless
`conn redirect` is enabled, and must cover "ipv4, ipv6, tcp, udp" items with
"drop, queue" actions (`README.md:62`).

Root is required for modprobe, devbind, hugepage reservation, and `make install`.

### Complexity as the docs imply

The README walks through DPDK download, patching, meson build, install,
hugepage reservation, module loading, and NIC binding before DPVS itself is
touched. It also notes that at least 2 NICs are needed for two-arm testing
(`README.md:172`).

---

## DPDK

Pinned at `d55ccd4e6de64e3f797f60de9e81f1d60f849775`. `VERSION` = `26.11.0-rc0`.

### Documented build sequence

`meson.build:13` sets `meson_version: '>= 0.57.2'`. `pktgen-dpdk/INSTALL.md:84-88`
transcribes the canonical sequence:
```
cd dpdk
meson setup build
ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### Declared dependencies

From `doc/guides/linux_gsg/sys_reqs.rst`:

| dependency | constraint |
|---|---|
| C compiler supporting C11 including standard atomics | GCC 8.0+ recommended, or Clang 7+ recommended |
| `pkg-config` or `pkgconf` | required for building end-user binaries against DPDK |
| Python | 3.6 or later |
| Meson | version 0.57+ |
| ninja | unversioned |
| `pyelftools` | version 0.22+ |
| NUMA library | `libnuma-dev` (Debian/Ubuntu), `numactl-devel` (RHEL/Fedora) |
| libarchive | optional, for unit tests that use tar |
| libelf | optional, "to compile and use the bpf library" |

`meson.build:13` requires meson `>= 0.57.2`, marginally stricter than the
`0.57+` in the prose.

CI dependency list, `.github/workflows/build.yml`, `build_deps`:
```
ccache libarchive-dev libbsd-dev libbpf-dev libfdt-dev libibverbs-dev
libipsec-mb-dev libisal-dev libjansson-dev libnuma-dev libpcap-dev libssl-dev
libvirt-dev ninja-build pkg-config python3-pip python3-pyelftools
python3-setuptools python3-wheel zlib1g-dev
```
CI runner for checkpatch: `ubuntu-24.04`. The build matrix covers cross targets
`aarch64`, `i386`, `mingw`, `ppc64le`, `riscv64`, and check variants
`abi`, `asan`, `ubsan`, `debug`, `doc`, `examples`, `tests`, `stdatomic`, `mini`.

### Kernel, hugepages, IOMMU, NIC, root

`sys_reqs.rst` notes that for the majority of x86 platforms no special BIOS
settings are needed, but HPET timer, power management, and "high performance of
small packets" may require BIOS changes, referring to `enable_func`. Hugepage
and driver-binding requirements live in `linux_gsg/linux_drivers.rst` and
`sys_reqs.rst` sections not reproduced here; DPVS and pktgen both restate them.

### Complexity as the docs imply

`sys_reqs.rst` describes the required set as ordinary distro development
packages plus meson/ninja, with per-PMD extra dependencies auto-detected:
"the presence or absence of these dependencies will be automatically detected
enabling or disabling the relevant components appropriately."

---

## pktgen-dpdk

Pinned at `1f052ac714168b29955e6e9bd846ab4073eb94b2`.

### Documented build sequence

`INSTALL.md:102-115`:
```
git clone https://github.com/pktgen/Pktgen-DPDK
cd pktgen-dpdk
make
or
make build    # Same as 'make'
or
make rebuild  # removes the 'builddir' then builds it again via meson/ninja
or
make rebuildlua # to enable Lua builds
```
preceded by `export PKG_CONFIG_PATH=/usr/local/lib/x86_64-linux-gnu/pkgconfig`
(`INSTALL.md:94`) and `sudo apt-get install libbsd-dev` (`INSTALL.md:74`).

Runtime setup, `INSTALL.md:162-169`:
```
export RTE_SDK=<DPDKinstallDir>
export RTE_TARGET=x86_64-native-linux-gcc
sudo ./tools/setup.sh          # run as root once per boot
```

### Declared dependencies

| dependency | constraint | file |
|---|---|---|
| DPDK | **no pinned version**; "Please use the latest DPDK and latest Pktgen versions other combinations may work, but with limited resources and time the latest versions are the only ones tested" | `INSTALL.md:59` |
| `libdpdk.pc` | "At least a libdpdk.pc file must be present in the system" | `INSTALL.md:66` |
| meson | **`>= 0.58.0`** | `meson.build:11` |
| ninja | unversioned | `INSTALL.md:69` |
| libbsd | `libbsd-dev` | `INSTALL.md:74` |
| libpcap | `libpcap-devel` on CentOS via PowerTools | `INSTALL.md:215-221` |
| clang-format | 21, lint only | `.github/workflows/clang-format.yml` |
| sphinx, meson, ninja-build | docs only | `.github/workflows/doc.yml` |
| markdownlint-cli2, Node 20 | lint only | `.github/workflows/markdownlint.yml` |

`meson.build:6-10` sets `buildtype=release`, `default_library=static`,
`warning_level=3`, `werror=true`, and adds `-march=native` on non-riscv64,
non-aarch64 targets (`meson.build:24-26`).

There are **no build CI workflows**. The three that exist cover clang-format,
docs deployment, and markdown lint. There is no workflow that compiles pktgen,
so CI cannot be used to cross-check the documented build here.

### Documented OS and kernel

`INSTALL.md:48`: "Ubuntu 22.04 to 23.10 desktop it should work on most Linux
systems as long as the kernel has hugeTLB page support and builds DPDK."
`INSTALL.md:58`: "Tested with Ubuntu 23.10 kernel version 6.5.0-28-generic
(Mantic), and other earlier versions should work."

### Hugepages, IOMMU, NIC drivers, root

- hugeTLB page support required (`INSTALL.md:48`); `tools/setup.sh` sets up
  hugepages and "the two echo commands [...] finish setting up the huge pages
  one for each socket" (`INSTALL.md:173-177`)
- `INSTALL.md:121-122`: "If you want to use vfio-pci then edit
  /etc/default/grub and add 'intel_iommu=on' to the LINUX default line
  Then use 'update-grub' command then reboot the system."
- `modprobe uio` plus `igb-uio.ko` loaded by `tools/setup.sh` (`INSTALL.md:173`)
- setup script must run as root once per boot (`INSTALL.md:169`)
- `INSTALL.md:117-119` warns DPDK installs no `/etc/ld.so.conf.d` entry, so
  `/usr/local/lib/x86_64-linux-gnu` may need adding manually before `ldconfig`

### Internal inconsistency in the docs, flagged not resolved

`INSTALL.md:65` states "Pktgen has been converted to use meson/ninja for
configuration and building. **Makefiles have been removed.**" Later, lines
197-202 of the same file instruct:
```
cd $RTE_SDK
make install T=x86_64-native-linux-gcc -j
```
which is the pre-meson DPDK build command removed from DPDK in 20.11. The two
halves of `INSTALL.md` describe different build systems. Recorded as-is.

### Complexity as the docs imply

`INSTALL.md:97-100` says the `make` wrapper exists "to help build Pktgen without
having to fully understand meson/ninja command line".

---

## trex-core

Pinned at `27e0153b5ff833c51d48f1625ace979a2868d8a0`.

### Documented build sequence

**Not in the repository.** `README.asciidoc:163-165` is:

```
=== How to build

Internal link:https://github.com/cisco-system-traffic-generator/trex-core/wiki[Wiki]
```

The build instructions live on the GitHub wiki, which is not part of the git
clone and was not fetched. What can be read from the tree itself:

- `linux_dpdk/` contains a vendored waf (`waf-2.0.21`), `wscript`, and
  `ws_main.py`, so the build is waf-driven
- `linux_dpdk/b` and `linux_dpdk/bcov` are the driver scripts
- `src/dpdk/` holds a bundled DPDK tree (`auto-config-h.sh`, `drivers/`, `lib/`)

### Declared dependencies

| dependency | constraint | file |
|---|---|---|
| GCC | selectable 6.2 / 7.4 / 8.3 via `--gcc6` / `--gcc7` / `--gcc8`; searched under `/usr/local/gcc-N` and `/opt/rh/devtoolset-N` | `linux_dpdk/ws_main.py:69-71,164-171` |
| clang | alternate flag set present | `linux_dpdk/ws_main.py:44` |
| python | waf is python | `linux_dpdk/ws_main.py:1` |
| Mellanox OFED | version-checked at configure time; `--no-mlx` to skip | `linux_dpdk/ws_main.py:697-716` |
| DPDK | **bundled in-tree at `src/dpdk/`**, no external version constraint stated | tree layout |

### NIC requirements

`README.asciidoc:46`: "Support Physical DPDK 1/2.5/10/25/50/40/100Gbps
interfaces (Broadcom/Intel/Mellanox/Cisco VIC/Napatech/Amazon ENA)".

### Complexity as the docs imply

Cannot be characterised from the repository, because the build documentation is
off-repo. Flagged as a gap rather than guessed at.

---

## AnchorHash (`anchorhash-cpp`)

Pinned at `3ef98f05cbfe1a449f92b97cdfb1363317db85e1`.

### Documented build sequence

`README.md`, section "Try it", in full:

> Go into the `tests\speed` and `tests\balance` directories, run make, run the
> python script, and plot

So: `cd tests/speed && make`, and `cd tests/balance && make`. Makefiles exist at
`tests/speed/Makefile`, `tests/balance/Makefile`, and `mem/speed/Makefile`.

### Declared dependencies

| dependency | constraint | file |
|---|---|---|
| SSE4.2 `CRC32` instruction | required, "You can replace it in `misc/crc32c_sse42_u64.h`" | `README.md`, "System Requirements" |
| make + C++ compiler | implied by the Makefiles | `tests/*/Makefile` |
| python | for `speed_test.py`, `balance_test.py`, `speed_test_plots.py` | file names |

No kernel, BTF, DPDK, hugepage, IOMMU, or root requirement. The `mem/` directory
holds a lower-memory variant with its own README.

### Complexity as the docs imply

Two Makefiles and a plotting script. No dependency management of any kind.

---

## Beamer

Pinned per `repos.md`. The artifact is six repositories plus three external
forks.

### `beamer-mod` (kernel module)

`Makefile`, in full:
```
# TODO: KDIR

KDIR		:= ../mptcp
PWD		:= $(shell pwd)

kbuild:
	make -C $(KDIR) M=$(PWD)

modules: kbuild
	make -C $(KDIR) M=$(PWD) modules

install:
	make -C $(KDIR) M=$(PWD) modules_install
	depmod -a

clean:
	make -C $(KDIR) M=$(PWD) clean
	-rm -f Module.symvers *~
```

`Kbuild`:
```
obj-m  := beamer.o
beamer-y := beamer_main.o beamer_hooks.o beamer_bucket_table.o beamer_srv.o \
            beamer_tcpopt.o beamer_sysfs.o beamer_pm.o beamer_hook_utils.o \
            beamer_p4_crc32.o beamer_gen.o
```

Dependency: `KDIR := ../mptcp` is a **hard path dependency on the sibling
checkout `Beamer-LB/mptcp`**, a Linux kernel fork. It is not a generic
`/lib/modules/$(uname -r)/build` reference, so the module is built against
Beamer's own kernel, not the running one. The `# TODO: KDIR` comment on line 1
is the authors' own note that this is hardcoded.

### `beamer-click` (Click elements)

No Makefile, no build system, no README. Files are `beamermux.{cc,hh}`,
`statefulmux.{cc,hh}`, and `lib/` containing `tcpopt`, `dumper`, `p4crc32`,
`dipmap`, `zkclient`, `ggencapper`. `zkclient` implies a ZooKeeper client
dependency. The elements are intended to be compiled inside a Click or FastClick
tree; `Beamer-LB/fastclick` exists in the org.

### `beamer-ctrl` (control plane)

Java, NetBeans/Ant project: `build.xml` importing `nbproject/build-impl.xml`,
plus `manifest.mf`. Build command is `ant`. No dependency manifest was read.

### `beamer-p4`

A single file, `beamer.p4.php`. The `.php` extension implies the P4 source is
generated by running it through PHP before compiling with a P4 compiler. No
build documentation.

### `beamer-doc`

`README.md` in full:
```
Please see the [wiki](https://github.com/Beamer-LB/beamer-doc/wiki).
===========================
```
The wiki is not in the clone.

### Complexity as the docs imply

There is no build documentation inside any of the six cloned repositories. The
only machine-readable build files are `beamer-mod/Makefile` (pointing at an
uncloned kernel fork) and `beamer-ctrl/build.xml`.

---

## DPDK version constraints across repos

Reported per repo. **Not resolved.**

| consumer | required DPDK | source |
|---|---|---|
| dpvs v1.10 | **24.11** exactly, with in-repo patches under `patch/dpdk-24.11/` | `README.md:81,85,105`; `scripts/dpdk-build.sh:8` |
| dpvs CI (current) | 24.11 | `.github/workflows/build.yaml:27` |
| dpvs CI (LTS) | 20.11.10 | `.github/workflows/build-lts.yaml:22` |
| dpvs, floor | nothing earlier than 20.11 supported | `README.md:81` |
| pktgen-dpdk | "the latest DPDK", no pin | `INSTALL.md:59-61` |
| trex-core | bundled in-tree at `src/dpdk/`, no external pin | tree layout |
| DPDK clone in `repos/dpdk` | **26.11.0-rc0** | `dpdk/VERSION` |

Conflicts, stated not resolved:

1. **dpvs requires 24.11; the cloned DPDK is 26.11.0-rc0.** These are different
   release series. dpvs also ships patches directory-named for 24.11, which
   would need to apply cleanly against whatever tree is used.
2. **dpvs pins a version; pktgen asks for the latest.** A single shared DPDK
   install cannot satisfy "exactly 24.11" and "latest" at the same time unless
   24.11 happens to be latest, which it is not at this clone date.
3. **trex-core bundles its own DPDK**, so it neither consumes nor conflicts with
   a system-wide DPDK, but it also will not share one.
4. **Meson floor differs**: DPDK requires `>= 0.57.2` (`dpdk/meson.build:13`),
   pktgen requires `>= 0.58.0` (`pktgen-dpdk/meson.build:11`). The stricter of
   the two governs a combined build.
5. The cloned DPDK is a **release candidate** (`26.11.0-rc0`), not a released
   version. No repo documents a constraint against an rc.
