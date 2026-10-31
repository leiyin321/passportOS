#!/usr/bin/env python3
"""Check recorded on-device acceptance, including protection-fault controls."""
import re
import sys
from pathlib import Path
root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / 'logs'
normal = (root / 'pool-device.log').read_text()
assert 'memprot=enabled' in normal
assert normal.count('first=42 second=43 request_rc=0 result=PASS') == 2
assert normal.count('exec_drift=0 8bit_drift=0 result=PASS') >= 3
assert normal.count('POOL after_unload used=0 all_zero=1') == 2
assert 'zero_rejected=1 oversize_rejected=1' in normal
assert normal.count('POOL busy_rejected=1') == 2
assert 'capacity=16384 used=4588 peak=4588' in normal
assert 'exec_free=0 exec_min=0 exec_largest=0' in normal
assert 'rc=-22' in normal
assert 'result=FAIL' not in normal and 'Guru Meditation' not in normal
for mode, fault in [('heap', 'Illegal instruction'), ('text', 'Memory protection fault')]:
    log = (root / f'pool-{mode}-device.log').read_text()
    arena = re.search(r'ready=1 verify=0 begin=(0x[0-9a-f]+) end=(0x[0-9a-f]+)', log)
    target = re.search(r'PROBE .* target=(0x[0-9a-f]+)', log)
    assert arena and target and fault in log
    lo, hi = (int(v, 16) for v in arena.groups())
    assert not lo <= int(target.group(1), 16) < hi
    assert 'UNEXPECTED_SUCCESS' not in log
assert '-- verify OK (digest matched)' in (root / 'pool-restore-verify.log').read_text()
os = (root / 'pool-os-restored.log').read_text()
assert 'PASSPORT list count=1' in os
assert 'APP start id=org.passport.xiaozhi ok=1' in os
assert 'METRIC exit' in os and 'lua=0' in os
print('PASS protected load/call/reload, bounded pool, zeroization, zero heap drift, rejected relocation')
print('PASS heap execution failed at outside-pool target; observed Illegal instruction')
print('PASS outside-pool IRAM write raised Memory protection fault')
print('PASS full Flash restoration digest and original Lua start/exit')
