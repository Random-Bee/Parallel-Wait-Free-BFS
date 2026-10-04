#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$script_dir"

graph_file=${1:-../datasets/liveJournal1.edges}
num_threads=${2:-4}
numa_node=${3:-0}
source_file=${4:-wf-dir-mo.cpp}

if [[ ! -f "$graph_file" ]]; then
    printf 'Graph file not found: %s\n' "$graph_file" >&2
    exit 2
fi

if [[ ! -f "$source_file" ]]; then
    printf 'Source file not found: %s\n' "$source_file" >&2
    exit 2
fi

if [[ ! "$num_threads" =~ ^[1-9][0-9]*$ ]]; then
    printf 'Thread count must be a positive integer, got: %s\n' \
        "$num_threads" >&2
    exit 2
fi

if [[ ! "$numa_node" =~ ^[0-9]+$ ||
        ! -d "/sys/devices/system/node/node$numa_node" ]]; then
    printf 'Invalid NUMA node: %s\n' "$numa_node" >&2
    exit 2
fi

if ! command -v perf >/dev/null 2>&1; then
    printf 'perf is not available on PATH\n' >&2
    exit 127
fi

if ! command -v numactl >/dev/null 2>&1; then
    printf 'numactl is not available on PATH\n' >&2
    exit 127
fi

if [[ -z "${NEWPERF:-}" || ! -x "${NEWPERF:-}" ]]; then
    printf 'NEWPERF must point to an executable perf binary\n' >&2
    exit 127
fi

# a call in tail position would become a jump, and its callee would then show
# up in the call graphs below under its caller's caller; otherwise built as
# the build scripts build (see DO_BFS/script.sh), so profiles match their times
cxx=/opt/rh/gcc-toolset-15/root/usr/bin/g++
"$cxx" -std=c++23 -O2 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -falign-loops=32 -falign-jumps=32 -pthread \
    "$source_file" -o wfdmo.out

workload=(
    numactl
    "--cpunodebind=$numa_node"
    "--membind=$numa_node"
    ./wfdmo.out
    "$graph_file"
    "$num_threads"
)

mkdir -p "$script_dir/tmp"
control_dir=$(mktemp -d "$script_dir/tmp/perf-control.XXXXXX")
control_fifo="$control_dir/control"
ack_fifo="$control_dir/ack"
mkfifo "$control_fifo" "$ack_fifo"

cleanup() {
    rm -f -- "$control_fifo" "$ack_fifo"
    rmdir -- "$control_dir" 2>/dev/null || true
}
trap cleanup EXIT

printf 'Profiling %s with %s threads on %s, NUMA node %s\n' \
    "$source_file" "$num_threads" "$graph_file" "$numa_node"

echo "Control FIFO: $control_fifo"
echo "Ack FIFO: $ack_fifo"

PERF_CTL_FIFO="$control_fifo" \
PERF_ACK_FIFO="$ack_fifo" \
perf stat -D -1 \
    --control "fifo:$control_fifo,$ack_fifo" \
    -e task-clock,cycles,instructions,branches,branch-misses,cache-references,cache-misses \
    -- "${workload[@]}"

PERF_CTL_FIFO="$control_fifo" \
PERF_ACK_FIFO="$ack_fifo" \
"$NEWPERF" stat -D -1 \
    --control "fifo:$control_fifo,$ack_fifo" \
    -e '{slots,topdown-retiring,topdown-bad-spec,topdown-br-mispredict,topdown-fe-bound,topdown-be-bound,topdown-mem-bound}' \
    -- "${workload[@]}"

# the per-level metrics are measured in cycles, so they overlap and do not
# add up to tma_memory_bound, which is measured in slots
PERF_CTL_FIFO="$control_fifo" \
PERF_ACK_FIFO="$ack_fifo" \
"$NEWPERF" stat -D -1 \
    --control "fifo:$control_fifo,$ack_fifo" \
    --metric-no-group \
    -M tma_memory_bound,tma_l1_bound,tma_l2_bound,tma_l3_bound,tma_dram_bound,tma_store_bound \
    -- "${workload[@]}"

