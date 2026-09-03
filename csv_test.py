#!/usr/bin/env python3

import argparse
import numpy as np


def main():
    parser = argparse.ArgumentParser(
        description="Calculate drag coefficient from LBM CSV data."
    )

    parser.add_argument("force_csv")
    parser.add_argument("momentum_csv")
    parser.add_argument("density_csv")

    parser.add_argument("--U", type=float, default=0.003)
    parser.add_argument("--D", type=float, default=10.0)
    parser.add_argument("--rho", type=float, default=1.0)

    args = parser.parse_args()

    # Load CSVs
    force = np.loadtxt(args.force_csv, delimiter=",")
    momentum = np.loadtxt(args.momentum_csv, delimiter=",")
    density = np.loadtxt(args.density_csv, delimiter=",")

    # If each file is just a scalar field:
    Fx = force
    px = momentum
    rho = density

    # Total force
    F_drag = abs(np.sum(Fx))

    # 2D cylinder, unit depth
    A_ref = args.D

    # Dynamic pressure
    q = 0.5 * args.rho * args.U**2

    # Drag coefficient
    Cd = F_drag / (q * A_ref)

    print(f"Total Fx       = {np.sum(Fx):.8e}")
    print(f"Drag force     = {F_drag:.8e}")
    print(f"Reference area = {A_ref:.8e}")
    print(f"Cd             = {Cd:.8f}")

    print("\nDiagnostics:")
    print(f"rho: min={rho.min():.6e}, max={rho.max():.6e}, mean={rho.mean():.6e}")
    print(f"px:  min={px.min():.6e}, max={px.max():.6e}, mean={px.mean():.6e}")
    print(f"Fx:  min={Fx.min():.6e}, max={Fx.max():.6e}, mean={Fx.mean():.6e}")


if __name__ == "__main__":
    main()
