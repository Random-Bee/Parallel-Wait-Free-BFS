#!/usr/bin/env bash
set -euo pipefail

# The experiments with stalled threads: LiveJournal from vertex 0 on 16
# threads pinned to NUMA node 0, with memory there too. Built with -DSTALLS
# (stall.hpp), each of the last BFS_STALLED threads busy-waits BFS_STALL_US
# microseconds when it starts its work in a level or round: wf-dir.cpp and
# wf-dir-mem.cpp at the top of every level, GAPBS at the start of every
# top-down and bottom-up step, GBBS and PASGAL at the first vertex or edge a
# worker takes in a round. Two experiments:
#   count   0, 1, 2, 4, 8, 12 or 15 of the 16 threads stalled, 1000 us each
#   length  1 of the 16 threads stalled, 0 to 10000 us
# Each program is built once, with the benchmark's flags and its code unshifted.
# A run is 20 searches, and every setting runs RUNS times (5 by default) for
# each program; the programs take turns, so drift reaches them all alike. A run
# is correct when its distances match a sequential BFS's. Rows go to
# results/stalls.csv, and per setting the median of the runs to
# results/stalls-summary.csv; for our programs, last_us is when the last thread
# returned, which stalled threads put off. A stopped run picks up where it left
# off, as a run already correct is not run again. It runs for about an hour:
#   setsid nohup ./stalls.sh > results/stalls.log 2>&1 &

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(dirname "$here")
runs=${RUNS:-5}
threads=16

cxx=/opt/rh/gcc-toolset-15/root/usr/bin/g++
graph="$root/datasets/liveJournal1.edges"
ref="$root/WF_BFS/tmp/seq-liveJournal1.txt"
lock="$root/WF_BFS/tmp/bench.lock"
build="$here/build"
results="$here/results"
out="$results/stalls.csv"
summary="$results/stalls-summary.csv"
programs=(wd mem gapbs gbbs pasgal)

# a setting a line: experiment, threads stalled, microseconds per stall
settings=$(
    for k in 0 1 2 4 8 12 15; do echo "count $k 1000"; done
    for us in 0 10 20 50 100 200 500 1000 2000 5000 10000; do echo "length 1 $us"; done
)

for file in "$graph" "$ref"; do
    [ -f "$file" ] || { echo "missing $file" >&2; exit 1; }
done

declare -A done_run
if [ -f "$out" ]; then
    while IFS=, read -r experiment stalled us run name time last correct finished; do
        if [ "$correct" = yes ]; then
            done_run["$experiment,$stalled,$us,$run,$name"]=1
        fi
    done < <(tail -n +2 "$out")
fi

exec 9> "$lock"
if ! flock -n 9; then
    echo "waiting for $lock, held by another benchmark"
    flock 9
fi
echo "started $(date '+%F %T') on $(hostname), $("$cxx" --version | awk 'NR == 1')," \
     "commit $(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo none)"

# GCC's intermediate files go here rather than in /tmp
export TMPDIR="$build/tmp"
mkdir -p "$TMPDIR" "$build/stall-run" "$results"
flags=(-std=c++23 -O2 -g -fno-omit-frame-pointer -falign-loops=32 -falign-jumps=32
       -include "$here/text-shift.h" -DTEXT_SHIFT=0 -DSTALLS)
parlay=(-mcx16 -pthread -DNDEBUG)
build_program() {
    local binary="$build/stall-$1.out"
    case $1 in
        wd) "$cxx" "${flags[@]}" -pthread "$root/WF_DO_BFS/wf-dir.cpp" -o "$binary" ;;
        mem) "$cxx" "${flags[@]}" -pthread "$root/WF_DO_BFS/wf-dir-mem.cpp" -o "$binary" ;;
        gapbs) "$cxx" "${flags[@]}" -fopenmp "$root/DO_BFS/dir-bfs.cpp" -o "$binary" ;;
        pasgal) "$cxx" "${flags[@]}" "${parlay[@]}" \
                -I "$root/PASGAL/external/parlaylib/include" "$root/PASGAL_BFS/pasgal-bfs.cpp" \
                -o "$binary" ;;
        gbbs) "$cxx" "${flags[@]}" "${parlay[@]}" -DPARLAY_USE_STD_ALLOC \
                -I "$root/gbbs/external/parlaylib/include" "$root/GBBS_BFS/gbbs-bfs.cpp" \
                -o "$binary" ;;
    esac
}
pids=()
for name in "${programs[@]}"; do
    build_program "$name" 2> "$TMPDIR/stall-build-$name.log" &
    pids+=($!)
