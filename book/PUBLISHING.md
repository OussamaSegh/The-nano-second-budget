# The Nanosecond Budget: publishing kit

**Title:** The Nanosecond Budget
**Subtitle:** Fast, Correct Concurrent Data Structures in Modern C++
**Author:** Oussama Seghaier *(confirm)*
**Edition:** First edition, version 1.0, November 2026 *(confirm month)*
**Language:** English · **Interior:** 344 pages, black and white · **Trim:** 7.5 × 9.25 in

## Files

| File | Use |
|---|---|
| `print/interior.pdf` | Paperback interior. 344 pages at 7.5 × 9.25 in, no bleed, all fonts embedded (TrueType, no Type 3), grayscale. 15 figures and a 202-entry index. Upload to KDP Print or IngramSpark. |
| `print/cover-wrap.pdf` | Paperback cover: back, spine and front with 0.125 in bleed. 16.025 × 9.50 in, spine 0.775 in (344 pages on white paper). A white 2 × 1.2 in box at bottom right of the back is kept free for the barcode. |
| `print/cover-wrap-preview.png` | Preview of the full wrap. |
| `the-nanosecond-budget.epub` | Ebook for Kindle (KDP accepts EPUB), Apple Books, Kobo and Google Play. EPUB 3, passes EPUBCheck 5.1 with no errors or warnings, and the DAISY Ace accessibility check (EPUB Accessibility 1.1 metadata, alt text on every figure). |
| `cover.jpg` | Ebook cover, 1600 × 2560 px JPEG (the size KDP recommends). |
| `the-nanosecond-budget.pdf` | Reading PDF with cover, bookmarks and links, for direct sale (Leanpub, Gumroad, your own site). |
| `llds-book-code.zip` | Companion code (MIT). Needs a public home: see placeholder 1. |
| `source/` | Manuscript and build scripts. `build-book/edition.yaml` holds every name, ISBN and spec. |

## Before you upload: placeholders to fill in

All of these live in `source/build-book/edition.yaml`. Change them and rebuild with
`source/build-book/build.sh source/manuscript <out-dir>`:

1. **Code URL (`code_url`). Blocker.** The preface, copyright page and back cover tell readers where to get the code, and currently print "[code URL to be added]". Publish `llds-book-code.zip` as a public repository (for example on GitHub), put its URL here and rebuild.
2. **Author name** as it should be printed.
3. **ISBNs.** Paperback and ebook need different ISBNs. On KDP you can take a free KDP ISBN for the paperback (the imprint then reads "Independently published"), and the Kindle edition needs no ISBN. IngramSpark, or wide distribution under your own imprint, needs ISBNs you buy (in Switzerland from the national agency; in the US from Bowker). Until you fill them in, the copyright page says "to be assigned".
4. **Publisher or imprint name.**
5. **Author bio.** If you leave it empty, the book has no About the Author page and the back cover has no bio.
6. **Website** for errata (optional).
7. **Paper colour.** On cream paper the spine is wider (0.860 in). Rebuild the cover if you switch.

8. **Author's voice.** A few passages speak in the first person or as "the author" (the Acknowledgements, and "A result that surprised the author" in Chapter 8). Make them yours or cut them.

If the page count changes, rebuild. The cover script works out the spine width from the interior.

## Store listing

**Description** (paste into KDP, IngramSpark, Apple and Kobo; also in `source/build-book/description.txt`):

> A trading gateway has a few microseconds from packet to order. A bedside monitor must raise its alarm within a bounded time, every time, for years. A PET scanner streams millions of detector events a second and may not lose one without saying so. A mutex and a standard container will not get you there. Knowing what the cache, the memory model and the scheduler really do will.
>
> *The Nanosecond Budget* builds every structure from scratch in C++20, explains it line by line, tests it under ThreadSanitizer and measures it with a harness that tells the truth about tail latency. You will learn to:
>
> - read latency as a distribution and measure p99.9 without fooling yourself
> - use the C++ memory model, atomics and release/acquire with confidence
> - build spinlocks, seqlocks, SPSC rings, MPMC queues and a Disruptor-style broadcast ring
> - take malloc off the hot path, and reclaim memory safely with hazard pointers and epochs
> - design concurrent hash maps, skip lists and order books
> - tune Linux for low jitter, and verify concurrent code with sanitizers and model checking
> - apply it all to a real-time PET data-acquisition pipeline with coincidence sorting
>
> Eight Foundations chapters bring newcomers to threads up to speed. Every chapter ends with exercises, all solved in full, and the book comes with llds, a complete header-only companion library with tests and benchmarks.

**Seven KDP keywords:** C++20 concurrency · lock-free data structures · low latency programming · C++ memory model atomics · multithreading performance · real-time systems Linux · medical imaging software PET

**BISAC categories:**
- COM051310 COMPUTERS / Languages / C++
- COM051230 COMPUTERS / Software Development & Engineering / General
- COM051440 COMPUTERS / Software Development & Engineering / Tools

**Audience:** professional and advanced learners, adult. **Contains AI-generated content:** KDP asks about this when you publish. Answer it truthfully for the text, the code and the cover.

## Print specification

- Trim 7.5 × 9.25 in (190.5 × 235 mm). KDP and IngramSpark both offer it.
- Margins: inside 0.875 in, outside 0.625 in, top 0.9 in, bottom 0.8 in. These meet KDP's minimum gutter for 301 to 500 pages.
- Interior: black ink, white paper, no bleed. Code is typeset in grayscale, so the cheaper B&W print tier applies.
- Front matter is numbered i to xxii in roman numerals. The body starts at page 1 with Part 0. Parts, chapters and front-matter sections open on right-hand pages.
- Fonts: Source Serif 4, Inter and JetBrains Mono, all under the SIL Open Font License, which allows embedding in commercial books.


## Changes since the independent review (2026-10-26)

- Code: hazard-pointer `protect()` now validates with a seq_cst load (the acquire load could be reordered before the slot store on ARM and POWER). Epoch reclamation now reuses records when threads exit, so the 129th thread no longer crashes; leftover nodes go to an orphan list. Thread-limit checks abort in every build. New tests start 1,000 short-lived threads; flaky tests made deterministic.
- Verified: 52 of 52 library tests pass with GCC Release, Clang, TSan and ASan, plus 25 Release and 8 TSan repeat runs; the exercise starters still fail as intended.
- Book: matching text in Chapter 12 and Appendix D, 15 new figures, a curated index, compiler requirements (GCC 13+, or Clang 17+ with libstdc++), corrected timing figures, full serif font (no fallback glyphs).
- Not done: the reviewer suggested 20 to 30 figures and a 600+ entry index; the book now has 15 and about 200. Chapters 8 to 22 have not had a line-by-line copy edit.
- The previous edition is in `v3-pro-before-review/`.
