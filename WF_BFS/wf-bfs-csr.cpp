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
// vis marks vertices already expanded
vector<int> dist, vis;

int num_t;

// keeps the compiler from moving memory accesses across this point; x86 itself
// keeps stores in order and loads in order (SDM Vol. 3A, 8.2.2)
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

// a thread's bucket of a level; it sits in the thread's entry of the level, which
// keeps it off the lines of other threads' buckets (see owner_entry)
class customList {
    public:
    int* list = nullptr;
    int size = 0;
    int capacity = 0;
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

// one thread's entries in a level. The first line holds what threads read as
// they walk the level: its bucket's array and size, and the flag, set once.
// Only the owner changes the bucket, nearly always while the level before is
// expanded, but an owner stalled in that level can still push to it, and grow
// it, after other threads have started or finished this one. Such late entries
// are vertices the threads that finished the level before pushed too, so
// readers that miss them miss nothing. The owner rewrites its progress after
// every entry it finishes, so that goes on the second line, where those writes
// don't take the first from the readers. Each entry fills its own pair of
// lines, which the L2 prefetcher fetches together, so no two threads' entries
// share one
struct alignas(128) owner_entry {
    // only the owner pushes to it
    customList bucket;
    // set by the thread that finishes the bucket
    uint8_t done = 0;
    // how far the owner has got: the last entry of its bucket it finished
    alignas(64) int last_done = -1;