done
for i in "${!pids[@]}"; do
    if ! wait "${pids[i]}"; then
        cat -- "$TMPDIR/stall-build-${programs[i]}.log" >&2
        exit 1
    fi
done
echo "built ${#programs[@]} programs"

if [ ! -f "$out" ]; then
    echo "experiment,stalled,stall_us,run,program,us,last_us,correct,finished" > "$out"
fi
cd "$build/stall-run"
cleanup() {
    rm -f -- wfd-out.txt dir-out.txt gbbs-out.txt pasgal-out.txt stdout.txt stderr.txt
}
trap cleanup EXIT

for run in $(seq 1 "$runs"); do
    while read -r experiment stalled us; do
        # a different program starts each run's settings
        for i in "${!programs[@]}"; do
            name=${programs[(i + run) % ${#programs[@]}]}
            key="$experiment,$stalled,$us,$run,$name"
            if [ -n "${done_run[$key]:-}" ]; then
                continue
            fi
            case $name in
                gapbs) output=dir-out.txt ;;
                gbbs) output=gbbs-out.txt ;;
                pasgal) output=pasgal-out.txt ;;
                *) output=wfd-out.txt ;;
            esac
            rm -f -- "$output"
            status=0
            numactl --cpunodebind=0 --membind=0 env BFS_STALLED="$stalled" BFS_STALL_US="$us" \
                "$build/stall-$name.out" "$graph" "$threads" < /dev/null > stdout.txt 2> stderr.txt ||
                status=$?
            # for one thread count a program prints just its time
            time=$(awk 'NF == 1 && $1 ~ /^[0-9]+$/ { t = $1 } END { print t }' stdout.txt)
            last=$(awk '$1 == "last" && $2 == "return" && $3 ~ /^[0-9]+$/ { t = $3 } END { print t }' \
                stderr.txt)
            correct=no
            if [ "$status" = 0 ] && [ -n "$time" ] && cmp -s "$output" "$ref"; then
                correct=yes
            fi
            echo "$key,${time:-failed},$last,$correct,$(date +%FT%T)" >> "$out"
            echo "$(date +%T) $key: ${time:-failed} us${last:+, last return $last us}, correct $correct"
            if [ "$correct" != yes ]; then
                echo "    exit status $status"
                tail -n 5 stderr.txt | sed 's/^/    /'
            fi
        done
    done <<< "$settings"
done

# per setting and program, the median, fastest and slowest of the correct runs
# (a rerun's latest), the median last return for ours, and how many runs
{
    echo "experiment,stalled,stall_us,program,median_us,min_us,max_us,median_last_us,runs"
    awk -F, 'NR > 1 && $8 == "yes" { key = $1 "," $2 "," $3 "," $5
                                     t[key SUBSEP $4] = $6; l[key SUBSEP $4] = $7 }
             function median(v, k) { return (k % 2) ? v[(k + 1) / 2] : (v[k / 2] + v[k / 2 + 1]) / 2 }
             END { for (ks in t) { split(ks, p, SUBSEP)
                                   times[p[1]] = times[p[1]] " " t[ks]
                                   if (l[ks] != "") lasts[p[1]] = lasts[p[1]] " " l[ks] }
                   for (key in times) {
                       k = split(times[key], v, " "); asort(v)
                       m = split(lasts[key], w, " "); if (m) asort(w)
                       printf "%s,%d,%d,%d,%s,%d\n", key, median(v, k), v[1], v[k],
                              m ? sprintf("%d", median(w, m)) : "", k
                   } }' "$out" | sort -t, -k1,1 -k4,4 -k2,2n -k3,3n
} > "$summary"
cat -- "$summary"
echo "finished $(date '+%F %T')"
