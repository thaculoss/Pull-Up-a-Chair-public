Pull Up a Chair — artclean1 source and license materials
Author: thaculoss
Prepared: 2026-10-15

This is a separate source package, not a mod-manager install package.
It corresponds to Pull-Up-a-Chair-0.9.9-beta-artclean1-FOMOD.zip:
SHA256 1bd5d0a4a553fcec6496801b80d6c48e7ef20c1ecf1771be27f14e372cb0930b

CONTENTS
native/core       Exact artclean1 ordering and inherited dining/EAS production source.
native/service    Original service source matching paper3's unchanged service DLL.
papyrus           Canonical source for all eight installed PEX scripts.
plugin-source     Editable final record data for three ESP and two SEQ files.
vendor            Nineteen pinned dependency source snapshots with notices.
vendor-build      Out-of-tree source build definitions; no automatic downloads.
tools             Plugin record reconstruction and Papyrus verification tools.
test-data         Font and historical configuration inputs for offline tests.
licenses          Original third-party notices, exceptions and font license.
provenance        Input hashes, origins, original build definitions and patches.
evidence          Script/plugin/source correspondence reports.

LICENSE
The author approved GPL-3.0-only for original code he is entitled to license.
See LICENSE.txt and the complete COPYING. Third-party licenses and exceptions
remain applicable. This does not grant new rights to photographs, handwriting,
recordings, fonts, Bethesda graphics, game records or third-party patch inputs.
The separately supplied rights materials describe their respective origins.

BUILD AND VERIFY
See BUILD.txt. Native entry points have been made relative to this source tree.
Artwork.h now recognizes PNG field "-" as an intentional absence of illustration.
inn_artwork.tsv disables only the Retching Netch illustration; names and titles stay.
These are the only production source/config changes since paper3. The offline
preview/main.cpp also accepts deliberately absent illustrations, while retaining
the title centering check. Portable CMake
wiring is retained from the paper3 source preparation. PSC files are unchanged.
Original CMake files and their portable wiring diffs are in provenance/build.
The project retains the service DLL's original 0.9.0 code version because the
paper3 release reused it; it is not a mistaken downgrade to be auto-updated.

Current source/binary evidence is in evidence/artclean1-validation.json.
The following checks are historical paper3 results; their 13 PEX/ESP/SEQ
payload files were freshly verified identical in artclean1:
All eight PSC files were recompiled and their full instruction/state content
compared with paper3, ignoring only build/debug metadata and identifier casing.
The final ESP/SEQ record representation reproduces paper3 byte-for-byte.
These checks do not establish live gameplay correctness or compatibility.

No compiled dependencies, game masters, CK/compiler binaries, private settings,
saves, Git history or raw personal photos/recordings are included here.
The game/CK/SKSE/Papyrus Extender declarations needed for Papyrus must be
obtained from their proper sources. This package does not provide those licenses.

HISTORICAL MATERIAL
historical-generators and provenance/previous-source-docs preserve earlier
workflows, including original local path references and outdated release notes.
They are evidence, not the current instructions. Use the root BUILD.txt and
tools/plugin_records.py / tools/verify_papyrus.py for current commands.
The current project license is the root LICENSE.txt, not an old README statement.

The new artclean1 FOMOD omits morrowind_netch.png. Old paper3 is preserved.
No historical report was relabeled as a fresh result. historical-paper3 contains
the previous 63-asset records; the current inventory contains 62 assets.
test-data/lowtable-before is historical test data, not install configuration.
Nothing is uploaded or automatically deployed.
