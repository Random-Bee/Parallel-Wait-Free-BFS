#include <bits/stdc++.h>
#include "../CPU_helpers/cpu_affinity.hpp"
#include "../Graph_helpers/graph_reader.hpp"
using namespace std;
using namespace chrono;

int N;
size_t M;
// vertex u's neighbors are adj[offsets[u]] up to, not including, adj[offsets[u+1]]
graph_helpers::array<size_t> offsets;
graph_helpers::array<int> adj;
// vis marks vertices expanded top-down
vector<int> dist, vis;
// a one-byte code per vertex for the bottom-up frontier test, a quarter the size
// of dist. Each level that a bottom-up level reads or writes gets its own code
// from the level plan, handed out in increasing order and never reused within a
// search, so a code names one level at any depth and a stale thread's test stays
// exact. 0 is no code; once max_code is used up, levels test dist. Bottom-up
// discoveries write their code before dist, so a thread that sees a vertex
// marked also sees its code; top-down pushes write none, since many threads
// pushing nearby vertices fight over the lines, and the first bottom-up level
// after them writes its frontier's codes before testing
constexpr int max_code = 255;
vector<uint8_t> dist8;

int num_t;

// keeps the compiler from moving memory accesses across this point; x86 itself
// keeps stores in order and loads in order (SDM Vol. 3A, 9.2.2, December 2022)
[[gnu::always_inline]] inline void compiler_barrier() {
    asm volatile("" ::: "memory");
}

// the memory one thread hands out during a search: its buckets' arrays and the
// levels it adds. A block is handed out by moving a pointer, so a thread never
// waits on a lock in the allocator while it searches, and once the search is
// over all of it is taken back at once rather than block by block. The chunks
// stay for later searches. Blocks start and end on 128-byte boundaries, so two
// that different threads write never share a line, nor the pair of lines that
// the L2 prefetcher fetches together. Arenas are padded the same way: before
// C++17 a vector of them is only 16-byte aligned, but two lines of padding
// still keep each arena's fields out of the next one's lines
class alignas(128) arena {
    public:
    static constexpr size_t block_alignment = 128;

    arena() = default;
    arena(const arena&) = delete;
    arena& operator=(const arena&) = delete;
    ~arena() {
        for(chunk& c: chunks) ::operator delete(c.begin);
    }

    void* allocate(size_t bytes) {
        bytes = (bytes + block_alignment - 1) & ~(block_alignment - 1);
        uintptr_t start = align_up(next);
        if(next == nullptr || start + bytes > reinterpret_cast<uintptr_t>(end)) {
            start = take_chunk(bytes);
        }
        next = reinterpret_cast<char*>(start + bytes);
        return reinterpret_cast<void*>(start);
    }

    // only once no thread can still read a block it handed out
    void reset() {
        current = 0;
        next = chunks.empty() ? nullptr : chunks[0].begin;
        end = chunks.empty() ? nullptr : chunks[0].begin + chunks[0].bytes;
    }

    private:
    struct chunk {
        char* begin;
        size_t bytes;
    };
    static constexpr size_t chunk_bytes = size_t(4) << 20;

    char* next = nullptr;
    char* end = nullptr;
    size_t current = 0;
    vector<chunk> chunks;

    static uintptr_t align_up(const char* p) {
        uintptr_t address = reinterpret_cast<uintptr_t>(p);
        return (address + block_alignment - 1) & ~(block_alignment - 1);
    }

    // the first chunk after the one in use that the block fits in, or a new one
    uintptr_t take_chunk(size_t bytes) {
        if(next != nullptr) current++;
        while(current < chunks.size() && chunks[current].bytes < bytes + block_alignment) {
            current++;
        }
        if(current == chunks.size()) {
            size_t size = max(chunk_bytes, bytes + block_alignment);
            chunks.push_back({static_cast<char*>(::operator new(size)), size});
        }
        next = chunks[current].begin;
        end = next + chunks[current].bytes;
        return align_up(next);
    }
};

// each thread rewrites its size and degree sum on every push, so no two buckets
// may share a cache line, nor the pair of lines that the L2 prefetcher fetches
// together
class alignas(128) customList {
    public:
    int* list = nullptr;
    int size = 0;
    int capacity = 0;
    // summed degrees of the vertices pushed: the direction rule compares the
    // top-down sums with the edges still unexplored, and a bottom-up level
    // sizes its buckets by them
    long long degree_sum = 0;
    // the arena of the bucket's owner, the one thread that pushes to it
    arena* memory;

    explicit customList(arena* memory) : memory(memory) {}

    // readers load size before list, so publish the array contents, then list,
    // then size; otherwise a reader can index past what it is able to see
    [[gnu::always_inline]] inline void push_back(int x) {
        if(size == capacity) grow();
        list[size] = x;
        compiler_barrier();
        size++;
    }

