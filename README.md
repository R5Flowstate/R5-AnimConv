# R5-AnimConv ( R5Flowstate/S21 )

Converts Source / ReSource animation assets (MDL, RRIG, RSEQ) between
versions. This fork adds a **Season 21 writer** (`-o 21`) emitting
**rrig v17 + rseq v11**; upstream wrote Season 3 only.

Agents view included: CLAUDE.md

## Usage

```
R5-AnimConv <model.mdl> [-o <season>] [-rp <rrig path>] [-sp <rseq path>]
R5-AnimConv <directory> [-i <season>] [-o <season>] [-outpath <dir>]
```

```
# S21 client (rrig v17 + rseq v11)
R5-AnimConv <dir> -i 28 -o 21

# S3 dedicated server -- the default
R5-AnimConv <dir> -i 28

# single model, rewriting the internal rrig path
R5-AnimConv model.mdl -o 21 -rp animations/mymodel.rrig
```

Directory mode expects `animseq_data/0x{GUID}.asqd` beside the extract; a
missing asqd skips that sequence rather than aborting. Long clips may write
a sibling `.rseq_extn` -- pack it alongside the `.rseq`.

Upstream: [someoneatemylastsliceofpizza/R5-AnimConv](https://github.com/someoneatemylastsliceofpizza/R5-AnimConv)
