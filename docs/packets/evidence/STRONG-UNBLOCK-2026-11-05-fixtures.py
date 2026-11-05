"""Independent wire examples from reviewed server writes, never client serializers.

Run from the repository root. Writes only the named contract fixture JSONs.
These are source-derived examples, not captures or production replay results.
"""
import json
import struct
from pathlib import Path

DEST = Path(__file__).resolve().parents[1] / 'contracts'


def fixed(text, size):
    encoded = text.encode('ascii')
    assert len(encoded) <= size
    return encoded.ljust(size, b'\0')


def write(name, obj):
    (DEST / name).write_text(json.dumps(obj, indent=2) + '\n', encoding='utf-8')


rows = []
for bit, effect, option in ((1, 166, 130), (2, 166, 128), (4, 166, 169), (0x4000, 617, None)):
    rows.append(dict(actor='PC-visible', bit=hex(bit), effects=[effect],
                     selector='first present in listed selector order', message44_option=option))
for bit, effects in ((8, [203]), (0x10, [254]), (0x40, [474]), (0x80, [368]),
                     (0x100, [382]), (0x200, [394]), (0x400, [396]), (0x800, [375]),
                     (0x1000, [418, 544]), (0x2000, [419]), (0x8000, [424]),
                     (0x10000, [655]), (0x20000, [586])):
    rows.append(dict(actor='PC-visible', bit=hex(bit), effects=effects,
                     start_guard='first listed effect absent', clear='shared-owner reconciliation; no blanket Ref PC clear branch'))
for bit, effect, terminal in ((2, 420, 540), (4, 427, None), (0x10, 254, None),
                              (0x10000, 655, 656), (0x8000, 667, None)):
    rows.append(dict(actor='nonPC-visible', bit=hex(bit), effects=[effect],
                     start_guard='effect absent', clear='remove all matching Ref instances',
                     terminal_per_removed=terminal))
write('PK22.SHOWEFST-LIFETIME.json', dict(
    profile=20080910, authority='Ref GameModeShowEFST plus selected clif_changeoption2/e_sc_opt3',
    scope='branch specification; no implementation or lifetime test certificate',
    wire={'size': 18, 'GID': 'u32@2', 'OPTION': 'u32@6', 'level': 'i32@10', 'OPT3': 'u32@14'},
    hide_branch_input='current actor OPTION; incoming OPTION separately gates level-99 message',
    hidden_remove_one={'PC': [203, 254, 418, 586], 'nonPC': [203, 254, 418]},
    blade_stop={'bit': '0x20', 'gray': [64, 64, 64], 'PC_forced_animation_jobs': [15, 4016, 4038],
                'other_actors': 'motion freeze', 'clear': 'recompute from independent owner claims'},
    branches=rows))

mail_id = 0x11223344
row = struct.pack('<I', mail_id) + fixed('T' * 40, 40) + b'\0' + fixed('Alice', 24) + struct.pack('<I', 1700000000)
assert len(row) == 73
list_one = struct.pack('<HHI', 0x240, 81, 1) + row
body = b'Hello'
read = bytearray(101 + len(body))
struct.pack_into('<HHI', read, 0, 0x242, len(read), mail_id)
read[8:48], read[48:72] = fixed('Gift', 40), fixed('Alice', 24)
struct.pack_into('<IIIHHBBB4H', read, 72, 0, 12345, 2, 501, 0, 1, 0, 0, 0, 0, 0, 0)
read[99] = len(body)
read[100:] = body + b'\0'
read_empty = bytearray(113)
struct.pack_into('<HHI', read_empty, 0, 0x242, len(read_empty), mail_id)
read_empty[99] = 12
read_empty[100:] = b'(no message)\0'
assert len(read_empty) == 113
fixtures = [
    ('list-empty', struct.pack('<HHI', 0x240, 8, 0), {'count': 0}),
    ('list-one-full-width-title', list_one, {'count': 1, 'mail_id': mail_id, 'title_bytes': 40, 'read': False}),
    ('read-item-and-zeny', read, {'mail_id': mail_id, 'zeny': 12345, 'amount': 2, 'item_id': 501, 'body': 'Hello'}),
    ('read-no-item-empty-source-body', read_empty, {'body': '(no message)', 'amount': 0, 'zeny': 0}),
    ('unread-refresh-id-zero', struct.pack('<HI', 0x24a, 0) + fixed('Gift', 40) + fixed('Alice', 24), {'insert_row': False}),
    ('missing-read-return-failure', struct.pack('<HIH', 0x274, mail_id, 1), {'pending_operation': 'read', 'failed_id': mail_id}),
    ('claim-success', struct.pack('<HB', 0x245, 0), {'credit_inventory_or_zeny': False}),
    ('claim-capacity', struct.pack('<HB', 0x245, 2), {'credit_inventory_or_zeny': False}),
]
write('PK32.LEGACY-MAIL-fixtures.json', dict(
    profile=20080910, origin='independently constructed from selected server WFIFO offsets; no client serializer',
    scope='wire examples; not captures, escrow certification or production replay',
    fixtures=[dict(name=name, hex=bytes(data).hex(), size=len(data), expected=expected) for name, data, expected in fixtures],
    sequence_requirements=[
        {'name': 'one-claim-two-successes', 'request_id': mail_id,
         'ordered_inputs': ['authoritative item update', 'claim-success', 'authoritative zeny update', 'claim-success'],
         'expected': 'retain original request ID; no resource credit from ack; reconcile without retargeting'},
        {'name': 'read-refresh-unread', 'ordered_inputs': ['read-item-and-zeny', 'list-one-full-width-title', 'unread-refresh-id-zero'],
         'expected': 'list/read state by mail ID; notification zero never becomes a mail'},
    ]))
print('PASS: generated 22 ShowEFST branch rows and 8 independent legacy-mail wire examples; no production replay claimed.')