    // an array smaller than an arena block would leave the rest of it unused
    [[gnu::noinline]] void grow() {
        reserve(max(capacity * 2, static_cast<int>(arena::block_alignment / sizeof(int))));
    }

    // a reader may still hold the old array, which stays in the arena until the
    // search is over
    void reserve(int wanted) {
        if(wanted <= capacity) return;
        int* new_list = static_cast<int*>(
            memory->allocate(sizeof(int) * static_cast<size_t>(wanted)));
        if(size > 0) std::copy(list, list+size, new_list);
        compiler_barrier();
        list = new_list;
        capacity = wanted;
    }
};

// padded to a cache line, since each owner rewrites its entry as it goes
struct alignas(64) owner_progress {
    int last_done;
};

enum level_direction { top_down, bottom_up };

// what the direction rule carries from one level to the next
struct level_plan {
    level_direction direction;
    long long edges_to_check;
    long long frontier_size;
    // vertices in no frontier yet; a vertex pushed twice is counted twice, so
    // this can run below the true count
    long long undiscovered;
    // the code of this level's frontier in dist8, or 0, and the highest code
    // handed out so far
    int code;
    int last_code;
};

// a level's per-thread entries are arrays of num_t, built by new_level
class outer_list_node {
    public:
    int depth;
    customList* buckets;
    // one byte per flag: flags packed into shared words would make threads
    // setting different flags race on the same word
    uint8_t* done;
    // how far each bucket's frontier codes are written, by its owner, and
    // whether some thread has written all of them
    owner_progress* codes_progress;
    uint8_t* codes_done;
    // how far each owner has got: the last entry of its bucket it finished
    // top-down, the last of its blocks bottom-up
    owner_progress* progress;
    // one proposal per thread; the first thread to claim plan_owner decides
    level_plan* proposals;
    atomic<int> plan_owner;
    atomic<outer_list_node*> next;
    outer_list_node() {
        plan_owner = -1;
        next = nullptr;
    }
};

vector<arena> arenas;

// a level, in the arena of the thread that adds it; bucket t's arrays come from
// thread t's arena, since only thread t pushes to it. Proposals are written
// before they are read, so they start unset
outer_list_node* new_level(int depth, arena& memory) {
    outer_list_node* node = new(memory.allocate(sizeof(outer_list_node))) outer_list_node;
    node->depth = depth;
    node->buckets = static_cast<customList*>(memory.allocate(sizeof(customList) * num_t));
    node->done = static_cast<uint8_t*>(memory.allocate(num_t));
    node->codes_progress = static_cast<owner_progress*>(
        memory.allocate(sizeof(owner_progress) * num_t));
    node->codes_done = static_cast<uint8_t*>(memory.allocate(num_t));
    node->progress = static_cast<owner_progress*>(
        memory.allocate(sizeof(owner_progress) * num_t));
    node->proposals = static_cast<level_plan*>(memory.allocate(sizeof(level_plan) * num_t));
    for(int t=0; t<num_t; t++) {
        new(&node->buckets[t]) customList(&arenas[t]);
        node->done[t] = 0;
        node->codes_progress[t].last_done = -1;
        node->codes_done[t] = 0;
        node->progress[t].last_done = -1;
    }
    return node;
}

struct affinePermutation {
    size_t index;
    size_t step;
};

size_t greatest_common_divisor(
        size_t first, size_t second) {
    while(second != 0) {
        size_t remainder = first % second;
        first = second;
        second = remainder;
    }
    return first;
}

affinePermutation affine_shuffle(
        size_t size, mt19937& rng) {
    if(size <= 1) return {0, 0};

    size_t index = static_cast<size_t>(rng()) % size;
    size_t step = 1 + static_cast<size_t>(rng()) % (size - 1);

    while(greatest_common_divisor(step, size) != 1) {
        step++;
        if(step == size) step = 1;
    }

    return {index, step};
}

[[gnu::always_inline]] inline size_t next_affine_index(
        size_t index, size_t step, size_t size) {
    index += step;
    return index >= size ? index - size : index;
}

void control_perf(const char* command) {
    static FILE* control = nullptr;
    static FILE* ack = nullptr;
    static bool initialized = false;

    if(!initialized) {
        const char* control_path = getenv("PERF_CTL_FIFO");
        const char* ack_path = getenv("PERF_ACK_FIFO");

        if(control_path == nullptr && ack_path == nullptr) {
            initialized = true;
            return;
        }
        if(control_path == nullptr || ack_path == nullptr) {
            cerr << "Error: Both PERF_CTL_FIFO and PERF_ACK_FIFO "
                 << "must be set\n";
            exit(EXIT_FAILURE);
        }

        control = fopen(control_path, "w");
        if(control == nullptr) {
            cerr << "Error: Failed to open perf control FIFO\n";
            exit(EXIT_FAILURE);
        }

        ack = fopen(ack_path, "r");
        if(ack == nullptr) {
            cerr << "Error: Failed to open perf acknowledgement FIFO\n";
            exit(EXIT_FAILURE);
        }

        initialized = true;
    }

    if(control == nullptr) return;

    if(fprintf(control, "%s\n", command) < 0 || fflush(control) != 0) {
        cerr << "Error: Failed to send command to perf\n";
        exit(EXIT_FAILURE);
    }

    constexpr char expected[] = "ack\n";
    char response[sizeof(expected)];
    if(fread(response, 1, sizeof(response), ack) != sizeof(response) ||
            memcmp(response, expected, sizeof(expected)) != 0) {
        cerr << "Error: Invalid acknowledgement from perf\n";
        exit(EXIT_FAILURE);
    }
}

