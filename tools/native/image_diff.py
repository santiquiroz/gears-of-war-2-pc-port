"""Compare two images and report the fraction of pixels that differ.

A pixel counts as different when the largest per-channel absolute difference
between the two images is greater than ``tolerance``.

CLI:
    python tools/native/image_diff.py A.png B.png [--tolerance N] [--crop x,y,w,h] [--max F]

Prints ``diff <ratio>`` (four decimals) and exits with 1 when the ratio is
greater than ``--max``, otherwise 0.
"""

import argparse
import sys

from PIL import Image, ImageChops


def diff_ratio(
    a_path: str,
    b_path: str,
    tolerance: int = 24,
    crop: tuple[int, int, int, int] | None = None,
) -> float:
    a = Image.open(a_path).convert("RGB")
    b = Image.open(b_path).convert("RGB")
    if crop is not None:
        x, y, w, h = crop
        box = (x, y, x + w, y + h)
        a = a.crop(box)
        b = b.crop(box)
    if a.size != b.size:
        raise ValueError(f"image sizes differ: {a.size} vs {b.size}")

    width, height = a.size
    total = width * height
    if total == 0:
        return 0.0

    # Per-channel absolute difference, then the max across R, G, B for each pixel.
    channels = ImageChops.difference(a, b).split()
    max_diff = ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])
    histogram = max_diff.histogram()
    over = sum(histogram[tolerance + 1:])
    return over / total


def _parse_crop(text: str) -> tuple[int, int, int, int]:
    parts = text.split(",")
    if len(parts) != 4:
        raise argparse.ArgumentTypeError("--crop expects x,y,w,h")
    try:
        x, y, w, h = (int(p) for p in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("--crop values must be integers") from exc
    return x, y, w, h


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compare two images by pixel difference.")
    parser.add_argument("a", help="first image")
    parser.add_argument("b", help="second image")
    parser.add_argument("--tolerance", type=int, default=24, help="per-channel tolerance (default 24)")
    parser.add_argument("--crop", type=_parse_crop, default=None, help="crop box as x,y,w,h")
    parser.add_argument("--max", dest="max_ratio", type=float, default=0.05, help="maximum allowed ratio (default 0.05)")
    args = parser.parse_args(argv)

    ratio = diff_ratio(args.a, args.b, tolerance=args.tolerance, crop=args.crop)
    print(f"diff {ratio:.4f}")
    return 1 if ratio > args.max_ratio else 0


if __name__ == "__main__":
    sys.exit(main())
