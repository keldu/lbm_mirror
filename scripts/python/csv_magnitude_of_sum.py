#!/usr/bin/env python3

import argparse
import numpy as np


def main():
    parser = argparse.ArgumentParser(description="Compute the sum of magnitude");
    parser.add_argument("one");
    args = parser.parse_args();

    a = np.loadtxt(args.one, delimiter=",");

    print(np.linalg.norm(np.sum(a,axis=0)));
#enddef

if __name__ == "__main__":
    main();
#enddef
