#!/usr/bin/env python3

import argparse
import numpy as np


def main():
    parser = argparse.ArgumentParser(description="Compute the L2 Norm");
    parser.add_argument("one");
    parser.add_argument("two");
    args = parser.parse_args();

    a = np.loadtxt(args.one, delimiter=",");
    b = np.loadtxt(args.two, delimiter=",");

    if a.shape != b.shape:
        raise ValueError(f"Shape mismatch: {a.shape} vs {b.shape}");
    #endif

    print(np.linalg.norm(a-b));

#enddef

if __name__ == "__main__":
    main();
#enddef
