"""
Fix ECU Carrier Board PCB:
1. Back up Thesis/ECU_Carrier_Board/ECU_Carrier_PCB.PcbDoc to Thesis/ECU_Carrier_Board/History/ECU_Carrier_PCB.backup_before_fix.PcbDoc
2. Extract ECU_Carrier_PCB.~(18).PcbDoc from Thesis/ECU_Carrier_Board/History/ECU_Carrier_PCB.~(18).PcbDoc.Zip
3. Patch Rules6/Data stream (un-tent solder mask: ISTENTINGTOP=FALSE, ISTENTINGBOTTOM=FALSE) and update OLE2 directory entry size
4. Write patched document to Thesis/ECU_Carrier_Board/ECU_Carrier_PCB.PcbDoc
"""
import os
import shutil
import struct
import zipfile

SOURCE_ZIP = 'Thesis/ECU_Carrier_Board/History/ECU_Carrier_PCB.~(18).PcbDoc.Zip'
ZIP_INNER_NAME = 'ECU_Carrier_PCB.~(18).PcbDoc'
CURRENT_PCB = 'Thesis/ECU_Carrier_Board/ECU_Carrier_PCB.PcbDoc'
BACKUP_PCB = 'Thesis/ECU_Carrier_Board/History/ECU_Carrier_PCB.backup_before_fix.PcbDoc'

