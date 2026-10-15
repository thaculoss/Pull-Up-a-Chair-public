"""Compile supplied Papyrus sources and compare functions against a release ZIP.

Compiler and import sources are external prerequisites, not redistributed here.
Example: py verify_papyrus.py --compiler /path/Caprica.exe --imports /path/imports
    --archive /path/release.zip --output /path/verification
"""
from pathlib import Path
import argparse, hashlib, json, struct, subprocess, sys, zipfile
from pex_reader import Pex

def sha(b):
    return hashlib.sha256(b).hexdigest()

def parse(raw):
    obj = Pex.__new__(Pex)
    try:
        obj.__init__(raw)
    except AssertionError:
        # Caprica emits a little-endian object size excluding its own 4 bytes.
        assert obj.at == len(raw)
        assert struct.unpack_from('<I', raw, obj.size_at)[0] == len(raw)-obj.size_at-4
        raw = raw[:obj.size_at] + struct.pack('>I', len(raw)-obj.size_at) + raw[obj.size_at+4:]
        obj = Pex(raw)
    return obj, raw

def signature(p):
    def operand(v):
        return (v[0], v[1].lower() if v[0] == 1 else v[1])
    # Papyrus identifiers and type names are case insensitive. Do not lowercase
    # string literal operands (type 2) or documentation text.
    def declarations(rows):
        return [(name.lower(),type_name.lower()) for name,type_name in rows]
    return {k: ([f['metadata'][0].lower(), *f['metadata'][1:]], declarations(f['params']), declarations(f['locals']), [
        (o['op'], [operand(v) for v in o['args']], [operand(v) for v in o['extra']])
        for o in f['ops']]) for k,f in p.functions.items()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--imports', type=Path, required=True)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent.parent / 'papyrus'
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    with zipfile.ZipFile(args.archive) as z:
        for entry in sorted(n for n in z.namelist() if n.lower().endswith('.pex')):
            name = Path(entry).stem
            psc = source / (name+'.psc')
            assert psc.is_file(), psc
            command = [str(args.compiler), psc.name, '--game=skyrim', '--import', str(source)+';'+str(args.imports), '--flags', str(args.imports/'TESV_Papyrus_Flags.flg'), '--output', str(args.output)]
            run = subprocess.run(command, cwd=source, capture_output=True, text=True, encoding='utf-8', errors='replace')
            (args.output/(name+'.compile.log')).write_text(run.stdout+run.stderr, encoding='utf-8')
            assert run.returncode == 0, (name, run.stdout, run.stderr)
            before, _ = parse(z.read(entry))
            after, normalized = parse((args.output/(name+'.pex')).read_bytes())
            (args.output/(name+'.pex')).write_bytes(normalized)
            state_equal = (before.name, before.parent, before.variables, before.properties, before.states)==(after.name, after.parent, after.variables, after.properties, after.states)
            fn_before, fn_after = signature(before), signature(after)
            changes = [key for key in fn_before.keys() | fn_after.keys() if fn_before.get(key) != fn_after.get(key)]
            item = {'archive_path':entry, 'source':psc.name, 'source_sha256':sha(psc.read_bytes()), 'release_sha256':sha(z.read(entry)), 'recompiled_sha256':sha(normalized), 'state_and_properties_match':state_equal, 'different_functions':changes, 'all_function_instructions_match':not changes}
            results.append(item)
            print(json.dumps(item))
    report = {'archive_sha256':sha(args.archive.read_bytes()), 'compiler_sha256':sha(args.compiler.read_bytes()), 'scripts':results, 'all_passed':all(r['state_and_properties_match'] and r['all_function_instructions_match'] for r in results), 'comparison_normalization':'Ignore file headers, debug lines, and case of Papyrus identifiers/type names; retain literal strings, instructions, arguments and state fields.', 'scope':'Offline source-to-PEX instruction/state comparison; no gameplay assertion.'}
    (args.output/'papyrus-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    assert report['all_passed'], 'One or more source-to-release comparisons differ; see report.'

if __name__ == '__main__':
    main()
