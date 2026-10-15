"""Isolated, structural rename of the reviewed r2 payload; no live deployment."""
from pathlib import Path
import hashlib, json, re, shutil, struct, sys, zlib
sys.dont_write_bytecode = True
BASE = Path(__file__).resolve().parent
ROOT = BASE.parent
OLD = ROOT / 'inn-ordering-fomod-20261013-r2'
STAGE = BASE / 'stage'
NAME = 'Pull-Up-a-Chair-0.9-beta-FOMOD-20261013'
MAPPING = {
    'Local - Inn Ordering': 'Pull up a chair Ordering',
    'Local - Inn Service': 'Pull up a chair Service',
    'Local Inn Ordering': 'Pull Up a Chair',
    'Local Inn Service': 'Pull Up a Chair Service',
    'LocalFoodGallery': 'PullUpAChairOrdering',
    'LocalDailyMealPlate': 'PullUpAChairEAS',
    'LocalInnOrdering': 'PullUpAChairOrdering',
    'LocalInnService': 'PullUpAChairService',
    'LocalInn': 'PullUpAChair',
}
PATTERN = re.compile('|'.join(re.escape(s) for s in sorted(MAPPING, key=len, reverse=True)), re.I)
LOOKUP = {key.lower(): value for key, value in MAPPING.items()}
def rename(text):
    def substitute(match):
        result = LOOKUP[match.group().lower()]
        return result.lower() if match.group().islower() else result
    return PATTERN.sub(substitute, text)
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(value, bytes): path.write_bytes(value)
    else: path.write_text(value, encoding='utf-8', newline='\n')
def save(path, value): write(path, json.dumps(value, ensure_ascii=False, indent=2))


sys.path.insert(0, str(ROOT/'download-compat-audit-20261004/gotobed-vokrii-patch-20261004'))
from patch_original_pex import Pex


def rename_pex(data):
    """Only rebuild length-prefixed header/string-table values. Bytecode is retained."""
    original = Pex(data)
    pos = 16
    output = bytearray(data[:pos])
    def string():
        nonlocal pos
        length = struct.unpack_from('>H', data, pos)[0]; pos += 2
        old = data[pos:pos+length]; pos += length
        new = rename(old.decode('utf-8', errors='surrogateescape')).encode('utf-8', errors='surrogateescape')
        return struct.pack('>H', len(new)) + new
    for _ in range(3): output.extend(string())
    count = struct.unpack_from('>H', data, pos)[0]
    output.extend(data[pos:pos+2]); pos += 2
    for _ in range(count): output.extend(string())
    suffix = data[pos:]
    output.extend(suffix)
    patched = Pex(bytes(output))
    assert patched.strings == [rename(s) for s in original.strings]
    assert bytes(output)[len(output)-len(suffix):] == suffix
    assert len(patched.functions) == len(original.functions)
    return bytes(output), {'object': patched.name, 'string_count': count, 'functions': len(patched.functions), 'bytecode_and_object_data_unchanged': True}


def rename_vmad(value):
    # Script and fragment names use uint16 byte-length strings inside VMAD.
    edits = []
    for match in PATTERN.finditer(value.decode('latin1')):
        at = match.start()
        # The match can be a prefix of a complete script name.
        assert at >= 2
        size = struct.unpack_from('<H', value, at - 2)[0]
        assert len(match.group()) <= size < 256 and at+size <= len(value), ('Unexpected VMAD string', at, size)
        old = value[at:at+size].decode('utf-8')
        new = rename(old).encode('utf-8')
        edits.append((at-2, at+size, struct.pack('<H', len(new))+new))
    result = value
    for start, end, new in sorted(edits, reverse=True): result = result[:start]+new+result[end:]
    return result


def rename_esp(data):
    changes, records = [], []
    def walk(start, end):
        result = bytearray()
        pos = start
        while pos < end:
            tag = data[pos:pos+4]
            header = bytearray(data[pos:pos+24])
            size = struct.unpack_from('<I', data, pos+4)[0]
            if tag == b'GRUP':
                body = walk(pos+24, pos+size)
                struct.pack_into('<I', header, 4, len(body)+24)
                pos += size
            else:
                flags, fid = struct.unpack_from('<II', data, pos+8)
                raw = data[pos+24:pos+24+size]
                body = zlib.decompress(raw[4:]) if flags & 0x40000 else raw
                fields, offset = bytearray(), 0
                while offset < len(body):
                    subtag, length = struct.unpack_from('<4sH', body, offset); offset += 6
                    assert subtag != b'XXXX', 'Extended subrecord needs explicit handling'
                    value = body[offset:offset+length]; offset += length
                    if subtag == b'VMAD':
                        new = rename_vmad(value)
                    elif PATTERN.search(value.decode('latin1')):
                        assert subtag in {b'EDID', b'FULL', b'CNAM', b'SNAM', b'MAST', b'ANAM', b'FNAM', b'MODL'}, subtag
                        new = rename(value.decode('utf-8')).encode('utf-8')
                    else: new = value
                    if new != value: changes.append({'record': tag.decode(), 'formid': hex(fid), 'field': subtag.decode(), 'before_bytes': len(value), 'after_bytes': len(new)})
                    assert len(new) < 65536
                    fields.extend(struct.pack('<4sH', subtag, len(new))+new)
                assert offset == len(body)
                body = struct.pack('<I', len(fields))+zlib.compress(bytes(fields)) if flags & 0x40000 else bytes(fields)
                struct.pack_into('<I', header, 4, len(body))
                records.append((tag.decode(), fid, flags))
                pos += size + 24
            result.extend(header); result.extend(body)
        assert pos == end
        return bytes(result)
    result = walk(0, len(data))
    return result, {'records': records, 'changed_fields': changes, 'formids_and_record_flags_preserved': True}


