# Cloned repositories

All clones are `git clone --depth 1` of the default branch. Clone date for every
entry below: **2026-08-30T12:20:06Z**. Nothing has been modified, built, or
submodule-initialised since. `describe` is `git describe --tags --always`, which
in a shallow clone resolves to the short SHA unless a tag happens to sit on HEAD.

Root: `~/l4lb-bench/repos/`

## Requested targets

| dir | upstream URL | commit SHA | branch | describe | HEAD commit date | size |
|---|---|---|---|---|---|---|
| `katran` | https://github.com/facebookincubator/katran | `e6f781e09144641967487f696a9a9f2e2975f4ef` | main | e6f781e | 2026-08-29T09:33:30-07:00 | 9.9M |
| `cilium` | https://github.com/cilium/cilium | `fa4c8b8e192e5d18d0baed648d977c766afea60a` | main | fa4c8b8e | 2026-08-30T08:47:33+00:00 | 465M |
| `dpvs` | https://github.com/iqiyi/dpvs | `4582d20dc6cc16ab123c3b72e245e3ec9a47965b` | master | **v1.10.2** | 2025-06-23T09:57:04+08:00 | 19M |
| `dpdk` | https://github.com/DPDK/dpdk | `d55ccd4e6de64e3f797f60de9e81f1d60f849775` | main | d55ccd4 | 2026-08-24T17:51:07+02:00 | 153M |
| `pktgen-dpdk` | https://github.com/pktgen/Pktgen-DPDK | `1f052ac714168b29955e6e9bd846ab4073eb94b2` | main | 1f052ac | 2026-07-28T07:12:48-05:00 | 22M |
| `trex-core` | https://github.com/cisco-system-traffic-generator/trex-core | `27e0153b5ff833c51d48f1625ace979a2868d8a0` | master | 27e0153 | 2025-11-05T15:55:45+02:00 | 418M |

`dpvs` is the only clone whose HEAD carries a release tag: **v1.10.2**.

`dpdk/VERSION` on the cloned commit reads **`26.11.0-rc0`**. This is a
pre-release of the main development branch, not a released version. See
`build-requirements.md` for the per-repo DPDK version constraints and the
conflict this creates.

## AnchorHash reference implementation

The task said to search GitHub for the code accompanying the AnchorHash paper by
Mendelson, Vargaftik et al.

**Search performed:** GitHub REST API, `GET /users/anchorhash/repos` and
`GET /orgs/anchorhash/repos`. The `orgs` form returned 404 (it is a user
account, not an org); the `users` form returned four repositories. Direct URL
probes were also run against candidate names.

URL used as the primary reference implementation:
**https://github.com/anchorhash/cpp-anchorhash**

Its own README self-describes as "AnchorHash - A Scalable Consistent Hash" and
cites the paper DOI `10.1109/TNET.2020.3039547`, so the attribution is confirmed
from the repository itself rather than inferred from the name.

| dir | upstream URL | commit SHA | branch | HEAD commit date | size |
|---|---|---|---|---|---|
| `anchorhash-cpp` | https://github.com/anchorhash/cpp-anchorhash | `3ef98f05cbfe1a449f92b97cdfb1363317db85e1` | main | 2021-12-05T13:09:29+02:00 | 304K |
| `anchorhash-go` | https://github.com/anchorhash/go-anchorhash | `32e170e5fb77e2e735aa7a984a7317a255cb18dd` | main | 2020-11-19T11:24:00+02:00 | 744K |
| `anchorhash-py` | https://github.com/anchorhash/py-anchorhash | `75d8cfc99565775bfa5dc698853605ead903d073` | main | 2020-11-29T16:24:03+02:00 | 228K |

The Go and Python clones are the same authors' ports, linked from the C++
README. They are cloned for reference; the C++ repo is the one the paper's
evaluation figures were produced with, per its README.

A fourth repository exists under the same account and was **not** cloned:
`https://github.com/anchorhash/jetlb` ("Load Balancing with JET: Just Enough
Tracking for Connection Consistency"). It is a different paper, flagged here
rather than pulled in.

`anchorhash-cpp` README states the implementation uses the `CRC32` instruction
from SSE4.2, replaceable in `misc/crc32c_sse42_u64.h`.

## Beamer NSDI'18 artifact

**Status: still reachable.** `https://github.com/Beamer-LB` returns HTTP 200 and
the org API lists nine repositories, all last pushed February to March 2018,
consistent with NSDI'18.

There is **no single repository named `beamer`**. `https://github.com/Beamer-LB/beamer`
returns 404. The artifact is split across the org. This is a structural fact
about the artifact, not a substitution.

Cloned (six of nine):

| dir | upstream URL | commit SHA | branch | HEAD commit date | size |
|---|---|---|---|---|---|
| `beamer-ctrl` | https://github.com/Beamer-LB/beamer-ctrl | `8436afdc6b15d7eb36191075b783b276c26272c4` | master | 2018-02-22T01:23:07+02:00 | 584K |
| `beamer-click` | https://github.com/Beamer-LB/beamer-click | `a11c05f2e610e51cbcfd0b29c0ff88c7f6234db8` | master | 2018-02-22T02:49:09+02:00 | 332K |
| `beamer-p4` | https://github.com/Beamer-LB/beamer-p4 | `d6b6148a9c83aee03362094e0084b81cdfd20f53` | master | 2018-02-22T02:33:36+02:00 | 208K |
| `beamer-mod` | https://github.com/Beamer-LB/beamer-mod | `146c350349b83f2a491fa543fd490696d5ed70a4` | master | 2018-02-25T17:24:12+02:00 | 352K |
| `beamer-doc` | https://github.com/Beamer-LB/beamer-doc | `188a6f51f03d7062720bffa4a4ef370e7917b804` | master | 2018-03-09T19:18:07+02:00 | 200K |
| `beamer-clickityclack` | https://github.com/Beamer-LB/clickityclack | `3c54d42114d6dc45755eaa351381ccbb5173e96a` | master | 2018-02-22T02:16:28+02:00 | 396K |

Not cloned, listed for completeness. These three are forks of external projects
that Beamer builds against rather than Beamer's own code:

- `https://github.com/Beamer-LB/mptcp` (Linux kernel fork; `beamer-mod/Makefile`
  hardcodes `KDIR := ../mptcp`, so it is a build dependency of `beamer-mod`)
- `https://github.com/Beamer-LB/fastclick` (FastClick fork; `beamer-click`
  contains Click elements with no in-repo build system)
- `https://github.com/Beamer-LB/netmap`

`beamer-doc/README.md` contains only a pointer to
`https://github.com/Beamer-LB/beamer-doc/wiki`. The wiki content is not in the
git clone and was not fetched.

## Nothing was unfindable

Every item on the task list was located. No substitutions were made.

## Total

15 clones, 1.1 GB on disk.