constexpr int vis_prefetch_distance = 32;
constexpr int header_prefetch_distance = 16;
constexpr int payload_prefetch_distance = 8;
constexpr uintptr_t cache_line_bytes = 64;
constexpr size_t ints_per_line = cache_line_bytes / sizeof(int);
constexpr size_t payload_prefetch_lines = 3;
constexpr size_t list_prefetch_lookahead =
    payload_prefetch_lines * ints_per_line;
// the far lead pays off only on lists longer than a 4 KiB page
constexpr size_t far_cursor_min_count = 4096 / sizeof(int);
constexpr size_t far_cursor_lines = 16;

// helpers walk what is left of a list in blocks of helper_block entries: the
// blocks in a random order, the entries inside a block in order, so
// neighboring entries share cache lines
constexpr int helper_block = 16;
// the look-ahead reaches at most two blocks past the current one
static_assert(vis_prefetch_distance <= 2*helper_block,
              "helper look-ahead must stay within two blocks");

struct blockWalk {
    size_t count;
    size_t step;
    // upcoming[k] is the block k steps after the current one in the walk
    size_t upcoming[3];
};

[[gnu::always_inline]] inline blockWalk start_block_walk(
        int entries, mt19937& rng) {
    size_t count = (entries + helper_block - 1) / helper_block;
    affinePermutation order = affine_shuffle(count, rng);
    blockWalk walk = {count, order.step, {order.index, 0, 0}};
    walk.upcoming[1] = next_affine_index(walk.upcoming[0], walk.step, count);
    walk.upcoming[2] = next_affine_index(walk.upcoming[1], walk.step, count);
    return walk;
}

[[gnu::always_inline]] inline void advance_block_walk(blockWalk& walk) {
    walk.upcoming[0] = walk.upcoming[1];
    walk.upcoming[1] = walk.upcoming[2];
    walk.upcoming[2] = next_affine_index(walk.upcoming[2], walk.step, walk.count);
}

// the entry `distance` places after entry `offset` of the current block;
// entries past the end of the list clamp to its last entry
[[gnu::always_inline]] inline int entry_ahead(
        const blockWalk& walk, int offset, int distance, int last) {
    unsigned position = offset + distance;
    int entry = static_cast<int>(walk.upcoming[position / helper_block])*helper_block
        + static_cast<int>(position % helper_block);
    return min(entry, last);
}

[[gnu::always_inline]] inline void prefetch_offsets(int u) {
    __builtin_prefetch(&offsets[u], 0, 2);
    // offsets[u+1] starts the next line whenever offsets[u] ends one
    __builtin_prefetch(&offsets[u+1], 0, 2);
}

[[gnu::always_inline]] inline void prefetch_vertex_header(int u) {
    prefetch_offsets(u);
    __builtin_prefetch(&vis[u], 0, 2);
    __builtin_prefetch(&dist[u], 0, 2);
}

[[gnu::always_inline]] inline void prefetch_vertex_payload(int u) {
    uintptr_t line = reinterpret_cast<uintptr_t>(adj.data() + offsets[u]) & ~(cache_line_bytes - 1);
    __builtin_prefetch(reinterpret_cast<const void*>(line), 0, 3);
    for(size_t k=1; k<payload_prefetch_lines; k++) {
        __builtin_prefetch(
            reinterpret_cast<const void*>(line + k*cache_line_bytes), 0, 2);
    }
}

// visited vertices map to vertex 0, whose lines stay cached, so helpers
// spend no memory traffic on entries they are going to skip; GCC turns
// the equivalent ?: into an unpredictable branch, the mask it cannot
[[gnu::always_inline]] inline int unvisited_or_root(int u) {
    return u & -static_cast<int>(vis[u] == 0);
}

// push before marking: if this thread stops in between, v stays unmarked and
// another thread can still discover and push it. The code goes before the mark
// too, so a thread that finds v marked also finds its code; past the last code
// it writes 0, which an undiscovered v already has
[[gnu::always_inline]] inline void discover_vertex(
        int v, customList& output_bucket, int next_distance, uint8_t next_code) {
    output_bucket.push_back(v);
    compiler_barrier();
    dist8[v] = next_code;
    compiler_barrier();
    dist[v] = next_distance;
}

