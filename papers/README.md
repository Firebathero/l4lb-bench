# Papers

Nine of nine requested papers are present. None were paywalled in the end, and
no paywall was bypassed: the fetcher accepted a file only if it began with the
`%PDF` magic bytes and exceeded 20 KB, so an ACM Digital Library interstitial or
a login page was rejected rather than saved.

Fetched 2026-08-30.

| file | source URL used | bytes | identity verified by |
|---|---|---|---|
| `2013-sigcomm-ananta.pdf` | https://conferences.sigcomm.org/sigcomm/2013/papers/sigcomm/p207.pdf | 1575803 | extracted text |
| `2014-sigcomm-duet.pdf` | https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/sigcomm14-duet-final.pdf | 1790381 | extracted text |
| `2016-nsdi-maglev.pdf` | https://www.usenix.org/system/files/conference/nsdi16/nsdi16-paper-eisenbud.pdf | 3472645 | canonical USENIX path |
| `2017-sigcomm-silkroad.pdf` | http://minlanyu.seas.harvard.edu/writeup/sigcomm17.pdf | 1323860 | extracted text |
| `2018-arxiv-anchorhash.pdf` | https://arxiv.org/pdf/1812.09674 | 1103678 | extracted text |
| `2018-nsdi-beamer.pdf` | https://www.usenix.org/system/files/conference/nsdi18/nsdi18-olteanu.pdf | 2643810 | canonical USENIX path |
| `2018-nsdi-faild.pdf` | https://www.usenix.org/system/files/conference/nsdi18/nsdi18-araujo.pdf | 897687 | canonical USENIX path |
| `2020-nsdi-cheetah.pdf` | https://www.usenix.org/system/files/nsdi20-paper-barbette.pdf | 843516 | extracted text |
| `2020-socc-concury.pdf` | https://users.soe.ucsc.edu/~qian/papers/Concury-SOCC20.pdf | 893231 | XMP `dc:title` |

## Verification method

Titles were extracted with a stdlib-only Python script (`zlib` over the PDF
content streams plus the Info and XMP metadata dictionaries). No PDF tooling was
installed. Six files yielded a readable title string:

- `2013-sigcomm-ananta.pdf`: "Ananta: Cloud Scale Load Balancing", Patel, Bansal,
  Yuan, Murthy, Greenberg, Maltz, Kern, Kumar, Zikos, Wu, Kim, Karri (Microsoft)
- `2014-sigcomm-duet.pdf`: "Duet: Cloud Scale Load Balancing with Hardware and
  Software", Gandhi, Liu, Hu, Lu, Padhye, Yuan, Zhang (Microsoft, Purdue, Yale)
- `2017-sigcomm-silkroad.pdf`: "SilkRoad: Making Stateful Layer-4 Load Balancing
  Fast and Cheap Using Switching ASICs", Miao (USC), Zeng (Facebook), Kim
  (Barefoot), Lee (Barefoot), Yu (Yale)
- `2018-arxiv-anchorhash.pdf`: "AnchorHash: A Scalable Consistent Hash",
  Mendelson, Vargaftik, Barabash, Lorenz, Keslassy, Orda
- `2020-nsdi-cheetah.pdf`: "A High-Speed Load-Balancer Design with Guaranteed
  Per-Connection-Consistency", Barbette, Tang, Yao, Kostic, Maguire,
  Papadimitratos, Chiesa (KTH)
- `2020-socc-concury.pdf`: "Concury: A Fast and Light-weight Software Cloud Load
  Balancer", Shi, Yu, Xie, Li, Li, Zhang, Qian

Three files (Maglev, Beamer, Faild) embed their body text in subset Type 1 fonts
with a non-ASCII encoding, so no title string could be recovered locally. All
three came from `usenix.org/system/files/...` paths whose filenames encode the
conference and first author (`nsdi16-paper-eisenbud`, `nsdi18-olteanu`,
`nsdi18-araujo`), matching the requested papers. Flagging this as
provenance-verified rather than content-verified.

## Corrections made during fetching

**arXiv 1908.03349 is not Concury.** An early candidate URL guessed that arXiv
ID for the Concury paper. It downloaded successfully as a valid PDF and would
have passed a naive existence check. The abstract page identifies it as "First
Law of Thermodynamics and Emergence of Cosmic Space in a Non-Flat Universe"
(Hareesh, Krishna, Mathew; JCAP 2019), an unrelated cosmology paper. The file
was deleted and replaced from the authors' own copy at
`users.soe.ucsc.edu/~qian/papers/Concury-SOCC20.pdf`. The correct arXiv preprint
for Concury, if a preprint version is wanted later, is **1908.01889**, whose
title differs slightly from the SoCC'20 version: "Concury: A Fast and
Light-weighted Software Load Balancer".

**Duet required four attempts.** These returned HTML 404 pages, not PDFs:

- `https://conferences.sigcomm.org/sigcomm/2014/doc/papers/p27.pdf`
- `https://pages.cs.wisc.edu/~akella/papers/duet-sigcomm14.pdf`
- `https://www.microsoft.com/.../duet-sigcomm14.pdf`
- `https://www.cs.purdue.edu/homes/ychu/publications/sigcomm14_duet.pdf`

The Semantic Scholar API returned `Paper with id DOI:10.1145/2619239.2626317 not
found`. The Microsoft Research publication page carries a free PDF at the URL in
the table above, which is the one used. ACM DL and ResearchGate copies exist but
were not touched.

## Unreachable or paywalled

None.
