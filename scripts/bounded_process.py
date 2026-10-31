"""Bound captured compiler output while the process is running."""
import math
import os
import signal
import subprocess
import threading


def run_bounded(argv, timeout=30, max_bytes=1024 * 1024):
    # Validate before spawning. A float/NaN/Infinity byte limit used to pass
    # the original comparison but then crash a pipe-reader thread at the
    # slice expression, losing output without a fail-closed error.
    if (type(timeout) not in (int, float) or not 0 < timeout <= 3600
            or not math.isfinite(timeout)):
        raise ValueError('finite positive timeout <=3600 seconds required')
    if type(max_bytes) is not int or not 0 < max_bytes <= 16 * 1024 * 1024:
        raise ValueError('integer output budget in (0, 16 MiB] required')
    # A compiler can spawn helpers that inherit stdout/stderr. Isolate their
    # process group so failure or timeout cannot leave descendants running.
    process = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               start_new_session=(os.name == 'posix'))

    def kill_tree():
        if os.name == 'posix':
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        else:
            try:
                process.kill()
            except ProcessLookupError:
                pass

    chunks = [bytearray(), bytearray()]
    lock = threading.Lock()
    exceeded = threading.Event()
    reader_errors = []
    total = 0

    def drain(pipe, slot):
        nonlocal total
        try:
            while True:
                block = pipe.read(8192)
                if not block:
                    break
                with lock:
                    remaining = max_bytes - total
                    chunks[slot].extend(block[:max(0, remaining)])
                    total += len(block)
                    if total > max_bytes:
                        exceeded.set()
                        kill_tree()
        except Exception as error:
            # Background thread errors must not be silently dropped and turn
            # an incomplete compiler transcript into a successful receipt.
            with lock:
                reader_errors.append(error)
            kill_tree()
        finally:
            pipe.close()

    readers = [threading.Thread(target=drain, args=(pipe, i), daemon=True)
               for i, pipe in enumerate((process.stdout, process.stderr))]
    for reader in readers:
        reader.start()
    timed_out = False
    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        kill_tree()
        process.wait()
    finally:
        for reader in readers:
            reader.join(timeout=1)
        lingering = any(reader.is_alive() for reader in readers)
        if lingering:
            # A helper inherited a pipe even after the compiler exited.
            # Terminate the whole dedicated group, then drain and reject.
            kill_tree()
            for reader in readers:
                reader.join(timeout=1)
    if any(reader.is_alive() for reader in readers):
        raise RuntimeError('compiler output pipes remained open after process-group kill')
    if reader_errors:
        raise RuntimeError('compiler output reader failed') from reader_errors[0]
    if timed_out:
        raise subprocess.TimeoutExpired(argv, timeout)
    if lingering:
        raise RuntimeError('compiler descendants held captured output pipes open')
    if exceeded.is_set():
        raise RuntimeError('compiler output budget exceeded')
    return subprocess.CompletedProcess(argv, process.returncode,
                                       chunks[0].decode('utf-8'), chunks[1].decode('utf-8'))