def main():
    print(f"Step 1: Backing up '{CURRENT_PCB}' to '{BACKUP_PCB}'...")
    assert os.path.exists(CURRENT_PCB), f"Current PCB file not found: {CURRENT_PCB}"
    shutil.copy2(CURRENT_PCB, BACKUP_PCB)
    assert os.path.exists(BACKUP_PCB), f"Backup failed: {BACKUP_PCB}"
    print(f"  Backup created successfully (size: {os.path.getsize(BACKUP_PCB)} bytes)")

    print(f"\nStep 2: Extracting '{ZIP_INNER_NAME}' from '{SOURCE_ZIP}'...")
    with zipfile.ZipFile(SOURCE_ZIP) as z:
        raw_pcb = bytearray(z.read(ZIP_INNER_NAME))
    print(f"  Extracted {len(raw_pcb)} bytes.")

    print("\nStep 3: Parsing OLE2 CFB structures and patching Rules6/Data...")
    # Validate CFB header
    magic = raw_pcb[:8]
    assert magic == b'\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1', "Invalid OLE2 magic header"

    sec_size = 1 << struct.unpack_from('<H', raw_pcb, 0x1E)[0]
    fat_count = struct.unpack_from('<I', raw_pcb, 0x2C)[0]
    dir_start = struct.unpack_from('<I', raw_pcb, 0x30)[0]
    mini_cutoff = struct.unpack_from('<I', raw_pcb, 0x38)[0]
    fat_sectors = [s for s in struct.unpack_from('<109I', raw_pcb, 0x4C) if s < 0xFFFFFFFC]

    print(f"  Sector size: {sec_size}, FAT count: {fat_count}, Dir start: {dir_start}, Mini cutoff: {mini_cutoff}")

    fat = []
    for s in fat_sectors[:fat_count]:
        offset = (s + 1) * sec_size
        fat.extend(struct.unpack(f'<{sec_size//4}I', raw_pcb[offset:offset+sec_size]))

    def get_chain(start):
        chain = []
        curr = start
        while curr < 0xFFFFFFFC:
            chain.append(curr)
            curr = fat[curr]
        return chain

    dir_chain = get_chain(dir_start)

    # Locate Rules6 storage and Rules6/Data stream directory entry
    rules_data_entry_idx = None
    storage_name = ''
    for idx in range(len(dir_chain) * sec_size // 128):
        dir_sec = dir_chain[idx * 128 // sec_size]
        offset_in_sec = (idx * 128) % sec_size
        ent_off = (dir_sec + 1) * sec_size + offset_in_sec
        ent = raw_pcb[ent_off : ent_off + 128]
        name_len = struct.unpack_from('<H', ent, 0x40)[0]
        if name_len > 0:
            name = ent[:name_len-2].decode('utf-16le', errors='ignore')
            obj_type = ent[0x42]
            if obj_type == 1:
                storage_name = name
            elif obj_type == 2:
                if storage_name == 'Rules6' and name == 'Data':
                    rules_data_entry_idx = idx
                    rules_data_ent_off = ent_off
                    break

    assert rules_data_entry_idx is not None, "Could not find Rules6/Data directory entry"
    start_sec = struct.unpack_from('<I', raw_pcb, rules_data_ent_off + 0x74)[0]
    curr_stream_size = struct.unpack_from('<Q', raw_pcb, rules_data_ent_off + 0x78)[0]
    print(f"  Rules6/Data located at dir entry {rules_data_entry_idx}, start sector {start_sec}, size {curr_stream_size}")
    assert curr_stream_size >= mini_cutoff, "Rules6/Data is expected to be in regular FAT sectors"

    # Get Rules6/Data sector chain
    rules_chain = get_chain(start_sec)
    print(f"  Rules6/Data sector chain: {len(rules_chain)} sectors ({len(rules_chain) * sec_size} bytes allocated)")
    assert len(rules_chain) * sec_size >= curr_stream_size + 2, "Chain has insufficient slack space for +2 bytes"

    # Read current stream
    stream_bytes = bytearray()
    for s in rules_chain:
        off = (s + 1) * sec_size
        stream_bytes.extend(raw_pcb[off : off + sec_size])
    stream_data = bytes(stream_bytes[:curr_stream_size])

    # Find SolderMaskExpansion rule record (UNIQUEID=DWAUVLHV)
    pos = 0
    target_pos = None
    target_rtype = None
    target_rlen = None
    records = []
    while pos < len(stream_data):
        rtype, rlen = struct.unpack_from('<HI', stream_data, pos)
        payload = stream_data[pos+6 : pos+6+rlen]
        records.append((pos, rtype, rlen, payload))
        if b'UNIQUEID=DWAUVLHV' in payload:
            target_pos = pos
            target_rtype = rtype
            target_rlen = rlen
        pos += 6 + rlen

    assert target_pos is not None, "Rule with UNIQUEID=DWAUVLHV not found"
    print(f"  Found SolderMaskExpansion record at offset {target_pos}, type=0x{target_rtype:04X}, len={target_rlen}")

    old_payload = stream_data[target_pos+6 : target_pos+6+target_rlen]
    assert b'ISTENTINGTOP=TRUE' in old_payload, "Expected ISTENTINGTOP=TRUE"
    assert b'ISTENTINGBOTTOM=TRUE' in old_payload, "Expected ISTENTINGBOTTOM=TRUE"

    new_payload = old_payload.replace(b'ISTENTINGTOP=TRUE', b'ISTENTINGTOP=FALSE')
    new_payload = new_payload.replace(b'ISTENTINGBOTTOM=TRUE', b'ISTENTINGBOTTOM=FALSE')
    assert len(new_payload) == len(old_payload) + 2, "Payload length did not increase by exactly +2 bytes"

    # Build new stream
    new_stream = (
        stream_data[:target_pos] +
        struct.pack('<HI', target_rtype, len(new_payload)) +
        new_payload +
        stream_data[target_pos + 6 + target_rlen:]
    )
    new_stream_size = curr_stream_size + 2
    assert len(new_stream) == new_stream_size

    # Verify all records parse in new stream
    p = 0
    new_records = []
    while p < len(new_stream):
        rt, rl = struct.unpack_from('<HI', new_stream, p)
        pl = new_stream[p+6 : p+6+rl]
        new_records.append((p, rt, rl, pl))
        p += 6 + rl
    assert p == new_stream_size, f"Stream records parsing failed to reach end: {p} vs {new_stream_size}"
    assert len(new_records) == len(records), f"Record count changed: {len(new_records)} vs {len(records)}"
    print(f"  All {len(new_records)} records parsed successfully in patched stream.")

    # Write patched stream back into the allocated sectors of raw_pcb
    # Note: sectors in rules_chain are contiguous 1456..1497
    bytes_written = 0
    for s in rules_chain:
        off = (s + 1) * sec_size
        chunk = new_stream[bytes_written : bytes_written + sec_size]
        if chunk:
            raw_pcb[off : off + len(chunk)] = chunk
            bytes_written += len(chunk)
    assert bytes_written == new_stream_size, "Did not write all stream bytes"

    # Update directory entry size (+2 bytes)
    struct.pack_into('<Q', raw_pcb, rules_data_ent_off + 0x78, new_stream_size)
    updated_size = struct.unpack_from('<Q', raw_pcb, rules_data_ent_off + 0x78)[0]
    assert updated_size == new_stream_size, "Failed to update directory entry size"
    print(f"  Updated Directory Entry size: {curr_stream_size} -> {updated_size}")

    # Write output file
    print(f"\nWriting patched document to '{CURRENT_PCB}'...")
    with open(CURRENT_PCB, 'wb') as f:
        f.write(raw_pcb)
    print(f"  File saved: {os.path.getsize(CURRENT_PCB)} bytes.")
    print("Done!")

if __name__ == '__main__':
    main()