def prepare():
    assert not STAGE.exists()
    old_manifest = json.loads((OLD/'stage/package-manifest.json').read_text('utf-8'))
    baseline = []
    pex_checks, esp_checks = {}, {}
    paths = set()
    for item in old_manifest['files']:
        source = OLD/'stage'/item['path']
        assert sha(source) == item['sha256']
        relative = rename(item['path'])
        assert relative.lower() not in paths, relative
        paths.add(relative.lower())
        data = source.read_bytes()
        target = STAGE/relative
        if source.suffix.lower() == '.pex':
            data, pex_checks[relative] = rename_pex(data)
        elif source.suffix.lower() == '.esp':
            data, esp_checks[relative] = rename_esp(data)
        elif source.suffix.lower() not in {'.dll', '.png', '.wav', '.seq'}:
            # License notices keep original attribution/content.
            if '/Licenses/' not in relative and not source.name.endswith('OFL.txt'):
                data = rename(data.decode('utf-8-sig')).encode('utf-8')
        write(target, data)
        baseline.append({'original': str(source.relative_to(ROOT)), 'source_sha256': item['sha256'], 'path': relative, 'initial_sha256': sha(target)})
    # Set the product release name without rewriting historical behavior details.
    info = STAGE/'fomod/info.xml'
    text = info.read_text('utf-8').replace('2.0.0-alpha.4.19-promptclear1', '0.9 beta').replace('MachineVersion="2.0.0.419"', 'MachineVersion="0.9.0.0"')
    write(info, text)
    module = STAGE/'fomod/ModuleConfig.xml'
    write(module, module.read_text('utf-8').replace('Pull Up a Chair - Portable Test / 独立点菜测试', 'Pull Up a Chair 0.9 beta'))
    # Complete the native source mirror into a self-contained project using local pinned dependencies.
    core = BASE/'native/core'
    service = BASE/'native/service'
    shutil.copytree(STAGE/'00_Core/Source/NativeCore', core)
    shutil.copytree(STAGE/'20_Service/Source/NativeService', service)
    shutil.copytree(STAGE/'30_EAS/Source/NativeAdapter/src', core/'adapter/src')
    shutil.copytree(STAGE/'00_Core/SKSE/Plugins/PullUpAChairOrdering/catalog', core/'config')
    shutil.copytree(STAGE/'00_Core/SKSE/Plugins/PullUpAChairOrdering/locales', core/'config/locales')
    for filename in ['PullUpAChairOrderingSettingsUI.ini', 'PullUpAChairOrderingUI.ini']:
        shutil.copyfile(STAGE/'00_Core/SKSE/Plugins'/filename, core/'config'/filename)
    shutil.copyfile(STAGE/'90_Settings/en/0/1/SKSE/Plugins/PullUpAChairOrdering.ini', core/'config/PullUpAChairOrdering.ini')
    for project in [core, service]:
        cmake = project/'CMakeLists.txt'
        value = cmake.read_text('utf-8').split('set(IMGUI ')[0]
        value = value.replace('VERSION 2.0.0', 'VERSION 0.9.0').replace('VERSION 0.2.0', 'VERSION 0.9.0')
        write(cmake, value)
    for source in [core/'src/main.cpp', service/'src/main.cpp', core/'adapter/src/main.cpp']:
        value = source.read_text('utf-8')
        value = value.replace('Independent Inn Ordering 2.0.0-alpha.4.19-ui13-skse-compact-dak4-ordercleanup1-topics1-safety1-cleanup2-dak5-bundleonly1-aiguard1-scriptguard2-promptclear1', 'Pull Up a Chair Ordering 0.9 beta')
        value = value.replace('PullUpAChairService 0.2.0-alpha.2-menu4-safety1', 'Pull Up a Chair Service 0.9 beta')
        value = value.replace('PullUpAChairEAS optional adapter 2.0.0-alpha.4', 'Pull Up a Chair EAS adapter 0.9 beta')
        write(source, value)
    save(BASE/'rename-map.json', MAPPING)
    save(BASE/'baseline.json', baseline)
    save(BASE/'pex-validation.json', pex_checks)
    save(BASE/'esp-validation.json', esp_checks)
    profile = ROOT/'profiles/整合新版本测试'
    save(BASE/'source-state.json', {str(p.relative_to(ROOT)): sha(p) for p in [ROOT/'ModOrganizer.ini', profile/'modlist.txt', profile/'plugins.txt', profile/'loadorder.txt']})
    food = json.loads((OLD/'food-rule-provenance.json').read_text('utf-8'))
    save(BASE/'food-rule-provenance.json', food)
    # Verify r2 binary/source basis against the currently active installed package.
    active_mod = ROOT/'mods/pull up a chair 0.9 beta FOMOD'
    matches = []
    for item in old_manifest['files']:
        parts = Path(item['path']).parts
        if parts[0] not in {'00_Core','20_Service','30_EAS'}: continue
        rel = Path(*parts[1:])
        if rel.suffix.lower() not in {'.dll','.esp','.pex','.cpp','.h','.inl','.psc'}: continue
        current = active_mod/rel
        assert current.is_file() and sha(current) == item['sha256'], ('Active source differs', str(current))
        matches.append(str(rel))
    save(BASE/'active-basis.json', {'mod': str(active_mod.relative_to(ROOT)), 'verified_binary_and_source_files': matches})
    print(json.dumps({'stage_files': len(baseline), 'pex_files': len(pex_checks), 'esp_files': len(esp_checks), 'active_basis_verified': len(matches)}, ensure_ascii=False))


if __name__ == '__main__': prepare()