[[gnu::always_inline]] inline long long degree(int v) {
    return offsets[v+1] - offsets[v];
}

// pushed before marking, as in discover_vertex. Of the threads that push v,
// only the one whose swap marks it counts its edges: the direction rule
// subtracts them from those still unexplored, and counting copies runs that
// total down long before the search ends
[[gnu::always_inline]] inline void discover_and_count(
        int v, customList& output_bucket, int next_distance) {
    output_bucket.push_back(v);
    if(__sync_bool_compare_and_swap(&dist[v], -1, next_distance)) {
        output_bucket.degree_sum += degree(v);
    }
}

void process_neighbors_generic(
        const int* neighbors, size_t count,
        customList& output_bucket, int next_distance) {
    for(size_t j=0; j<count; j++) {
        int v = neighbors[j];
        if(dist[v] == -1) {
            discover_and_count(v, output_bucket, next_distance);
        }
    }
}

void process_neighbors_long(
    const int* neighbors, size_t count,
    customList& output_bucket, int next_distance
) {
    size_t cursor_lines = count > far_cursor_min_count
        ? far_cursor_lines : payload_prefetch_lines;
    uintptr_t prefetch_cursor = reinterpret_cast<uintptr_t>(neighbors)
        + cursor_lines*cache_line_bytes;

    size_t j = 0;
    for(; j+ints_per_line<=count; j+=ints_per_line) {
        // prefetches never fault, so the cursor may run past the list
        __builtin_prefetch(reinterpret_cast<const void*>(prefetch_cursor), 0, 3);
        prefetch_cursor += cache_line_bytes;

        for(size_t k=j; k<j+ints_per_line; k++) {
            int v = neighbors[k];
            if(dist[v] == -1) {
                discover_and_count(v, output_bucket, next_distance);
            }
        }
    }
    process_neighbors_generic(
        neighbors+j, count-j, output_bucket, next_distance);
}

[[gnu::always_inline]] inline void process_neighbors(
        int u, customList& output_bucket, int next_distance) {
    const int* neighbors = adj.data() + offsets[u];
    size_t count = offsets[u+1] - offsets[u];
    if(count <= list_prefetch_lookahead) {
        process_neighbors_generic(
            neighbors, count, output_bucket, next_distance);
    }
    else {
        process_neighbors_long(
            neighbors, count, output_bucket, next_distance);
    }
}

// a helper walks a long list as it walks a bucket: in blocks of helper_block
// entries, the blocks in a random order of its own. An owner and helpers that
// expand the same hub then swap different entries of dist at any moment;
// walking the list in the same order, they would all swap the same ones, each
// swap waiting for the line
void process_neighbors_shuffled(
        const int* neighbors, size_t count,
        customList& output_bucket, int next_distance, mt19937& rng) {
    size_t blocks = (count + helper_block - 1) / helper_block;
    affinePermutation order = affine_shuffle(blocks, rng);
    size_t block = order.index;
    for(size_t b=0; b<blocks; b++) {
        size_t next_block = next_affine_index(block, order.step, blocks);
        // out of the hardware prefetcher's reach, and a block can straddle two
        // lines; prefetches never fault, so the last may run past the list
        const int* upcoming = neighbors + next_block*helper_block;
        __builtin_prefetch(upcoming, 0, 3);
        __builtin_prefetch(upcoming + helper_block - 1, 0, 3);

        size_t end = min((block + 1)*helper_block, count);
        for(size_t j=block*helper_block; j<end; j++) {
            int v = neighbors[j];
            if(dist[v] == -1) {
                discover_and_count(v, output_bucket, next_distance);
            }
        }
        block = next_block;
    }
}

[[gnu::always_inline]] inline void process_neighbors_helping(
        int u, customList& output_bucket, int next_distance, mt19937& rng) {
    const int* neighbors = adj.data() + offsets[u];
    size_t count = offsets[u+1] - offsets[u];
    if(count <= list_prefetch_lookahead) {
        process_neighbors_generic(
            neighbors, count, output_bucket, next_distance);
    }
    else {
        process_neighbors_shuffled(
            neighbors, count, output_bucket, next_distance, rng);
    }
}

void process_own_bucket(
        outer_list_node* curr, outer_list_node* next_node,
        int tid, int sz
    ) {
    const int* bucket = curr->buckets[tid].list;
    customList& output_bucket = next_node->buckets[tid];
    int& last_done = curr->progress[tid].last_done;
    int last = sz - 1;

    for(int i=0; i<sz; i++) {
        prefetch_vertex_header(bucket[min(i+header_prefetch_distance, last)]);
        prefetch_vertex_payload(bucket[min(i+payload_prefetch_distance, last)]);

        int u = bucket[i];
        if(!vis[u]) {
            int du = dist[u];
            process_neighbors(u, output_bucket, du+1);
            vis[u] = 1;
        }
        // helpers skip entries up to here; vis is set only once a vertex is done
        last_done = i;
    }
}

