# R5-AnimConv

**R5-AnimConv** is a tool for converting **Source engine file formats** into ReSource Animation file formats.

---

### Usage


``` bash
# MDL mode
R5-AnimConv.exe <model.mdl> [-rp <override_rrig_path>] [-sp <override_rseq_path>] [-verbose] [-ne] [-nopause] [-comperr <error>]

# RSEQ mode
R5-AnimConv.exe <parent_directory> [-i <in_season>] [-verbose] [-ne] [-skipevents] [-nopause] [-comperr <error>]
```

>**Options:**
>- `-i <season>` — Input assets season (RSEQ mode only, range: 7–28, default: 28)
>- `-verbose` — Enable verbose output
>- `-ne` — Suppress RePak entries output
>- `-skipevents` — Skip events that may cause crashes
>- `-nopause` — No pause at execution end
>- `-comperr <float>` — Compression error threshold (0.5–2.0 recommended, 0.0 lossless, default: 1.0)
>- `-rp <path>` — Override internal rrig path (MDL mode only)
>- `-sp <path>` — Override internal rseq path (MDL mode only)

---

### Supported Versions

>**MDL Mode**
>- v49 *(missing ikrules / movements)*
>- v53

>**RSEQ Mode**
>- Seasons **S7 – S28**

---
### Based on:
>- [rmdlconv](https://github.com/r-ex/rmdlconv)
>- [RSX](https://github.com/r-ex/rsx)
>- [resource_model_templates](https://github.com/IJARika/resource_model_templates)