    explicit owner_entry(arena* memory) : bucket(memory) {}
};
static_assert(sizeof(owner_entry) == 128, "an entry is a pair of lines");

// a level's per-thread entries are an array of num_t, built by new_level
class outer_list_node {
    public:
    int depth;
    owner_entry* owners;
    atomic<outer_list_node*> next;
    outer_list_node() {
        next = nullptr;
    }
};

vector<arena> arenas;

// a level, in the arena of the thread that adds it; bucket t's arrays come from
// thread t's arena, since only thread t pushes to it. Arena blocks start on
// 128-byte boundaries, so every entry fills a pair of lines
outer_list_node* new_level(int depth, arena& memory) {
    outer_list_node* node = new(memory.allocate(sizeof(outer_list_node))) outer_list_node;
    node->depth = depth;
    node->owners = static_cast<owner_entry*>(memory.allocate(sizeof(owner_entry) * num_t));
    for(int t=0; t<num_t; t++) {
        new(&node->owners[t]) owner_entry(&arenas[t]);
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

// helpers walk what is left of a list in blocks of helper_block entries: the
// blocks in a random order, the entries inside a block in order, so
// neighboring entries share cache lines
constexpr int helper_block = 16;

// push before marking: if this thread stops in between, v stays unmarked and
// another thread can still discover and push it. Every thread that marks v
// writes the same distance, so a plain store does
[[gnu::always_inline]] inline void discover_vertex(
        int v, customList& output_bucket, int next_distance) {
    output_bucket.push_back(v);
    compiler_barrier();
    dist[v] = next_distance;
}

void process_neighbors(
        int u, customList& output_bucket, int next_distance) {
    const int* neighbors = adj.data() + offsets[u];
    size_t count = offsets[u+1] - offsets[u];
    for(size_t j=0; j<count; j++) {
        int v = neighbors[j];
        if(dist[v] == -1) {
            discover_vertex(v, output_bucket, next_distance);
        }
    }
}

// helpers walk lists of up to three blocks in order, as owners do: a thread is
// on one only briefly, and shuffling it would cost two random draws
constexpr size_t ordered_list_max = 3 * helper_block;

// a helper walks a long list as it walks a bucket: in blocks of helper_block
// entries, the blocks in a random order of its own. An owner and helpers that
// expand the same hub then write different entries of dist at any moment;
// walking the list in the same order, they would all write the same ones, each
// write waiting for the line
void process_neighbors_shuffled(
        int u, customList& output_bucket, int next_distance, mt19937& rng) {
    const int* neighbors = adj.data() + offsets[u];
    size_t count = offsets[u+1] - offsets[u];
    if(count <= ordered_list_max) {
        process_neighbors(u, output_bucket, next_distance);
        return;
    }
    size_t blocks = (count + helper_block - 1) / helper_block;
    affinePermutation order = affine_shuffle(blocks, rng);
    size_t block = order.index;
    for(size_t b=0; b<blocks; b++) {
        size_t end = min((block + 1)*helper_block, count);
        for(size_t j=block*helper_block; j<end; j++) {
            int v = neighbors[j];
            if(dist[v] == -1) {
                discover_vertex(v, output_bucket, next_distance);
            }
        }
        block = next_affine_index(block, order.step, blocks);
    }
}

void process_own_bucket(
        outer_list_node* curr, outer_list_node* next_node,
        int tid, int sz
    ) {
    const int* bucket = curr->owners[tid].bucket.list;
    customList& output_bucket = next_node->owners[tid].bucket;
    int& last_done = curr->owners[tid].last_done;

    for(int i=0; i<sz; i++) {
        int u = bucket[i];
        if(!vis[u]) {
            int du = dist[u];
            process_neighbors(u, output_bucket, du+1);
            // others skip a marked vertex, so its neighbors must be pushed first
            compiler_barrier();
            vis[u] = 1;
        }
        // helpers skip entries up to here, so the entry must be done first
        compiler_barrier();
        last_done = i;
    }
}

void process_other_bucket(
        outer_list_node* curr, outer_list_node* next_node,
        int work_on, int tid, int sz, mt19937& rng
    ) {
    int start = curr->owners[work_on].last_done + 1;
    if(start >= sz) return;
    int remaining = sz - start;

    size_t blocks = (remaining + helper_block - 1) / helper_block;
    affinePermutation order = affine_shuffle(blocks, rng);
    size_t block = order.index;

    const int* bucket = curr->owners[work_on].bucket.list + start;
    customList& output_bucket = next_node->owners[tid].bucket;

    for(size_t b=0; b<blocks; b++) {
        int base = static_cast<int>(block)*helper_block;
        int end = min(base + helper_block, remaining);
        for(int i=base; i<end; i++) {
            int u = bucket[i];
            if(vis[u]) continue;

            int du = dist[u];
            process_neighbors_shuffled(u, output_bucket, du+1, rng);
            // others skip a marked vertex, so its neighbors must be pushed first
            compiler_barrier();
            vis[u] = 1;
        }
        block = next_affine_index(block, order.step, blocks);
    }
}

// the level this thread built but lost the race to link, if any (see add_node)
thread_local outer_list_node* spare_level = nullptr;

// threads reach a level together, so nearly all of them build its successor and
// all but one lose the race to link it. A level that loses was never seen by
// another thread, so it is kept for the next one this thread adds, and only its
// depth changes; otherwise every loser would stay in its thread's arena until
// the search is over
void add_node(outer_list_node* curr, int tid) {
    outer_list_node* new_node = spare_level;
    if(new_node == nullptr) {
        new_node = new_level(curr->depth + 1, arenas[tid]);
    }
    else {
        new_node->depth = curr->depth + 1;
    }
    outer_list_node* expected = nullptr;
    bool linked = atomic_compare_exchange_strong(&(curr->next), &expected, new_node);
    spare_level = linked ? nullptr : new_node;
}

void top_down_level(
        outer_list_node* curr, int tid, mt19937& rng) {
    int work_on = tid;
    bool next_is_null = true;

    for(int _=0; _<num_t; _++) {
        if(curr->owners[work_on].done || curr->owners[work_on].bucket.size == 0) {
            work_on++;
            if(work_on>=num_t) work_on -= num_t;
            continue;
        }

        if(next_is_null && curr->next == nullptr) {
            add_node(curr, tid);
        }
        next_is_null = false;

        outer_list_node* next_node = curr->next.load();

        int sz = curr->owners[work_on].bucket.size;
        // size must be read before the list pointer (see push_back)
        compiler_barrier();

        if(work_on == tid) {
            process_own_bucket(curr, next_node, tid, sz);
        }
        else {
            process_other_bucket(
                curr, next_node, work_on, tid, sz, rng);
        }

        // the flag shares its line with the bucket, which every thread reads at every
        // level; rewriting a set flag would take the line from all of them for nothing
        if(!curr->owners[work_on].done) curr->owners[work_on].done = true;
        work_on++;
        if(work_on>=num_t) work_on -= num_t;
    }
}

// a level that finds nothing links no successor, which ends the search
void wf_bfs(outer_list_node* head, int tid, mt19937& rng) {
    for(outer_list_node* curr = head; curr != nullptr; curr = curr->next) {
        // what this thread wrote in the level before may still be in its store
        // buffer, where only it sees it, and the first thread to read this
        // level's buckets must have made all of that visible to the others
        atomic_thread_fence(memory_order_seq_cst);
        top_down_level(curr, tid, rng);
    }
}

void init(int N) {
    dist.resize(N, -1);
    vis.resize(N, 0);
}

// fills each vertex's neighbors in file order, so the lists match wf-dir.cpp's
void load_graph(const string& path) {
    graph_helpers::edge_list edges = graph_helpers::read_graph(path);
    N = edges.n;
    M = edges.from.size();
    init(N);
    graph_helpers::build_csr(edges, offsets, adj);
}

// a search starts from vertex 0, the one entry of its first level. The threads
// are idle while it is set up, so it can come from thread 0's arena
outer_list_node* make_first_level() {
    outer_list_node* head = new_level(0, arenas[0]);
    head->owners[0].bucket.push_back(0);
    return head;
}

// only once every thread has returned, since until then one may still be
// reading a bucket
void free_levels() {
    for(int t=0; t<num_t; t++) arenas[t].reset();
}

// thread tid's share of what a search starts from: a slice of dist and vis,
// with the source at 0, and no spare level, since the arenas took the last one
// back
void reset_share(int tid) {
    spare_level = nullptr;
    int begin = static_cast<int>(1LL * N * tid / num_t);
    int end = static_cast<int>(1LL * N * (tid + 1) / num_t);
    fill(dist.begin() + begin, dist.begin() + end, -1);
    fill(vis.begin() + begin, vis.begin() + end, 0);
    if(begin == 0 && end > 0) dist[0] = 0;
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
    graph_helpers::write_distances("wf-csr-out.txt", dist);
}

// the timed searches on num_t threads pinned to cpu_ids, returning their
// average in microseconds. All that is sized by the thread count, and the
// counts the threads meet at, start afresh, so one thread count can follow
// another
long long run_searches(const vector<int>& cpu_ids) {
    arenas = vector<arena>(num_t);
    searches.return_times.assign(num_t, {});
    searches.number = 0;
    searches.reset = 0;
    searches.returned = 0;
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

    return total_duration / repetitions;
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);

    if(argc != 3) {
        cout << "Usage: " << argv[0] << " <input_file> <num_threads>[,<num_threads>...]\n";
        return 1;
    }

    try {
        load_graph(argv[1]);
    }
    catch(const exception& error) {
        cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    vector<int> thread_counts;
    try {
        thread_counts = cpu_helpers::parse_thread_counts(argv[2]);
    }
    catch(const exception& error) {
        cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    // given several thread counts, a line for each, its distances checked
    // against the first one's; the last one's are written
    vector<int> first_dist;
    long long average_duration = 0;
    for(size_t c = 0; c < thread_counts.size(); c++) {
        if(c > 0) this_thread::sleep_for(cpu_helpers::pause_between_thread_counts);
        num_t = thread_counts[c];

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

        average_duration = run_searches(cpu_ids);
        if(thread_counts.size() > 1) {
            if(c == 0) first_dist = dist;
            cout << num_t << " " << average_duration << " "
                 << (dist == first_dist ? "same" : "differs") << endl;
        }
    }

    write_output();

    if(thread_counts.size() == 1) cout << average_duration << "\n";

    return 0;
}