void process_other_bucket(
        outer_list_node* curr, outer_list_node* next_node,
        int work_on, int tid, int sz, mt19937& rng
    ) {
    int start = curr->progress[work_on].last_done + 1;
    if(start >= sz) return;
    int remaining = sz - start;
    int last = remaining - 1;

    blockWalk walk = start_block_walk(remaining, rng);

    const int* bucket = curr->buckets[work_on].list + start;
    customList& output_bucket = next_node->buckets[tid];

    for(size_t b=0; b<walk.count; b++) {
        int base = static_cast<int>(walk.upcoming[0])*helper_block;
        int end = min(base + helper_block, remaining);
        for(int i=base; i<end; i++) {
            int offset = i - base;
            __builtin_prefetch(
                &vis[bucket[entry_ahead(walk, offset, vis_prefetch_distance, last)]], 0, 2);
            prefetch_vertex_header(unvisited_or_root(
                bucket[entry_ahead(walk, offset, header_prefetch_distance, last)]));
            prefetch_vertex_payload(unvisited_or_root(
                bucket[entry_ahead(walk, offset, payload_prefetch_distance, last)]));

            int u = bucket[i];
            if(vis[u]) continue;

            int du = dist[u];
            process_neighbors_helping(u, output_bucket, du+1, rng);
            vis[u] = 1;
        }
        advance_block_walk(walk);
    }
}

struct vertexRange {
    int begin;
    int end;
};

// bottom-up levels hand out vertices in small blocks: block b belongs to thread
// b % num_t, so every thread's blocks sample the whole id range, and a level
// whose work sits in one part of that range still spreads evenly. As with
// top-down buckets, each owner publishes the last of its blocks it finished,
// and helpers take the ones after that. Blocks much smaller than this are
// slower, since each starts a new run through the neighbor lists that the
// hardware prefetcher has to pick up again
constexpr int bottom_up_block = 1024;
int block_count;

[[gnu::always_inline]] inline int owned_blocks(int t) {
    return t < block_count ? (block_count - 1 - t) / num_t + 1 : 0;
}

// the vertices of thread t's k-th block
[[gnu::always_inline]] inline vertexRange owned_block(int t, int k) {
    int begin = (t + k*num_t) * bottom_up_block;
    return {begin, min(begin + bottom_up_block, N)};
}

// the bottom-up level at which a helper last finished each block, so other
// helpers and a late owner skip it; without it, every helper walks all of a
// stalled owner's blocks. Laid out by owner, so an owner's entries share lines
// only with its helpers. A stale thread can only write an older level, which
// costs a repeated scan but never a skipped block
vector<int> helped_at;
int blocks_per_owner;

[[gnu::always_inline]] inline int& helped_at_level(int t, int k) {
    return helped_at[t * blocks_per_owner + k];
}

[[gnu::always_inline]] inline bool has_frontier_neighbor(
        const int* neighbors, size_t count, int depth) {
    for(size_t j=0; j<count; j++) {
        // if(dist[neighbors[j]] == depth) return true;
        int v = neighbors[j];
        int dv = dist[v];
        if(dv == depth) return true;
    }
    return false;
}

// has_frontier_neighbor on dist8, for a level with a code
[[gnu::always_inline]] inline bool has_frontier_neighbor8(
        const int* neighbors, size_t count, uint8_t code) {
    for(size_t j=0; j<count; j++) {
        if(dist8[neighbors[j]] == code) return true;
    }
    return false;
}

// the neighbor test is inlined, since most tests stop after a few neighbors
// and the call around each was a large part of the cost
void scan_block(
        vertexRange range, int depth, int code, customList& output_bucket) {
    // the block's undiscovered vertices, gathered without branching on each:
    // every vertex is written, but the count moves only past undiscovered
    // ones; on some levels about half are, and a branch on it mispredicts often
    int candidates[bottom_up_block];
    int n = 0;
    for(int u=range.begin; u<range.end; u++) {
        candidates[n] = u;
        n += dist[u] == -1;
    }
    if(code > 0) {
        // what this level finds is the next level's frontier, under the next
        // code, which the plan reserved for it
        uint8_t frontier_code = static_cast<uint8_t>(code);
        uint8_t next_code = code < max_code ? static_cast<uint8_t>(code + 1) : 0;
        for(int i=0; i<n; i++) {
            int u = candidates[i];
            // read once: the barriers in discover_vertex would make the
            // compiler read the offsets again for the degree
            size_t begin = offsets[u], end = offsets[u+1];
            if(has_frontier_neighbor8(adj.data() + begin, end - begin, frontier_code)) {
                discover_vertex(u, output_bucket, depth + 1, next_code);
                output_bucket.degree_sum += end - begin;
            }
        }
        return;
    }
    for(int i=0; i<n; i++) {
        int u = candidates[i];
        size_t begin = offsets[u], end = offsets[u+1];
        if(has_frontier_neighbor(adj.data() + begin, end - begin, depth)) {
            discover_vertex(u, output_bucket, depth + 1, 0);
            output_bucket.degree_sum += end - begin;
        }
    }
}

