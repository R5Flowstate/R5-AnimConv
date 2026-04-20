# R5-AnimConv

**R5-AnimConv** is a tool for converting **Source engine file formats**
into ReSource Animation file formats.

---

### Usage

```bash
Mdl  mode : R5-AnimConv.exe <model.mdl> [-rp <override_rrig_path>] [-sp <override_rseq_path>] [-verbose] [-ne] [-comperr <acceptable error>]
```
```bash
Rseq mode : R5-AnimConv.exe <parent directory> [-i <in season>] [-verbose] [-ne] [-comperr <acceptable error>]
```

>**Options**
>* `-i <in_season>`  : Input assets season (only: Rseq mode, default: 28)
>* `-o <out_season>` : Output assets season (default: 3)
>* `-verbose`    : Verbose outputs
>* `-ne`         : No RePak Entries outputs
>* `-skipevents` : Skip any events that might crash if lag of asset
>* `-nopause`    : No pause at the end of execution
>* `-comperr`    : Acceptable compression error, Recommended 0.5-2.0 (~3-10% smaller without noticeable visual) (default: 1.0)
>* `-rp <override_rrig_path>` : Override internal rrig path (only: Mdl mode)
>* `-sp <override_rseq_path>` : Override internal rseq path (only: Mdl mode)

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