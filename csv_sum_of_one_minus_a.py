#!/usr/bin/env python3

import argparse
import numpy as np


def main():
    parser = argparse.ArgumentParser(description="Compute the sum of 1-a")
    parser.add_argument("one")
    args = parser.parse_args()

    a = np.loadtxt(args.one, delimiter=",")

    print(np.sum(1 - a, axis=0))


if __name__ == "__main__":
    main()
