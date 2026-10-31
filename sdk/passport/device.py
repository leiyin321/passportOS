"""Bounded command completion and optional observation on one serial connection."""
import re
import time


def read_completion(port, *, timeout=8, observe=0, verbose=False,
                    clock=time.monotonic, emit=print):
    if not 0 <= observe <= 60:
        raise ValueError('observe must be between 0 and 60 seconds')
    deadline = clock() + timeout
    completed = False
    while clock() < deadline:
        line = port.readline().decode(errors='replace').strip()
        if not line:
            continue
        if verbose or 'PASSPORT' in line or 'METRIC' in line:
            emit(line)
        if re.match(r'^PASSPORT \w+ FAIL(?: |$)', line):
            raise ValueError(line)
        if not completed and (line == 'PASSPORT DONE' or line.startswith('PASSPORT install OK')):
            completed = True
            if not observe:
                return
            deadline = clock() + observe
    if not completed:
        raise ValueError('device protocol timeout: completion was not received')
