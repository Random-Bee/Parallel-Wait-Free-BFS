#!/usr/bin/env bash
set -euo pipefail

# The full benchmark: wf-dir.cpp and wf-dir-mem.cpp (ours), our top-down-only
# WF_BFS/wf-bfs-csr.cpp, GAPBS's direction-optimizing BFS (DO_BFS/dir-bfs.cpp),
# PASGAL and GBBS, and at one thread DO_BFS/dir-bfs-seq.cpp as the sequential
# baseline. Two placements, run in this order:
#   node0  threads and memory on NUMA node 0, 1 to 64 threads
#   both   threads spread evenly over both nodes (BFS_SPREAD_NODES), each
#          still pinned to a CPU, and memory interleaved over both, 1 to 96
#          threads
# A run loads its graph once and goes through every thread count of its
# placement in turn, resting a few seconds between them. Each program is
# built as the build scripts build it (GCC 15, C++23, loops and jump targets
# on 32-byte boundaries) five times over, its code moved by 0 to 256 bytes in
# steps of 64 (text-shift.h): where the code lands moves a time by up to 5%,
# so each build runs once per graph and placement, and a point is the median
# of its five builds. A round runs every program once, so drift over the
# benchmark reaches them all alike. A point is correct when its distances
# match those of its run's first thread count, and the run's last distances
# match a sequential BFS's.
#
# Usage: ./benchmark.sh [placements] [graphs], e.g. ./benchmark.sh node0 "lj uk";
# both placements and all five graphs by default; PROGRAMS="wfbfs" limits it to
# some programs. DRY_RUN=1 lists the runs left without building or running
# anything. Rows go to results/benchmark.csv
# and per-point medians to results/benchmark-summary.csv. A stopped benchmark
# picks up where it left off, as a run with every point correct is not run
# again; delete results/benchmark.csv to start over, which is needed after
# any program changes. It runs for hours, so start it detached:
#   setsid nohup ./benchmark.sh > results/benchmark.log 2>&1 &

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(dirname "$here")
placements=${1:-node0 both}
graphs=${2:-lj twitter road uk kron}
dry_run=${DRY_RUN:-0}

cxx=/opt/rh/gcc-toolset-15/root/usr/bin/g++
datasets=/codemill/kumaraay/datasets
refs="$root/WF_BFS/tmp"
lock="$root/WF_BFS/tmp/bench.lock"
build="$here/build"
results="$here/results"
out="$results/benchmark.csv"
summary="$results/benchmark-summary.csv"
shifts=(0 64 128 192 256)
read -r -a programs <<< "${PROGRAMS:-wd mem wfbfs gapbs pasgal gbbs seq}"

# the thread counts a run goes through: the placement's, or 1 for the
# sequential baseline
thread_counts() {
    if [ "$2" = seq ]; then
        echo 1
        return
    fi
    case $1 in
        node0) echo "1 2 4 8 16 24 32 40 48 56 64" ;;
        both) echo "1 2 4 8 16 24 32 40 48 56 64 80 96" ;;
        *) return 1 ;;
    esac
}

# sets launcher, the command each run starts under
set_launcher() {
    case $1 in
        node0) launcher=(numactl --cpunodebind=0 --membind=0) ;;
        both) launcher=(env BFS_SPREAD_NODES=1 numactl --interleave=all) ;;
    esac
}

# sets path, the graph's file, and ref, the distances a sequential BFS found
set_graph() {
    case $1 in
        lj) path="$root/datasets/liveJournal1.edges"; ref="$refs/seq-liveJournal1.txt" ;;
        twitter) path="$datasets/twitter-2010.txt"; ref="$refs/twitter-ref.txt" ;;
        road) path="$datasets/road-road-usa.mtx"; ref="$refs/road-reference.txt" ;;
        uk) path="$datasets/uk-2002.mtx"; ref="$refs/uk-2002-ref.txt" ;;
        kron) path="$datasets/kron-25.edges"; ref="$refs/kron-25-ref.txt" ;;
        *) return 1 ;;
    esac
}

for placement in $placements; do
    thread_counts "$placement" wd > /dev/null || { echo "unknown placement $placement" >&2; exit 1; }