PERF_CTL_FIFO="$control_fifo" \
PERF_ACK_FIFO="$ack_fifo" \
"$NEWPERF" stat -D -1 \
    --control "fifo:$control_fifo,$ack_fifo" \
    --metric-no-group \
    -M tma_core_bound,tma_ports_utilization,tma_divider,tma_serializing_operation,tma_ports_utilized_0,tma_ports_utilized_1,tma_ports_utilized_2,tma_ports_utilized_3m \
    -- "${workload[@]}"

PERF_CTL_FIFO="$control_fifo" \
PERF_ACK_FIFO="$ack_fifo" \
"$NEWPERF" stat -D -1 \
    --control "fifo:$control_fifo,$ack_fifo" \
    --metric-no-group \
    -M tma_ports_utilized_3m_group \
    -- "${workload[@]}"

PERF_CTL_FIFO="$control_fifo" \
PERF_ACK_FIFO="$ack_fifo" \
"$NEWPERF" stat -D -1 \
    --control "fifo:$control_fifo,$ack_fifo" \
    --metric-no-group \
    -M tma_alu_op_utilization_group \
    -- "${workload[@]}"

PERF_CTL_FIFO="$control_fifo" \
PERF_ACK_FIFO="$ack_fifo" \
"$NEWPERF" stat -D -1 \
    --control "fifo:$control_fifo,$ack_fifo" \
    --metric-no-group \
    -e '{L1-dcache-loads:u,L1-dcache-load-misses:u}' \
    -e '{l2_rqsts.demand_data_rd_miss:u,l2_rqsts.miss:u}' \
    -- "${workload[@]}"

PERF_CTL_FIFO="$control_fifo" \
PERF_ACK_FIFO="$ack_fifo" \
"$NEWPERF" stat -D -1 \
    --control "fifo:$control_fifo,$ack_fifo" \
    -e '{l2_rqsts.swpf_hit:u,l2_rqsts.swpf_miss:u,l2_rqsts.all_hwpf:u,l2_rqsts.hwpf_miss:u}' \
    -- "${workload[@]}"


# recorded with the system perf: NEWPERF is built without the interactive
# report, and the system perf cannot read files that NEWPERF writes
record_profile() {
    local output_file=$1
    local events=$2

    # LBR stacks, which the CPU records from the calls and returns themselves.
    # Frame-pointer stacks put a leaf function's samples under its caller's
    # caller, since GCC 8 gives no frame to a leaf that doesn't use the stack,
    # whatever the flags; dwarf copies 8 KB of stack per sample, which
    # overflows perf's buffer and drops samples. The rate stays under one
    # sample per 1 ms tick, past which the kernel throttles (see stall_period)
    PERF_CTL_FIFO="$control_fifo" \
    PERF_ACK_FIFO="$ack_fifo" \
    perf record -D -1 \
        --control "fifo:$control_fifo,$ack_fifo" \
        -F 499 \
        --call-graph lbr \
        -e "$events" \
        -o "$output_file" \
        -- "${workload[@]}"
}

rm -f perf-cycles.data perf-branches.data perf-cache.data perf-loads.data \
    perf-stalls.data perf-loadsrc.data

record_profile perf-cycles.data \
    '{cycles:upp,instructions:upp}'
record_profile perf-branches.data \
    '{branches:upp,branch-misses:upp}'
record_profile perf-cache.data \
    '{L1-dcache-loads:upp,L1-dcache-load-misses:upp}'

record_counts() {
    local output_file=$1
    local period=$2
    shift 2

    PERF_CTL_FIFO="$control_fifo" \
    PERF_ACK_FIFO="$ack_fifo" \
    perf record -D -1 \
        --control "fifo:$control_fifo,$ack_fifo" \
        -c "$period" \
        "$@" \
        -o "$output_file" \
        -- "${workload[@]}"
}

