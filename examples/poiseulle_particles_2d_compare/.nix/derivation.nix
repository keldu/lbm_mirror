{ lib
, stdenv
, python3
}:

{ pkgs ? import <nixpkgs> {} }:

let

  pythonEnv = python3.withPackages (ps: with ps; [
    pandas
    matplotlib
    numpy
    imageio        # for video output
    imageio-ffmpeg # ffmpeg backend for imageio
  ]);

  # The comparison & video generation script (embedded)
  compareAndRenderPy = pkgs.writeText "compare_and_render.py" ''
    import pandas as pd
    import matplotlib.pyplot as plt
    import numpy as np
    import argparse
    import os
    import imageio.v2 as iio
    from matplotlib.animation import FuncAnimation, FFMpegWriter

    def main():
        parser = argparse.ArgumentParser()
        parser.add_argument('--csv1', required=True)
        parser.add_argument('--csv2', required=True)
        parser.add_argument('--csv3', required=True)
        parser.add_argument('--video-dir', required=True)
        args = parser.parse_args()

        df1 = pd.read_csv(args.csv1)
        df2 = pd.read_csv(args.csv2)
        df3 = pd.read_csv(args.csv3)

        times = df1['time'].values
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))

        def update(frame):
            ax1.clear()
            ax2.clear()
            ax1.set_title(f'Velocity at t = {times[frame]:.2f}')
            ax2.set_title(f'Pressure at t = {times[frame]:.2f}')
            ax1.plot(times[:frame+1], df1['velocity'][:frame+1], label='Binary1')
            ax1.plot(times[:frame+1], df2['velocity'][:frame+1], label='Binary2')
            ax1.plot(times[:frame+1], df3['velocity'][:frame+1], label='Binary3')
            ax2.plot(times[:frame+1], df1['pressure'][:frame+1], label='Binary1')
            ax2.plot(times[:frame+1], df2['pressure'][:frame+1], label='Binary2')
            ax2.plot(times[:frame+1], df3['pressure'][:frame+1], label='Binary3')
            ax1.legend(), ax2.legend()
            ax1.set_xlim(times[0], times[-1])
            ax2.set_xlim(times[0], times[-1])

        ani = FuncAnimation(fig, update, frames=len(times), repeat=False)
        writer = FFMpegWriter(fps=5, bitrate=1800)
        os.makedirs(args.video_dir, exist_ok=True)
        video_path = os.path.join(args.video_dir, 'comparison.mp4')
        ani.save(video_path, writer=writer)
        print(f"Video saved to {video_path}")

    if __name__ == '__main__':
        main()
  '';

  # Wrapper script that runs the three binaries and then the Python comparison
  runScript = pkgs.writeShellScript "run-simulation-pipeline" ''
    set -e
    mkdir -p results videos

    echo "Running binary1..."
    ${binary1}/bin/sim1
    echo "Running binary2..."
    ${binary2}/bin/sim2
    echo "Running binary3..."
    ${binary3}/bin/sim3

    echo "Generating comparison video..."
    ${pythonEnv}/bin/python ${compareAndRenderPy} \
      --csv1 results/binary1.csv \
      --csv2 results/binary2.csv \
      --csv3 results/binary3.csv \
      --video-dir videos

    echo "Done. Video available at videos/comparison.mp4"
  '';

in
pkgs.stdenv.mkDerivation {
  name = "simulation-pipeline";
  buildInputs = [ hlbm_2d psm_2d bgk_2d pythonEnv pkgs.ffmpeg ]; # ffmpeg for video encoding
  buildCommand = ''
    mkdir -p $out/bin
    cp ${runScript} $out/bin/run-pipeline
    chmod +x $out/bin/run-pipeline
    # Optionally copy the Python script for reference
    cp ${compareAndRenderPy} $out/compare_and_render.py
    echo "Pipeline installed. Run $out/bin/run-pipeline"
  '';
}
