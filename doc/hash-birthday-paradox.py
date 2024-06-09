#!/usr/bin/env python
import math
N = 2**16
probUnique = 1.0
for k in range(1, 10_000):
    val = 1 - math.exp(-0.5 * k * (k - 1) / N)
    # probAllUnique = probUnique * (N - (k - 1)) / N
    #print(k, 1 - probAllUnique, val)
    print(k, val)
    if val + 1e-12 > 1:
        break
