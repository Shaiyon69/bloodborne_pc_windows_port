#!/usr/bin/env python3
# bbport: records FSR 4.1.1 (the DLL's own 4.1.x provider) for every output size class and quality
# ratio vk_fsr411.cpp can be asked for, into capture_<render>_<output>/ next to fsr4cap.exe.
#
#   capture_all.py <dir with fsr4cap.exe, the loader and the upscaler DLL> [--frames N]
#
# The Linux original drove the DLL through umu-run (Proton, vkd3d-proton). On Windows D3D12 is
# native, so the exe runs directly; the DLL still has to accept this GPU (AMD's own file limits
# the INT8 model to some cards).
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

OUTPUTS = ["1920x1080", "2560x1440", "3840x2160"]
RATIOS = [1.0, 1.5, 1.7, 2.0, 3.0]
# Steam Deck, 1600x900 and the ultrawides, whose tensor widths are not multiples of 64.
EXTRA = [("853x533", "1280x800"), ("640x400", "1280x800"), ("1067x600", "1600x900"),
         ("1707x720", "2560x1080"), ("2293x960", "3440x1440")]


def render_size(output: str, ratio: float) -> str:
    """The render size the preset asks for, rounded as the capture script always rounded it."""
    width, height = (int(v) for v in output.split("x"))
    return f"{int(width / ratio + 0.5)}x{int(height / ratio + 0.5)}"


def record(work: Path, render: str, output: str, frames: int) -> bool:
    out_dir = work / f"capture_{render}_{output}"
    if out_dir.exists():
        shutil.rmtree(out_dir)
    done = subprocess.run([str(work / "fsr4cap.exe"), "4.1.1", render, output, str(frames)],
                          cwd=str(work), capture_output=True, text=True)
    log = work / "fsr4cap.log"
    last = ""
    if log.is_file():
        lines = [line for line in log.read_text(encoding="utf-8", errors="replace").splitlines() if line]
        last = lines[-1] if lines else ""
    ok = done.returncode == 0 and out_dir.is_dir()
    print(f"{output:<9} <- {render:<9} {'ok ' if ok else 'FAILED '}{last}", flush=True)
    if not ok and done.stderr.strip():
        print(f"  {done.stderr.strip().splitlines()[-1]}", flush=True)
    return ok


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("work", help="directory holding fsr4cap.exe and both AMD DLLs")
    parser.add_argument("--frames", type=int, default=3, help="frames per capture (default 3)")
    args = parser.parse_args()

    work = Path(args.work).resolve()
    for needed in ("fsr4cap.exe", "amd_fidelityfx_loader_dx12.dll", "amd_fidelityfx_upscaler_dx12.dll"):
        if not (work / needed).is_file():
            print(f"missing {needed} in {work}", file=sys.stderr)
            return 1

    jobs = [(render_size(output, ratio), output) for output in OUTPUTS for ratio in RATIOS] + EXTRA
    failed = [job for job in jobs if not record(work, job[0], job[1], args.frames)]
    print(f"{len(jobs) - len(failed)} of {len(jobs)} captures recorded")
    if failed:
        print("failed: " + ", ".join(f"{r}->{o}" for r, o in failed), file=sys.stderr)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
