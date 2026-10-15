Pull Up a Chair — paper3 source and license materials
Author: thaculoss
Prepared: 2026-10-15

This is a separate source package, not a mod-manager install package.
It corresponds to Pull-Up-a-Chair-0.9.9-beta-paper3-FOMOD.zip:
SHA256 fbdff8fe999e04cbcea7aeae1773b02142f06b6e436fac0254edc4ef5c71a26a

CONTENTS
native/core       Exact paper3 ordering and dining/EAS production source.
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
Only project CMake wiring changed; original production C++/PSC logic did not.
Original CMake files and their portable wiring diffs are in provenance/build.
The project retains the service DLL's original 0.9.0 code version because the
paper3 release reused it; it is not a mistaken downgrade to be auto-updated.

Three DLL source origins are recorded in evidence/native-source-validation.json.
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

The game FOMOD remains unchanged. Nothing is uploaded or automatically deployed.