done
for name in "${programs[@]}"; do
    case $name in
        wd|mem|wfbfs|gapbs|pasgal|gbbs|seq) ;;
        *) echo "unknown program $name" >&2; exit 1 ;;
    esac
done
for graph in $graphs; do
    set_graph "$graph" || { echo "unknown graph $graph" >&2; exit 1; }
    for file in "$path" "$ref"; do
        [ -f "$file" ] || { echo "missing $file" >&2; exit 1; }
    done
done

# how many distinct points each run (placement, graph, build, program) has
# measured correctly so far
declare -A correct_point correct_points
if [ -f "$out" ]; then
    while IFS=, read -r placement graph threads shift name us correct finished; do
        run="$placement,$graph,$shift,$name"
        if [ "$correct" = yes ] && [ -z "${correct_point[$run,$threads]:-}" ]; then
            correct_point["$run,$threads"]=1
            correct_points["$run"]=$(( ${correct_points[$run]:-0} + 1 ))
        fi
    done < <(tail -n +2 "$out")
fi

pending=()
for placement in $placements; do
    for graph in $graphs; do
        for shift in "${shifts[@]}"; do
            for name in "${programs[@]}"; do
                run="$placement,$graph,$shift,$name"
                read -r -a list <<< "$(thread_counts "$placement" "$name")"
                if [ "${correct_points[$run]:-0}" -lt "${#list[@]}" ]; then
                    pending+=("$run")
                fi
            done
        done
    done
done
echo "${#pending[@]} runs to go: placements $placements; graphs $graphs"
if [ "$dry_run" = 1 ]; then
    printf '%s\n' "${pending[@]}"
    exit 0
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
mkdir -p "$TMPDIR" "$build/run"
flags=(-std=c++23 -O2 -g -fno-omit-frame-pointer -falign-loops=32 -falign-jumps=32
       -include "$here/text-shift.h")
parlay=(-mcx16 -pthread -DNDEBUG)
build_program() {
    local name=$1 shift=$2
    local binary="$build/$name-$shift.out"
    case $name in
        wfbfs) "$cxx" "${flags[@]}" -DTEXT_SHIFT=$shift -pthread "$root/WF_BFS/wf-bfs-csr.cpp" \
                -o "$binary" ;;
        wd) "$cxx" "${flags[@]}" -DTEXT_SHIFT=$shift -pthread "$root/WF_DO_BFS/wf-dir.cpp" \
                -o "$binary" ;;
        mem) "$cxx" "${flags[@]}" -DTEXT_SHIFT=$shift -pthread "$root/WF_DO_BFS/wf-dir-mem.cpp" \
                -o "$binary" ;;
        gapbs) "$cxx" "${flags[@]}" -DTEXT_SHIFT=$shift -fopenmp "$root/DO_BFS/dir-bfs.cpp" \
                -o "$binary" ;;
        pasgal) "$cxx" "${flags[@]}" -DTEXT_SHIFT=$shift "${parlay[@]}" \
                -I "$root/PASGAL/external/parlaylib/include" "$root/PASGAL_BFS/pasgal-bfs.cpp" \
                -o "$binary" ;;
        gbbs) "$cxx" "${flags[@]}" -DTEXT_SHIFT=$shift "${parlay[@]}" -DPARLAY_USE_STD_ALLOC \
                -I "$root/gbbs/external/parlaylib/include" "$root/GBBS_BFS/gbbs-bfs.cpp" \
                -o "$binary" ;;
        seq) "$cxx" "${flags[@]}" -DTEXT_SHIFT=$shift -pthread "$root/DO_BFS/dir-bfs-seq.cpp" \
                -o "$binary" ;;
    esac
}
pids=()
logs=()
for name in "${programs[@]}"; do
    for shift in "${shifts[@]}"; do
        log="$TMPDIR/build-$name-$shift.log"
        build_program "$name" "$shift" 2> "$log" &
        pids+=($!)
        logs+=("$log")
    done
done
for i in "${!pids[@]}"; do
    if ! wait "${pids[i]}"; then
        cat -- "${logs[i]}" >&2
        exit 1
    fi
done
rm -f -- "${logs[@]}"
echo "built ${#pids[@]} programs"