// own blocks, in order, publishing each as the last one done and skipping those
// a helper got to first; a helper that finishes the rest marks the owner done,
// which stops it
void fetch_own_blocks(
        outer_list_node* curr, customList& output_bucket, int tid, int code) {
    int depth = curr->depth;
    int count = owned_blocks(tid);
    int& last_done = curr->progress[tid].last_done;
    for(int k=0; k<count && !curr->done[tid]; k++) {
        if(helped_at_level(tid, k) != depth) {
            scan_block(owned_block(tid, k), depth, code, output_bucket);
        }
        // helpers skip blocks up to here, so their vertices must be marked first
        compiler_barrier();
        last_done = k;
    }
    curr->done[tid] = 1;
}

// the blocks each other owner hasn't done yet, in a random order as top-down
// helpers take bucket entries, so helpers on the same owner spread out; blocks
// the owner or another helper has done since are skipped, and a helper that
// gets through the rest marks the owner done
void fetch_other_blocks(
        outer_list_node* curr, customList& output_bucket, int tid,
        mt19937& rng, int code) {
    int depth = curr->depth;
    for(int k=1; k<num_t; k++) {
        int owner = (tid + k) % num_t;
        if(curr->done[owner]) continue;
        int start = curr->progress[owner].last_done + 1;
        int remaining = owned_blocks(owner) - start;
        affinePermutation order = affine_shuffle(max(remaining, 0), rng);
        size_t index = order.index;
        for(int i=0; i<remaining; i++) {
            // read again for every block, since the owner and other helpers go on
            compiler_barrier();
            if(curr->done[owner]) break;
            int block = start + static_cast<int>(index);
            if(block > curr->progress[owner].last_done &&
                    helped_at_level(owner, block) != depth) {
                scan_block(owned_block(owner, block), depth, code, output_bucket);
                // others skip the block once this is set, so its vertices must be marked first
                compiler_barrier();
                helped_at_level(owner, block) = depth;
            }
            index = next_affine_index(index, order.step, remaining);
        }
        curr->done[owner] = 1;
    }
}

// a level that loses the race to be linked stays in this thread's arena until
// the search is over
void add_node(outer_list_node* curr, int tid) {
    outer_list_node* new_node = new_level(curr->depth + 1, arenas[tid]);
    outer_list_node* expected = nullptr;
    atomic_compare_exchange_strong(&(curr->next), &expected, new_node);
}

void top_down_level(
        outer_list_node* curr, int tid, mt19937& rng) {
    int work_on = tid;
    bool next_is_null = true;

    for(int _=0; _<num_t; _++) {
        if(curr->done[work_on] || curr->buckets[work_on].size == 0) {
            work_on++;
            if(work_on>=num_t) work_on -= num_t;
            continue;
        }

        if(next_is_null && curr->next == nullptr) {
            add_node(curr, tid);
        }
        next_is_null = false;

        outer_list_node* next_node = curr->next.load();

        int sz = curr->buckets[work_on].size;
        // size must be read before the list pointer (see push_back)
        compiler_barrier();

        if(work_on == tid) {
            process_own_bucket(curr, next_node, tid, sz);
        }
        else {
            process_other_bucket(
                curr, next_node, work_on, tid, sz, rng);
        }

        curr->done[work_on] = true;
        work_on++;
        if(work_on>=num_t) work_on -= num_t;
    }
}

// writes `code` for a frontier that top-down pushes left without one: this
// thread's own bucket in order, publishing how far it got, then whatever is left
// of each bucket no thread has finished. Every bucket is done when it returns,
// so the frontier test can start; a code written twice or late is still right,
// since every entry of the level's buckets is at the level
void write_frontier_codes(outer_list_node* curr, int tid, int code) {
    uint8_t frontier_code = static_cast<uint8_t>(code);
    for(int k=0; k<num_t; k++) {
        int owner = (tid + k) % num_t;
        if(curr->codes_done[owner]) continue;
        const customList& bucket = curr->buckets[owner];
        int sz = bucket.size;
        // size must be read before the list pointer (see push_back)
        compiler_barrier();
        const int* list = bucket.list;
        int& progress = curr->codes_progress[owner].last_done;
        if(owner == tid) {
            for(int i=0; i<sz; i++) {
                dist8[list[i]] = frontier_code;
                // helpers start past here, so the code must be written first
                compiler_barrier();
                progress = i;
            }
        }
        else {
            for(int i=progress+1; i<sz; i++) dist8[list[i]] = frontier_code;
        }
        // others skip the bucket once this is set, so its codes must be written first
        compiler_barrier();
        curr->codes_done[owner] = 1;
    }
}

