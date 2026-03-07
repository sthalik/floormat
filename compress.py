#!/usr/bin/env python
import sys, zlib
sys.stdout.buffer.write(zlib.compress(sys.stdin.buffer.read(), 9))
