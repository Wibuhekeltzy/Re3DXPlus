#!/bin/sh
# Regenerate the recompiled arcade physics from the arcade's `main` executable:
#   src/phys/phys_rc.c  (x86 -> C, the functions reachable from roots.txt)
#   data/phys.img       (main's rodata..bss after the game-logic static constructors)
# Needs python3 with capstone and unicorn (pip install capstone unicorn), and c++filt.
# Usage: tools/recomp/build_phys.sh [path/to/main]
set -e
cd "$(dirname "$0")"
export WMMT_MAIN="${1:-../../../Wangan Midnight Maximum Tune 3DX+ (Japan)/main}"
python3 recomp.py ../../src/phys/phys_rc.c $(cat roots.txt)
# the one unbounded walk the original code has (CAreaProcessorJunction::MoveToPrev reads
# area_array[-1] at section 0: a crash seen on a phone): clamped in the generated file, see
# guard_sections.py. Idempotent, and best effort: where it finds the code other than it expects it
# says so and the build goes on without it.
python3 guard_sections.py ../../src/phys/phys_rc.c || echo "guard_sections: not applied, the physics is as the original code is"
python3 mkimage.py ../../data/phys.img