void bottom_up_level(
        outer_list_node* curr, int tid, mt19937& rng, long long undiscovered,
        level_direction previous, int code) {
    if(curr->next == nullptr) {
        add_node(curr, tid);
    }
    // after a top-down level the code is fresh, and nothing has written it yet
    if(previous == top_down && code > 0) {
        write_frontier_codes(curr, tid, code);
    }
    outer_list_node* next_node = curr->next.load();
    customList& output_bucket = next_node->buckets[tid];
    // the level finds at most the undiscovered vertices, and at most one per
    // edge leaving the frontier, which keeps long thin tails small; a thread
    // finds them in its own blocks unless it helps, and those hold about a
    // num_t-th of them. Sizing the bucket for that, with a quarter to spare,
    // saves copying it at every doubling. Capacity never written costs no
    // memory traffic
    long long frontier_degrees = 0;
    for(int t=0; t<num_t; t++) frontier_degrees += curr->buckets[t].degree_sum;
    long long share = min(undiscovered, frontier_degrees) / num_t;
    output_bucket.reserve(static_cast<int>(share + share / 4));

    fetch_own_blocks(curr, output_bucket, tid, code);
    fetch_other_blocks(curr, output_bucket, tid, rng, code);
}

// the thresholds dir-bfs.cpp uses, so both switch at the same frontier sizes
constexpr long long gapbs_alpha = 15;
constexpr long long gapbs_beta = 18;

[[gnu::always_inline]] inline level_plan propose_plan(
        const level_plan& before, long long frontier_size, long long scout) {
    long long undiscovered = before.undiscovered - frontier_size;
    // an empty frontier ends the search, which only a top-down level detects
    if(frontier_size == 0) {
        return {top_down, before.edges_to_check, 0, undiscovered, 0, 0};
    }
    // stay bottom-up while the frontier grows or is still large
    if(before.direction == bottom_up &&
            (frontier_size >= before.frontier_size || frontier_size > N / gapbs_beta)) {
        return {bottom_up, before.edges_to_check, frontier_size, undiscovered, 0, 0};
    }
    // go bottom-up once the frontier's edges are a large share of those unexplored
    if(scout > before.edges_to_check / gapbs_alpha) {
        return {bottom_up, before.edges_to_check, frontier_size, undiscovered, 0, 0};
    }
    return {top_down, before.edges_to_check - scout, frontier_size, undiscovered, 0, 0};
}

// a bottom-up level after a bottom-up one tests the code its discoveries wrote,
// and any other bottom-up level takes the next unused code, which its first act
// writes for the frontier; either way its own discoveries take the code after
// it. Codes only grow within a search, so none is ever reused
[[gnu::always_inline]] inline void assign_code(
        const level_plan& before, level_plan& plan) {
    plan.code = 0;
    plan.last_code = before.last_code;
    if(plan.direction != bottom_up) return;
    int code = before.direction == bottom_up && before.code > 0
        ? before.code + 1 : before.last_code + 1;
    if(code <= max_code) {
        plan.code = code;
        plan.last_code = min(code + 1, max_code);
    }
}

// every thread must expand a level the same way, since its progress entries
// hold bucket positions top-down and block positions bottom-up; each thread
// proposes a plan from what it sees, and the first to claim the level decides
level_plan plan_level(
        outer_list_node* curr, const level_plan& before, int tid) {
    int owner = curr->plan_owner.load();
    if(owner >= 0) return curr->proposals[owner];

    long long frontier_size = 0;
    for(int t=0; t<num_t; t++) frontier_size += curr->buckets[t].size;

    // the edge count restarts at 1 after bottom-up levels, as GAPBS's does
    long long scout = 1;
    if(before.direction == top_down) {
        scout = 0;
        for(int t=0; t<num_t; t++) scout += curr->buckets[t].degree_sum;
    }

    level_plan plan = propose_plan(before, frontier_size, scout);
    assign_code(before, plan);
    curr->proposals[tid] = plan;
    if(!curr->plan_owner.compare_exchange_strong(owner, tid)) {
        return curr->proposals[owner];
    }
    return curr->proposals[tid];
}

void wf_bfs(outer_list_node* head, int tid, mt19937& rng) {
    // before the first level every edge is unexplored
    level_plan plan = {top_down, static_cast<long long>(offsets[N]), 1, N, 0, 0};
    outer_list_node* curr = head;

    while(curr != nullptr) {
        level_direction previous = plan.direction;
        plan = plan_level(curr, plan, tid);
        if(plan.direction == bottom_up) {
            bottom_up_level(curr, tid, rng, plan.undiscovered, previous, plan.code);
        }
        else {
            top_down_level(curr, tid, rng);
        }
        curr = curr->next;
    }
}

void init(int N, size_t M) {
    dist.resize(N, -1);
    dist8.resize(N, 0);
    vis.resize(N, 0);
    block_count = (N + bottom_up_block - 1) / bottom_up_block;
}

