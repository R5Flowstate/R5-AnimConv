# R5-AnimConv (R5Flowstate / S21)

Converts Source / ReSource animation (MDL, RRIG, RSEQ) between Apex seasons.
This fork adds a **Season 21 writer** (`-o 21`). Upstream defaulted to Season 3
output only.

Upstream: [someoneatemylastsliceofpizza/R5-AnimConv](https://github.com/someoneatemylastsliceofpizza/R5-AnimConv).

## What this fork adds

- **`-o 21`**: write **rrig v17 + rseq v11** (S21 client / S21 aseq).
- rseq v11: 4-byte-align animdesc; unsectioned sequences set `sectionstallframes=0`.
- Classic (non-datapoint) sources copy the `.asqd` RLE payload **verbatim**.
  S21 has no datapoint decoder, so datapoint sources still go through the
  re-encoder (the only option that S21 can play).

Default `-o` is still **3** (S3). Pass `-o 21` for the S21 pipeline.

## S21 v11 notes

- Each animdesc is 4-byte aligned. Unsectioned clips write `sectionstallframes = 0`
  (the engine ignores stall when `sectionframes` is 0).
- Classic (non-datapoint, single-section, no IK) `.asqd` is copied verbatim.
  Datapoint sources re-encode; S21 cannot play datapoint.
- `-o 21` sections any clip with `numframes > 61` (stall 16, 61-frame sections)
  even if the source was unsectioned.
- Long clips may write a sibling `.rseq_extn`. The S21 aseq packer embeds only
  the `.rseq` — include the extn if you pack those files.
- `-i 27` / `-i 28` need `animseq_data/0x{GUID}.asqd` next to the extract. A
  missing asqd skips that sequence (it does not abort the run).

## Usage

```bash
# MDL mode
R5-AnimConv.exe <model.mdl> [-o <out season>] [-rp <override_rrig_path>] [-sp <override_rseq_path>]

# RSEQ mode
R5-AnimConv.exe <parent_directory> [-i <in_season>] [-o <out season>]
```

**Options:**
- `-i <season>` — Input assets season (RSEQ mode only, range: 7–28 and 30, default: 28)
- `-o <season>` — Output assets season (3 or 21, default: 3)
- `-outpath <path>` - Output directory (default: `.\\conv\\`)
- `-verbose <level>` - 0 none, 1 minimal, 2 full (default: 1)
- `-ne` — Suppress RePak entries output
- `-skipevents` — Skip events that may cause crashes
- `-nopause` — No pause at execution end
- `-comperr <float>` — Compression error threshold (default: 1.0)
- `-rp <path>` — Override internal rrig path (MDL mode only)
- `-sp <path>` — Override internal rseq path (MDL mode only)

## Supported versions

**MDL in:** v49 (missing ikrules / movements), v53

**RSEQ in:** seasons 7–28 and 30

**Out:** season 3, season 21

## Based on

- [rmdlconv](https://github.com/r-ex/rmdlconv)
- [RSX](https://github.com/r-ex/rsx)
- [resource_model_templates](https://github.com/IJARika/resource_model_templates)
