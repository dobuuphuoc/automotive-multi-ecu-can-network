"""
Comprehensive Verification Script for Thesis/ECU_Carrier_Board/ECU_Carrier_PCB.PcbDoc
Checks:
1. OLE2 Compound File Binary structure (Header, FAT, MiniFAT, Directory hierarchy)
2. Rules6/Data stream records (37 records, SolderMaskExpansion ISTENTINGTOP=FALSE & ISTENTINGBOTTOM=FALSE)
3. Components6/Data (S1 at (44.75, 17.25) mm)
4. Pads6/Data (S1 pads at (41.50, 15.00), (41.50, 19.50), (48.00, 15.00), (48.00, 19.50) mm)
5. Alignment with USER_BTN & GND routed tracks
"""
import os
from pathlib import Path
import struct
import sys

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PCB_PATH = REPO_ROOT / 'Thesis' / 'ECU_Carrier_Board' / 'ECU_Carrier_PCB.PcbDoc'

def verify(pcb_path=None):
    if pcb_path is None:
        pcb_path = DEFAULT_PCB_PATH
    pcb_path = Path(pcb_path)
    print(f"Loading '{pcb_path}'...")
    assert pcb_path.exists(), f"File not found: {pcb_path}"
    with open(pcb_path, 'rb') as f:
        data = f.read()
    file_size = len(data)
    print(f"File size: {file_size} bytes")

    # 1. OLE2 Header Verification
    print("\n--- 1. OLE2 Header Verification ---")
    magic = data[:8]
    assert magic == b'\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1', f"Invalid magic: {magic.hex()}"
    major_ver, byte_order, sector_shift, mini_shift = struct.unpack_from('<HHHH', data, 26)
    assert byte_order == 0xFFFE, f"Unexpected byte order: 0x{byte_order:04X}"
    assert sector_shift == 9, f"Expected 512 sector size, got 2^{sector_shift}"
    assert mini_shift == 6, f"Expected 64 mini sector size, got 2^{mini_shift}"
    sec_size = 1 << sector_shift
    mini_sec_size = 1 << mini_shift

    num_dir_sectors, num_fat_sectors, first_dir_sector = struct.unpack_from('<III', data, 40)
    mini_cutoff, first_mini_fat, num_mini_fat = struct.unpack_from('<III', data, 56)
    first_difat, num_difat = struct.unpack_from('<II', data, 68)

    print(f"Sector size: {sec_size}, Mini sector size: {mini_sec_size}")
    print(f"FAT sectors: {num_fat_sectors}, Dir start sector: {first_dir_sector}")
    print(f"Mini FAT sectors: {num_mini_fat}, Mini FAT start sector: {first_mini_fat}, Mini cutoff: {mini_cutoff}")

    # 2. FAT & MiniFAT Verification
    print("\n--- 2. FAT & MiniFAT Verification ---")
    difat = list(struct.unpack_from('<109I', data, 76))
    fat_sids = [s for s in difat[:num_fat_sectors] if s < 0xFFFFFFFC]
    assert len(fat_sids) == num_fat_sectors, f"DIFAT sector count mismatch: {len(fat_sids)} vs {num_fat_sectors}"

    fat = []
    for fsid in fat_sids:
        offset = (fsid + 1) * sec_size
        fat.extend(struct.unpack(f'<{sec_size//4}I', data[offset:offset+sec_size]))
    print(f"FAT total entries: {len(fat)}")

    def get_fat_chain(start_sid):
        chain = []
        curr = start_sid
        visited = set()
        while curr < 0xFFFFFFFE: # ENDOFCHAIN
            assert curr not in visited, f"Cycle detected in FAT chain starting at {start_sid}: {curr}"
            assert curr < len(fat), f"FAT sector ID out of bounds: {curr} >= {len(fat)}"
            visited.add(curr)
            chain.append(curr)
            curr = fat[curr]
        return chain

    def read_fat_stream(start_sid, size):
        if size == 0:
            return b''
        chain = get_fat_chain(start_sid)
        assert len(chain) * sec_size >= size, f"Chain size {len(chain)*sec_size} < stream size {size}"
        buf = bytearray()
        for s in chain:
            off = (s + 1) * sec_size
            buf.extend(data[off : off + sec_size])
        return bytes(buf[:size])

    # MiniFAT reading
    minifat_chain = get_fat_chain(first_mini_fat)
    minifat = []
    for msid in minifat_chain:
        off = (msid + 1) * sec_size
        minifat.extend(struct.unpack(f'<{sec_size//4}I', data[off:off+sec_size]))
    print(f"MiniFAT total entries: {len(minifat)}")

    # 3. Directory Entries Verification
    print("\n--- 3. Directory Entries Verification ---")
    dir_chain = get_fat_chain(first_dir_sector)
    dir_data = bytearray()
    for dsid in dir_chain:
        off = (dsid + 1) * sec_size
        dir_data.extend(data[off : off + sec_size])

    num_dir_entries = len(dir_data) // 128
    print(f"Directory total entries: {num_dir_entries}")

    dir_entries = []
    for idx in range(num_dir_entries):
        ent = dir_data[idx*128 : (idx+1)*128]
        name_len = struct.unpack_from('<H', ent, 64)[0]
        if name_len > 2:
            name = ent[:name_len-2].decode('utf-16le', errors='replace')
        else:
            name = ''
        obj_type = ent[66]
        left = struct.unpack_from('<I', ent, 68)[0]
        right = struct.unpack_from('<I', ent, 72)[0]
        child = struct.unpack_from('<I', ent, 76)[0]
        start_sec = struct.unpack_from('<I', ent, 116)[0]
        stream_sz = struct.unpack_from('<Q', ent, 120)[0]
        dir_entries.append({
            'idx': idx,
            'name': name,
            'type': obj_type,
            'start': start_sec,
            'size': stream_sz,
            'left': left,
            'right': right,
            'child': child
        })

    # Root entry check
    root = dir_entries[0]
    assert root['type'] == 5, f"Root entry type is {root['type']}, expected 5"
    assert root['name'] == 'Root Entry', f"Root entry name is {root['name']}"
    ministream_chain = get_fat_chain(root['start'])
    ministream_data = bytearray()
    for s in ministream_chain:
        off = (s + 1) * sec_size
        ministream_data.extend(data[off : off + sec_size])
    ministream = bytes(ministream_data[:root['size']])
    print(f"Root Entry MiniStream container size: {len(ministream)} bytes")

    def read_ministream(start_mini_sid, size):
        curr = start_mini_sid
        chain = []
        visited = set()
        while curr < 0xFFFFFFFE:
            assert curr not in visited, f"Cycle in MiniFAT chain: {curr}"
            assert curr < len(minifat), f"MiniFAT index out of bounds: {curr}"
            visited.add(curr)
            chain.append(curr)
            curr = minifat[curr]
        buf = bytearray()
        for ms in chain:
            off = ms * mini_sec_size
            buf.extend(ministream[off : off + mini_sec_size])
        return bytes(buf[:size])

    def get_stream(full_path):
        storage_name, stream_name = full_path.split('/')
        # Find storage entry
        storage_idx = None
        for e in dir_entries:
            if e['type'] == 1 and e['name'] == storage_name:
                storage_idx = e['idx']
                break
        assert storage_idx is not None, f"Storage '{storage_name}' not found"

        # Traverse children of storage
        curr_storage = dir_entries[storage_idx]
        def find_in_tree(root_idx):
            if root_idx >= len(dir_entries) or root_idx == 0xFFFFFFFF:
                return None
            node = dir_entries[root_idx]
            if node['name'] == stream_name and node['type'] == 2:
                return node
            res = find_in_tree(node['left'])
            if res: return res
            return find_in_tree(node['right'])

        stream_entry = find_in_tree(curr_storage['child'])
        assert stream_entry is not None, f"Stream '{stream_name}' in storage '{storage_name}' not found"

        if stream_entry['size'] < mini_cutoff:
            return read_ministream(stream_entry['start'], stream_entry['size'])
        else:
            return read_fat_stream(stream_entry['start'], stream_entry['size'])

    # 4. Rules6/Data Verification
    print("\n--- 4. Rules6/Data Verification ---")
    rules_data = get_stream('Rules6/Data')
    print(f"Rules6/Data size: {len(rules_data)} bytes (expected: 21355)")
    assert len(rules_data) == 21355, f"Expected size 21355, got {len(rules_data)}"

    # Parse every record in Rules6/Data
    pos = 0
    records = []
    soldermask_rule = None
    while pos < len(rules_data):
        assert pos + 6 <= len(rules_data), f"Truncated record header at pos {pos}"
        rtype, rlen = struct.unpack_from('<HI', rules_data, pos)
        assert pos + 6 + rlen <= len(rules_data), f"Record payload exceeds stream at pos {pos}"
        payload = rules_data[pos+6 : pos+6+rlen]
        records.append((len(records), pos, rtype, rlen, payload))
        if b'UNIQUEID=DWAUVLHV' in payload:
            soldermask_rule = (rtype, rlen, payload)
        pos += 6 + rlen

    assert pos == len(rules_data), f"Records did not cleanly span full stream: pos {pos} != {len(rules_data)}"
    print(f"Total rules parsed cleanly: {len(records)} records (expected: 37)")
    assert len(records) == 37, f"Expected 37 records, got {len(records)}"

    assert soldermask_rule is not None, "SolderMaskExpansion rule (UNIQUEID=DWAUVLHV) not found!"
    rtype, rlen, payload = soldermask_rule
    print(f"SolderMaskExpansion rule: type=0x{rtype:04X}, len={rlen}")
    assert rtype == 0x000D, f"Expected rtype 0x000D, got 0x{rtype:04X}"
    assert rlen == 422, f"Expected rlen 422, got {rlen}"
    assert b'RULEKIND=SolderMaskExpansion' in payload
    assert b'UNIQUEID=DWAUVLHV' in payload
    assert b'ISTENTINGTOP=FALSE' in payload, "Missing ISTENTINGTOP=FALSE"
    assert b'ISTENTINGBOTTOM=FALSE' in payload, "Missing ISTENTINGBOTTOM=FALSE"
    assert b'ISTENTINGTOP=TRUE' not in payload, "Forbidden ISTENTINGTOP=TRUE found!"
    assert b'ISTENTINGBOTTOM=TRUE' not in payload, "Forbidden ISTENTINGBOTTOM=TRUE found!"
    print("  -> SolderMaskExpansion un-tenting verified (ISTENTINGTOP=FALSE, ISTENTINGBOTTOM=FALSE)")

    # 5. Components6/Data Verification (S1)
    print("\n--- 5. Components6/Data Verification (S1) ---")
    comp_stream = get_stream('Components6/Data')
    comp_text = comp_stream[6:].decode('latin1', errors='ignore')
    s1_record = None
    for rec in comp_text.split('\x00'):
        if 'SOURCEDESIGNATOR=S1|' in rec:
            s1_record = rec
            break
    assert s1_record is not None, "S1 not found in Components6/Data!"
    s1_props = dict([p.split('=', 1) for p in s1_record.split('|') if '=' in p])
    print(f"S1 X: {s1_props.get('X')}, Y: {s1_props.get('Y')}")
    assert s1_props.get('X') == '1761.8111mil', f"S1 X is {s1_props.get('X')}, expected 1761.8111mil"
    assert s1_props.get('Y') == '679.1339mil', f"S1 Y is {s1_props.get('Y')}, expected 679.1339mil"
    x_mm = float(s1_props['X'].replace('mil','')) * 0.0254
    y_mm = float(s1_props['Y'].replace('mil','')) * 0.0254
    print(f"  -> S1 position: ({x_mm:.2f}, {y_mm:.2f}) mm (expected: 44.75, 17.25 mm)")
    assert round(x_mm, 2) == 44.75
    assert round(y_mm, 2) == 17.25

    # 6. Pads6/Data Verification (S1 4 Pads)
    print("\n--- 6. Pads6/Data Verification (S1 4 Pads) ---")
    pads_stream = get_stream('Pads6/Data')[6:]
    pad_rec_len = 228
    num_pads = len(pads_stream) // pad_rec_len
    print(f"Total pads parsed: {num_pads}")
    all_pads = []
    for i in range(num_pads):
        p = pads_stream[i*pad_rec_len : (i+1)*pad_rec_len]
        px = struct.unpack_from('<i', p, 37)[0] / 393700.787
        py = struct.unpack_from('<i', p, 41)[0] / 393700.787
        all_pads.append((round(px, 2), round(py, 2)))

    expected_s1_pads = [(41.50, 15.00), (41.50, 19.50), (48.00, 15.00), (48.00, 19.50)]
    for ep in expected_s1_pads:
        found = ep in all_pads
        print(f"Pad at ({ep[0]:.2f}, {ep[1]:.2f}) mm present: {found}")
        assert found, f"Expected pad at {ep} not found in Pads6/Data!"
    print("  -> All 4 S1 pads verified at correct positions.")

    # 7. Alignment with USER_BTN & GND Tracks
    print("\n--- 7. Tracks Verification ---")
    nets_stream = get_stream('Nets6/Data')
    net_names = []
    p = 0
    while p < len(nets_stream):
        rt, rl = struct.unpack_from('<HI', nets_stream, p)
        raw = nets_stream[p+6 : p+6+rl].decode('latin1', errors='ignore')
        for item in raw.split('|'):
            if item.startswith('NAME='):
                net_names.append(item[5:])
        p += 6 + rl

    tracks_stream = get_stream('Tracks6/Data')[6:]
    track_rec_len = 54
    num_tracks = len(tracks_stream) // track_rec_len
    user_btn_endpoints = set()
    gnd_endpoints = set()
    for i in range(num_tracks):
        tr = tracks_stream[i*track_rec_len : (i+1)*track_rec_len]
        net_id = struct.unpack_from('<H', tr, 2)[0]
        net_str = net_names[net_id] if 0 <= net_id < len(net_names) else ''
        x1 = round(struct.unpack_from('<i', tr, 12)[0] / 393700.787, 2)
        y1 = round(struct.unpack_from('<i', tr, 16)[0] / 393700.787, 2)
        x2 = round(struct.unpack_from('<i', tr, 20)[0] / 393700.787, 2)
        y2 = round(struct.unpack_from('<i', tr, 24)[0] / 393700.787, 2)
        if net_str == 'USER_BTN':
            user_btn_endpoints.add((x1, y1))
            user_btn_endpoints.add((x2, y2))
        elif net_str == 'GND':
            gnd_endpoints.add((x1, y1))
            gnd_endpoints.add((x2, y2))

    assert (41.50, 15.00) in user_btn_endpoints, "USER_BTN track does not connect to (41.50, 15.00)"
    assert (41.50, 19.50) in user_btn_endpoints, "USER_BTN track does not connect to (41.50, 19.50)"
    assert (48.00, 15.00) in gnd_endpoints, "GND track does not connect to (48.00, 15.00)"
    assert (48.00, 19.50) in gnd_endpoints, "GND track does not connect to (48.00, 19.50)"
    print("  -> USER_BTN and GND track endpoints match S1 pads perfectly!")

    print("\n==========================================")
    print("VERIFICATION RESULT: ALL CHECKS PASSED OK!")
    print("==========================================")

if __name__ == '__main__':
    pcb = sys.argv[1] if len(sys.argv) > 1 else str(DEFAULT_PCB_PATH)
    verify(pcb)