mkdir -p "$results"
if [ ! -f "$out" ]; then
    echo "placement,graph,threads,shift,program,us,correct,finished" > "$out"
fi
cd "$build/run"
cleanup() {
    rm -f -- wfd-out.txt wf-csr-out.txt dir-out.txt pasgal-out.txt gbbs-out.txt dir-seq-out.txt \
        stdout.txt stderr.txt
}
trap cleanup EXIT

for run in "${pending[@]}"; do
    IFS=, read -r placement graph shift name <<< "$run"
    set_launcher "$placement"
    set_graph "$graph"
    counts=$(thread_counts "$placement" "$name")
    case $name in
        gapbs) output=dir-out.txt ;;
        pasgal) output=pasgal-out.txt ;;
        gbbs) output=gbbs-out.txt ;;
        seq) output=dir-seq-out.txt ;;
        wfbfs) output=wf-csr-out.txt ;;
        *) output=wfd-out.txt ;;
    esac
    args=("$path")
    if [ "$name" != seq ]; then
        args+=("${counts// /,}")
    fi
    rm -f -- "$output"
    status=0
    "${launcher[@]}" "$build/$name-$shift.out" "${args[@]}" > stdout.txt 2> stderr.txt ||
        status=$?
    ok=$(cmp -s "$output" "$ref" && echo yes || echo no)

    # a line per thread count, "threads us same|differs", or for one count
    # just its time; a count with no line failed
    rows=$(awk -v counts="$counts" -v ok="$ok" -v run="$run" -v when="$(date +%FT%T)" '
        NF == 1 && $1 ~ /^[0-9]+$/ { us[1] = $1; check[1] = "same" }
        NF == 3 && $1 ~ /^[0-9]+$/ && $2 ~ /^[0-9]+$/ { us[$1] = $2; check[$1] = $3 }
        END { split(run, r, ",")
              n = split(counts, c, " ")
              for (i = 1; i <= n; i++) {
                  t = c[i]
                  time = (t in us) ? us[t] : "failed"
                  correct = (time != "failed" && check[t] == "same" && ok == "yes") ? "yes" : "no"
                  printf "%s,%s,%s,%s,%s,%s,%s,%s\n", r[1], r[2], t, r[3], r[4], time,
                         correct, when
              } }' stdout.txt)
    echo "$rows" >> "$out"
    read -r -a list <<< "$counts"
    good=$(echo "$rows" | awk -F, '$7 == "yes" { n++ } END { print n + 0 }')
    echo "$(date +%T) $run: $(echo "$rows" | awk -F, '{ printf "%s:%s ", $3, $6 }')" \
         "($good of ${#list[@]} correct)"
    if [ "$status" != 0 ] || [ "$good" -ne "${#list[@]}" ]; then
        echo "    exit status $status, output matches the reference: $ok"
        tail -n 5 stderr.txt | sed 's/^/    /'
    fi
done

# per point, the median of the builds that ran correctly (a rerun build's
# latest time), the fastest and slowest of them, how many there are (five
# when complete), and how many rows, reruns included, failed or gave wrong
# distances
{
    echo "placement,graph,threads,program,median_us,min_us,max_us,builds,failed"
    awk -F, 'NR > 1 { key = $1 "," $2 "," $3 "," $5; seen[key] = 1
                      if ($7 == "yes" && $6 ~ /^[0-9]+$/) last[key SUBSEP $4] = $6
                      else failed[key]++ }
             END { for (ks in last) { split(ks, p, SUBSEP); times[p[1]] = times[p[1]] " " last[ks] }
                   for (key in seen) {
                       k = split(times[key], v, " ")
                       if (k) { asort(v); median = (k % 2) ? v[(k + 1) / 2] : (v[k / 2] + v[k / 2 + 1]) / 2 }
                       printf "%s,%s,%s,%s,%d,%d\n", key, k ? median : "", k ? v[1] : "",
                              k ? v[k] : "", k, failed[key]
                   } }' "$out" | sort -t, -k1,1r -k2,2 -k4,4 -k3,3n
} > "$summary"
cat -- "$summary"
echo "finished $(date '+%F %T')"
