# Service, Papyrus and plugin source for artclean1

Service, Papyrus and plugin payloads are inherited unchanged from paper3.
The original compilation/rebuild checks below are historical; this release
freshly checks the 13 PEX/ESP/SEQ payload files against paper3 byte-for-byte.
See evidence/artclean1-validation.json for the current check.

This directory accompanies `Pull-Up-a-Chair-0.9.9-beta-artclean1-FOMOD.zip`.
It belongs outside the install ZIP. It does not modify that ZIP, installed mods,
game saves, or the live MO2 profile.

## What is included

- `native/service`: the 23 original service build inputs. The current release's
  service DLL is byte-identical to the original 0.9 build output. Every source
  file also matches that original release's recorded source manifest.
- `papyrus`: all eight PSC files corresponding to the eight installed PEX files.
  The ordering quest is specifically the lowtablelayout1 source used by paper3;
  it is not the unapproved mealrestore2 experiment.
- `plugin-source`: the final three ESP record trees and two SEQ lists in editable
  JSON, exported from the current release. Record order, form IDs, header data,
  subrecord bytes and compression are retained. `data_hex` is authoritative;
  `text_annotation` and `formid` are explanatory fields, not alternate inputs.
- `tools/plugin_records.py`: portable Python 3 tool to rebuild final ESP/SEQ data.
- `tools/verify_papyrus.py` and `tools/pex_reader.py`: recompile source and compare
  PEX functions, operand values and save-state declarations against the ZIP.
- `historical-generators`: original construction/rename/compatibility patch
  scripts and local Python helper modules. These preserve development provenance,
  including Bethesda-template and Chatty NPCs inputs. They retain historical local
  paths and are not the portable release rebuild entry point. Do not run them
  blindly; some expect old staging directories, game masters or source scripts.

## Validation performed

All eight PSC files were compiled with Caprica v0.3.0 against the available Skyrim,
SKSE and powerofthree Papyrus declarations. The recompiled PEX functions have the
same complete instruction/argument sequences, variables, properties and states as
the release. Comparison ignores source paths, build timestamps, debug line data,
and case of Papyrus identifiers/type names; string literals are not normalized.
Raw PEX file hashes therefore differ. See `evidence/historical-paper3/papyrus-source-validation.json`.

All three ESP and both SEQ files rebuilt from JSON are byte-identical to the
current FOMOD. See `evidence/historical-paper3/plugin-rebuild-validation.json`. These checks are
offline source correspondence checks, not new game/compatibility tests.

## Portable commands

Python 3.12 was used; no third-party Python packages are needed:

```powershell
python tools/plugin_records.py rebuild plugin-source output/plugins
python tools/verify_papyrus.py --compiler C:/tools/Caprica.exe --imports C:/Papyrus/imports --archive C:/downloads/Pull-Up-a-Chair-0.9.9-beta-artclean1-FOMOD.zip --output output/papyrus
```

For Papyrus, obtain Caprica and supply Skyrim SE Creation Kit script declarations
(including `TESV_Papyrus_Flags.flg`), matching SKSE script sources, and
powerofthree's Papyrus Extender `PO3_SKSEFunctions.psc` declarations. The eight
project PSC files are automatically the first import directory. Game sources,
third-party runtime scripts and compiler executables are not redistributed here.
No EAS source patches are required by the current native API stub.

The historical native-service CMake file is preserved as original build evidence.
Use the consolidated source package's portable native build entry point and pinned
dependencies when available; do not assume the historical workspace paths exist.

## Redistribution/permission boundaries

Including generator provenance does not grant a license to Bethesda game data or
the original Chatty NPCs mod. The Chatty record override source is the data already
distributed in this compatibility patch, not a copy of the whole source mod.
Review the accompanying license and permission inventory before publication.
The current project code license is the root LICENSE.txt; this note does not override
that choice or invent third-party permission.

## Packaging selection

Include `native`, `papyrus`, `plugin-source`, `tools` (excluding `__pycache__`),
`historical-generators` and this README in the separate source archive. Include
the four JSON reports under `evidence` as support evidence. `compiled` contains
temporary verification outputs only and should not enter the source archive.
