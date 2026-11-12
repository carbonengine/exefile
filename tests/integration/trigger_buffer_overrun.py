# Copyright © 2026 CCP ehf.
"""Writes one byte past the end of a bytearray's buffer and frees it, which CPython's debug allocator hooks report."""
import ctypes

buffer = bytearray(16)
end = ctypes.addressof(ctypes.c_char.from_buffer(buffer)) + buffer.__alloc__()
ctypes.memset(end, 0, 1)
del buffer
