#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt

x = np.linspace(0, 1, 1000)

# Linear function
y_linear = x

# Step function
y_step = np.piecewise(
    x,
    [
        x < 0.125,
        (x >= 0.125) & (x < 0.375),
        (x >= 0.375) & (x < 0.625),
        (x >= 0.625) & (x < 0.875),
        x >= 0.875
    ],
    [0.0, 1/4, 2/4, 3/4, 1.0]
)

y_step2 = np.piecewise(
    x,
    [
        x < 0.0625,
        (x >= 0.0625) & (x < 0.1875),
        (x >= 0.1875) & (x < 0.3125),
        (x >= 0.3125) & (x < 0.4375),
        (x >= 0.4375) & (x < 0.5625),
        (x >= 0.5625) & (x < 0.6875),
        (x >= 0.6875) & (x < 0.8125),
        (x >= 0.8125) & (x < 0.9375),
        x >= 0.9375
    ],
    [0/8, 1/8, 2/8, 3/8, 4/8, 5/8, 6/8, 7/8, 1.0]
)

# Smooth cos²-like ramp from 0 → 1 over full domain
y_cos2 = np.sin((np.pi / 2.0) * x) ** 2

y_cos2_shift = np.sin((np.pi / 2.0) * (x+0.125)/1.25) ** 2



# Plot
plt.figure(figsize=(8, 6))

plt.plot(x, y_linear, label="y = x", linewidth=2)
plt.step(x, y_step, where="post", label="PSM subgrid of 8", linewidth=2)
plt.step(x, y_step2, where="post", label="PSM subgrid of 8", linewidth=2)
plt.plot(x, y_cos2, label=r'HLBM e_h:cell size 1:1', linewidth=2)
plt.plot(x, y_cos2_shift, label=r'HLBM e_h:cell size 1.25:1', linewidth=2)

plt.xlim(0, 1)
plt.ylim(0, 1)
plt.grid(True)
plt.legend()

plt.xlabel("x")
plt.ylabel("y")
plt.title("Linear + Step + Smooth Cos² Ramp")

plt.show()
