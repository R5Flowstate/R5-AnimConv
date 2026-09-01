# R5-AnimConv — agent notes

Human overview: `README.md`.

Writes season **3** or **21**. Default `-o` is **3**. Use **`-o 21`** for S21
(rrig v17 + rseq v11).

## Build

`R5-AnimConv.sln`, Release x64, toolset **v145** if MSB8020.

Output: `bin/Release_x64/R5-AnimConv.exe`.

## CLI

```
R5-AnimConv.exe <model.mdl> [-o <out>] [-rp <rrig>] [-sp <rseq>]
R5-AnimConv.exe <parent_directory> [-i <in>] [-o <out>]
```

| flag | meaning |
|------|---------|
| `-i` | input season (RSEQ mode, 7–28, default 28) |
| `-o` | output season: **3** or **21** (default 3) |
| `-outpath` | output dir (default `.\conv\`) |
| `-ne` | no RePak entry dump |
| `-skipevents` | drop crashy events |
| `-nopause` | no end pause |
| `-comperr` | compression threshold (default 1.0) |

Season map is `src/core/parsers.h`.

- **S21 write (`-o 21`)**: `WriteRRIG_v17` + `WriteRSEQ_v11`.
- **S21 read (`-i 21`)**: `ParseRRIG_v17` + `ParseRSEQ_v11`.
- **S3 write (`-o 3`)**: v8 rrig + v7 rseq (dedi).

## Do

- Point RSEQ mode at the **full extract** (models + `animrig/`), not only
  `animrig/`. `GatherRigPaths` accepts `.rrig` **and** `.rmdl`; a self-rigged
  animated model drives its own seqs via the sibling `.rson`.
- For S21 client anims: `-o 21`.
- For S3 dedi anims from S21 source: `-i 21 -o 3`.
- Classic (non-datapoint, single-section, no IK) `.asqd` is copied
  **verbatim**. Datapoint sources re-encode.
- `-i 27` / `-i 28` load `{in_dir}/animseq_data/0x{GUID}.asqd`. Missing
  asqd skips that rseq (do not `Error()` from the parse lambda —
  `std::async` + throw failfasts the process).

rseq v11 writer: 4-align animdesc; unsectioned `sectionstallframes = 0`;
classic asqd copy is also 4-aligned after the payload (framemovement can
follow). `-o 21` sections `numframes > 61` (stall 16, sf 61) even when
the source was unsectioned.

S21 GetAnim: `sectionframes == 0` uses `animindex` and ignores stall;
sectioned clips index `table[i]` including slot 0 (last frame is a
1-frame trail). Do not copy a later-season `Section()` that skips
`table[0]`.

## Do not

- Assume a nonzero exit means truncated output. The exe can AV **after**
  writing a complete tree. Diff the file set.
- Forget `animseq_derived/` when staging (pilot/weapon 3P seqs land there
  as well as `animseq/`).
- Treat `-o 21` as the default. It is not.
- Assume RePak's S21 aseq writer includes `.rseq_extn` — it embeds `.rseq`
  only. Native long clips use extn; converted `nf >= 96` clips can too.

## In / out

- MDL in: v49 (ikrules/movements incomplete), v53.
- RSEQ in: seasons 7–28.
- Out: season 3, season 21.