// fills each vertex's neighbors in file order, so the lists match wf-bfs.cpp's
void load_graph(const string& path) {
    graph_helpers::edge_list edges = graph_helpers::read_graph(path);
    N = edges.n;
    M = edges.from.size();
    init(N, M);
    graph_helpers::build_csr(edges, offsets, adj);
}

// a search starts from vertex 0, the one entry of its first level. The threads
// are idle while it is set up, so it can come from thread 0's arena
outer_list_node* make_first_level() {
    outer_list_node* head = new_level(0, arenas[0]);
    head->buckets[0].push_back(0);
    head->buckets[0].degree_sum = degree(0);
    return head;
}

// only once every thread has returned, since until then one may still be
// reading a bucket
void free_levels() {
    for(int t=0; t<num_t; t++) arenas[t].reset();
}

// thread tid's share of what a search starts from: a slice of dist, dist8 and
// vis, with the source at 0, and its own row of helped_at, whose entries left
// from the last search would match this one's levels
void reset_share(int tid) {
    int begin = static_cast<int>(1LL * N * tid / num_t);
    int end = static_cast<int>(1LL * N * (tid + 1) / num_t);
    fill(dist.begin() + begin, dist.begin() + end, -1);
    fill(dist8.begin() + begin, dist8.begin() + end, 0);
    fill(vis.begin() + begin, vis.begin() + end, 0);
    if(begin == 0 && end > 0) dist[0] = 0;
    fill(helped_at.begin() + tid * blocks_per_owner,
         helped_at.begin() + (tid + 1) * blocks_per_owner, -1);
}

// the threads live for the whole run, as GAPBS's OpenMP threads do, so no
// search pays for starting them. The main thread sets up each search and
// publishes its number; every thread then resets its share, searches, and
// notes when it returned
struct search_control {
    outer_list_node* head = nullptr;
    // the search to run, numbered from 1; -1 stops the threads
    atomic<int> number{0};
    // shares reset and searches returned, counted over all searches so far
    atomic<int> reset{0};
    atomic<int> returned{0};
    mutex returned_mutex;
    condition_variable all_returned;
    vector<high_resolution_clock::time_point> return_times;
};
search_control searches;

void search_thread(int tid, int cpu) {
    cpu_helpers::pin_current_thread(cpu);
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

    for(int last = 0; ; ) {
        int number;
        while((number = searches.number.load()) == last) __builtin_ia32_pause();
        if(number < 0) return;
        last = number;

        reset_share(tid);
        // a thread still resetting could overwrite a distance another has
        // already set, so no thread searches before every share is reset
        searches.reset++;
        while(searches.reset.load() < number * num_t) __builtin_ia32_pause();

        wf_bfs(searches.head, tid, rng);
        searches.return_times[tid] = high_resolution_clock::now();
        if(++searches.returned == number * num_t) {
            lock_guard<mutex> lock(searches.returned_mutex);
            searches.all_returned.notify_one();
        }
    }
}

void write_output() {
    graph_helpers::write_distances("wfd-out.txt", dist);
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);

    if(argc != 3) {
        cout << "Usage: " << argv[0] << " <input_file> <num_threads>\n";
        return 1;
    }

    try {
        load_graph(argv[1]);
    }
    catch(const exception& error) {
        cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    num_t = stoi(argv[2]);

    vector<int> cpu_ids;
    try {
        cpu_ids = cpu_helpers::select_idle_cpus(num_t);
    }
    catch(const exception& error) {
        cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    cerr << "Selected CPUs:";
    for(int cpu: cpu_ids) cerr << " " << cpu;
    cerr << "\n";

    // sized once the thread count is known to be valid, since owners divide by it
    blocks_per_owner = owned_blocks(0);
    helped_at.assign(static_cast<size_t>(num_t) * blocks_per_owner, -1);

    arenas = vector<arena>(num_t);
    searches.return_times.resize(num_t);
    vector<thread> threads;
    for(int i=0; i<num_t; i++) {
        threads.emplace_back(search_thread, i, cpu_ids[i]);
    }

    constexpr int repetitions = 20;
    long long total_duration = 0;

    for(int i=1; i<=repetitions; i++) {
        // counted one search at a time, since in between the threads only spin
        control_perf("enable");
        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        searches.head = make_first_level();
        searches.number = i;
        {
            unique_lock<mutex> lock(searches.returned_mutex);
            searches.all_returned.wait(
                lock, [&] { return searches.returned.load() == i * num_t; });
        }
        control_perf("disable");
        // a search ends when its first thread returns: a thread returns only
        // once every level is done, so the distances are final by then, and
        // the threads still running can only write the same ones again
        high_resolution_clock::time_point t2 = *min_element(
            searches.return_times.begin(), searches.return_times.end());

        total_duration += duration_cast<microseconds>(t2 - t1).count();
        free_levels();
    }

    searches.number = -1;
    for(thread& t: threads) t.join();

    auto average_duration = total_duration / repetitions;

    write_output();

    cout << average_duration << "\n";

    return 0;
}