# the system perf has no event list for this CPU, so these are raw encodings;
# `$NEWPERF list --details <event>` prints the encoding of an event name
#
# the kernel throttles an event, dropping its samples until the next 1 ms
# tick, once it takes kernel.perf_event_max_sample_rate / 1000 samples within
# a tick. It lowers that rate by itself when sampling interrupts run long, and
# at 2000, as here now, only one sample per tick is safe: the period must
# cover more than 1 ms of cycles, about 3.9M at full clock. The PEBS load
# events interrupt once per buffer of samples, not per sample, so their short
# period is fine
stall_period=4000037
record_counts perf-stalls.data "$stall_period" \
    -e cycles:u \
    -e 'cpu/event=0xa3,umask=0x4,cmask=0x4,name=cycle_activity.stalls_total/u' \
    -e 'cpu/event=0xa3,umask=0xc,cmask=0xc,name=cycle_activity.stalls_l1d_miss/u' \
    -e 'cpu/event=0xa3,umask=0x5,cmask=0x5,name=cycle_activity.stalls_l2_miss/u' \
    -e 'cpu/event=0xa3,umask=0x6,cmask=0x6,name=cycle_activity.stalls_l3_miss/u'

record_counts perf-loadsrc.data 10007 \
    -e 'cpu/event=0xd1,umask=0x2,name=mem_load_retired.l2_hit/ppu' \
    -e 'cpu/event=0xd1,umask=0x40,name=mem_load_retired.fb_hit/ppu' \
    -e 'cpu/event=0xd1,umask=0x4,name=mem_load_retired.l3_hit/ppu' \
    -e 'cpu/event=0xd1,umask=0x20,name=mem_load_retired.l3_miss/ppu'

printf 'Created perf-cycles.data, perf-branches.data, perf-cache.data, '
printf 'perf-stalls.data, and perf-loadsrc.data\n'

# throttling drops samples, so every count taken from that file comes out low
for data_file in perf-stalls.data perf-loadsrc.data; do
    throttled=$("$NEWPERF" report -i "$data_file" --stats 2>/dev/null \
        | awk '/ THROTTLE events:/ { print $3 }' || true)
    if [[ -n "$throttled" && "$throttled" != 0 ]]; then
        printf 'Warning: the kernel throttled %s %s times; raise its period\n' \
            "$data_file" "$throttled"
    fi
done

printf '\nStalled cycles per function, billions over all threads and repetitions.\n'
printf 'Stalls are split by the farthest level a pending load has to reach.\n'
"$NEWPERF" script -i perf-stalls.data -F event,ip,sym 2>/dev/null \
    | awk -v period="$stall_period" '
    # counters are sampled independently, so small differences can dip below 0
    function nonneg(x) { return x < 0 ? 0 : x }
    {
        event = $1; sub(/:.*$/, "", event)
        $1 = ""; $2 = ""
        sym = $0; sub(/^ +/, "", sym); sub(/\(.*$/, "", sym)
        count[sym, event]++
        seen[sym] = 1
    }
    END {
        scale = period / 1e9
        printf "%-30s %8s %8s %8s %8s %8s %8s\n", "function", "cycles", "stalled", "no-miss", "L2-hit", "L3-hit", "DRAM"
        n = 0
        for (s in seen) names[++n] = s
        for (i = 1; i <= n; i++)
            for (j = i + 1; j <= n; j++)
                if (count[names[j], "cycles"] > count[names[i], "cycles"]) { t = names[i]; names[i] = names[j]; names[j] = t }
        for (i = 1; i <= n && i <= 12; i++) {
            s = names[i]
            total = count[s, "cycle_activity.stalls_total"]
            l1 = count[s, "cycle_activity.stalls_l1d_miss"]
            l2 = count[s, "cycle_activity.stalls_l2_miss"]
            l3 = count[s, "cycle_activity.stalls_l3_miss"]
            printf "%-30s %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f\n", substr(s, 1, 30), count[s, "cycles"] * scale, total * scale, nonneg(total - l1) * scale, nonneg(l1 - l2) * scale, nonneg(l2 - l3) * scale, l3 * scale
        }
    }'

