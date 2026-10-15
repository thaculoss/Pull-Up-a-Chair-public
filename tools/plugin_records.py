"""Editable Skyrim ESP/SEQ record source and deterministic rebuild utility.

This serializes the final release record tree, retaining order and all header
metadata. It does not bundle or require Skyrim master files. Hex subrecord data
remains the authority; printable strings are also shown as annotations.
"""
from pathlib import Path
import argparse, hashlib, json, struct, zipfile, zlib

def sha(raw):
    return hashlib.sha256(raw).hexdigest()

def decode_subrecords(raw):
    out, pos = [], 0
    while pos < len(raw):
        tag, size = struct.unpack_from('<4sH', raw, pos)
        pos += 6
        value = raw[pos:pos+size]
        pos += size
        assert len(value) == size
        item = {'tag':tag.decode('ascii'), 'data_hex':value.hex()}
        if tag in (b'EDID', b'FULL', b'MAST', b'CNAM', b'SNAM', b'MODL') and value.endswith(b'\0'):
            try:
                text = value[:-1].decode('utf-8')
                if all(c.isprintable() or c in '\r\n\t' for c in text):
                    item['text_annotation'] = text
            except UnicodeDecodeError:
                pass
        out.append(item)
    assert pos == len(raw)
    return out

def decode_records(raw):
    def walk(start, end):
        out, pos = [], start
        while pos < end:
            tag, size = struct.unpack_from('<4sI', raw, pos)
            header = raw[pos:pos+24]
            if tag == b'GRUP':
                item = {'kind':'group', 'header_tail_hex':header[8:].hex(), 'records':walk(pos+24, pos+size)}
                pos += size
            else:
                body = raw[pos+24:pos+24+size]
                compressed = bool(struct.unpack_from('<I',header,8)[0] & 0x40000)
                original_body = body
                if compressed:
                    length = struct.unpack_from('<I',body,0)[0]
                    body = zlib.decompress(body[4:])
                    assert len(body) == length
                item = {'kind':'record', 'tag':tag.decode('ascii'), 'formid':f"{struct.unpack_from('<I',header,12)[0]:08X}", 'header_tail_hex':header[8:].hex(), 'subrecords':decode_subrecords(body)}
                if compressed:
                    # Preserve compressed representation for exact unchanged rebuild.
                    # If subrecords are edited, a fresh compressed body is produced.
                    item['original_compressed_body_hex'] = original_body.hex()
                    item['uncompressed_sha256'] = sha(body)
                pos += 24+size
            out.append(item)
        assert pos == end
        return out
    return walk(0, len(raw))

def encode_records(tree):
    out = bytearray()
    for item in tree:
        tail = bytes.fromhex(item['header_tail_hex'])
        assert len(tail) == 16
        if item['kind'] == 'group':
            body = encode_records(item['records'])
            out.extend(b'GRUP'+struct.pack('<I',len(body)+24)+tail+body)
        else:
            body = b''.join(s['tag'].encode('ascii')+struct.pack('<H',len(bytes.fromhex(s['data_hex'])))+bytes.fromhex(s['data_hex']) for s in item['subrecords'])
            if struct.unpack_from('<I',tail)[0] & 0x40000:
                if sha(body) == item.get('uncompressed_sha256'):
                    original = bytes.fromhex(item['original_compressed_body_hex'])
                    assert zlib.decompress(original[4:]) == body
                    body = original
                else:
                    body = struct.pack('<I',len(body))+zlib.compress(body)
            out.extend(item['tag'].encode('ascii')+struct.pack('<I',len(body))+tail+body)
    return bytes(out)

def extract(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    index = []
    with zipfile.ZipFile(archive) as z:
        for name in sorted(z.namelist()):
            suffix = Path(name).suffix.lower()
            if suffix not in ('.esp','.seq'):
                continue
            raw = z.read(name)
            item = {'archive_path':name, 'sha256':sha(raw), 'length':len(raw)}
            if suffix == '.esp':
                item['records'] = decode_records(raw)
                assert encode_records(item['records']) == raw
            else:
                assert len(raw)%4 == 0
                item['quest_formids_hex'] = [f'{v:08X}' for v in struct.unpack('<'+'I'*(len(raw)//4),raw)]
            json_name = Path(name).name+'.json'
            (destination/json_name).write_text(json.dumps(item,ensure_ascii=False,indent=2),encoding='utf-8')
            index.append({'source':json_name, 'archive_path':name, 'sha256':sha(raw), 'length':len(raw)})
    (destination/'index.json').write_text(json.dumps({'schema':1,'origin_archive_sha256':sha(Path(archive).read_bytes()),'files':index},indent=2),encoding='utf-8')
    return index

def rebuild(source, output):
    index = json.loads((source/'index.json').read_text('utf-8'))
    report = []
    for row in index['files']:
        item = json.loads((source/row['source']).read_text('utf-8'))
        raw = encode_records(item['records']) if 'records' in item else b''.join(struct.pack('<I',int(v,16)) for v in item['quest_formids_hex'])
        target = output/item['archive_path']
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(raw)
        report.append({'archive_path':item['archive_path'],'sha256':sha(raw),'matches_release':sha(raw)==item['sha256']})
    return report

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command',required=True)
    a = sub.add_parser('extract');a.add_argument('archive',type=Path);a.add_argument('destination',type=Path)
    b = sub.add_parser('rebuild');b.add_argument('source',type=Path);b.add_argument('output',type=Path)
    args = parser.parse_args()
    result = extract(args.archive,args.destination) if args.command=='extract' else rebuild(args.source,args.output)
    print(json.dumps(result,ensure_ascii=False,indent=2))
