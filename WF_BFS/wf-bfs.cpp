#include <bits/stdc++.h>
#include <semaphore.h>
#include "../CPU_helpers/cpu_affinity.hpp"
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
vector<int> dist, vis;

int num_t;

// keeps the compiler from moving memory accesses across this point; x86 itself
// keeps stores in order and loads in order (SDM Vol. 3A, 8.2.2)
[[gnu::always_inline]] inline void compiler_barrier() {
    asm volatile("" ::: "memory");
}

// each thread rewrites its size on every push, and its old arrays' list on
// every growth, so no two buckets may share a cache line. Before C++17 a vector
// aligns its storage to only 16 bytes, so one line of padding can leave that
// list in the line of the next bucket's size; two lines keep them apart at any
// 16-byte alignment
class alignas(128) customList {
    public:
    int* list;
    int size = 0;
    int capacity = 1;
    vector<int*> prevPointers;

    [[gnu::noinline]] customList() {
        list = new int[capacity];
    }

    // readers load size before list, so publish the array contents, then list,
    // then size; otherwise a reader can index past what it is able to see
    [[gnu::noinline]] void push_back(int x) {
        if(size == capacity) {
            capacity *= 2;
            int* new_list = new int[capacity];
            std::copy(list, list+size, new_list);
            prevPointers.push_back(list);
            compiler_barrier();
            list = new_list;
        }
        list[size] = x;
        compiler_barrier();
        size++;
    }

    [[gnu::noinline]] void clear() {
        delete[] list;
        for(auto p: prevPointers) {
            delete[] p;
        }
    }

    // ~customList() {
    //     delete[] list;
    //     for(auto p: prevPointers) {
    //         delete[] p;
    //     }
    // }
};

// padded to a cache line, since each owner rewrites its entry after every vertex
struct alignas(64) bucket_progress {
    int last_done;
};

class outer_list_node {
    public:
    vector<customList> buckets;
    // one byte per flag: vector<bool> packs flags into shared words, so threads
    // setting different flags would race on the same word
    vector<uint8_t> done;
    vector<bucket_progress> progress;
    atomic<outer_list_node*> next;
    [[gnu::noinline]] outer_list_node() {
        next = nullptr;
    }
};

struct affinePermutation {
    size_t index;
    size_t step;
};

[[gnu::noinline]] size_t greatest_common_divisor(
        size_t first, size_t second) {
    while(second != 0) {
        size_t remainder = first % second;
        first = second;
        second = remainder;
    }
    return first;
}

[[gnu::noinline]] affinePermutation affine_shuffle(
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

[[gnu::always_inline]] inline size_t affine_index_ahead(
        size_t index, size_t step, size_t size, int distance) {
    for(int k=0; k<distance; k++) {
        index = next_affine_index(index, step, size);
    }
    return index;
}

[[gnu::noinline]] void name_current_thread(const char* role, int tid) {
    char name[16];
    snprintf(name, sizeof(name), "%s-%d", role, tid);
    pthread_setname_np(pthread_self(), name);
}

[[gnu::noinline]] void control_perf(const char* command) {
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

[[gnu::always_inline]] inline void prefetch_vertex_header(int u) {
    const char* entry = reinterpret_cast<const char*>(&adj[u]);
    __builtin_prefetch(entry, 0, 2);
    // vector<int> is 24 bytes, so its end pointer can start the next line
    __builtin_prefetch(entry + sizeof(int*), 0, 2);
    __builtin_prefetch(&vis[u], 0, 2);
    __builtin_prefetch(&dist[u], 0, 2);
}

[[gnu::always_inline]] inline void prefetch_vertex_payload(int u) {
    uintptr_t line = reinterpret_cast<uintptr_t>(adj[u].data()) & ~(cache_line_bytes - 1);
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
// another thread can still discover and push it
[[gnu::always_inline]] inline void discover_vertex(
        int v, customList& output_bucket, int next_distance) {
    output_bucket.push_back(v);
    compiler_barrier();
    dist[v] = next_distance;
}

[[gnu::noinline]] void process_neighbors_generic(
        const int* neighbors, size_t count,
        customList& output_bucket, int next_distance) {
    for(size_t j=0; j<count; j++) {
        int v = neighbors[j];
        if(dist[v] == -1) {
            discover_vertex(v, output_bucket, next_distance);
        }
    }
}

[[gnu::noinline]] void process_neighbors_long(
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
                discover_vertex(v, output_bucket, next_distance);
            }
        }
    }
    process_neighbors_generic(
        neighbors+j, count-j, output_bucket, next_distance);
}

[[gnu::always_inline]] inline void process_neighbors(
        const vector<int>& neighbors,
        customList& output_bucket, int next_distance) {
    size_t count = neighbors.size();
    if(count <= list_prefetch_lookahead) {
        process_neighbors_generic(
            neighbors.data(), count, output_bucket, next_distance);
    }
    else {
        process_neighbors_long(
            neighbors.data(), count, output_bucket, next_distance);
    }
}

[[gnu::noinline]] void process_own_bucket(
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
            process_neighbors(adj[u], output_bucket, du+1);
            vis[u] = 1;
        }
        // helpers skip entries up to here; vis is set only once a vertex is done
        last_done = i;
    }
}

[[gnu::noinline]] void process_other_bucket(
        outer_list_node* curr, outer_list_node* next_node,
        int work_on, int tid, int sz, mt19937& rng
    ) {
    int start = curr->progress[work_on].last_done + 1;
    if(start >= sz) return;
    int remaining = sz - start;

    affinePermutation order = affine_shuffle(remaining, rng);

    const int* bucket = curr->buckets[work_on].list + start;
    customList& output_bucket = next_node->buckets[tid];
    size_t list_size = remaining;
    size_t vis_index = affine_index_ahead(
        order.index, order.step, list_size, vis_prefetch_distance);
    size_t header_index = affine_index_ahead(
        order.index, order.step, list_size, header_prefetch_distance);
    size_t payload_index = affine_index_ahead(
        order.index, order.step, list_size, payload_prefetch_distance);

    for(int i=0; i<remaining; i++) {
        __builtin_prefetch(&vis[bucket[vis_index]], 0, 2);
        prefetch_vertex_header(unvisited_or_root(bucket[header_index]));
        prefetch_vertex_payload(unvisited_or_root(bucket[payload_index]));
        vis_index = next_affine_index(vis_index, order.step, list_size);
        header_index = next_affine_index(header_index, order.step, list_size);
        payload_index = next_affine_index(payload_index, order.step, list_size);

        int u = bucket[order.index];
        order.index = next_affine_index(order.index, order.step, list_size);
        if(vis[u]) continue;

        int du = dist[u];
        process_neighbors(adj[u], output_bucket, du+1);
        vis[u] = 1;
    }
}

[[gnu::noinline]] void wf_bfs(outer_list_node* head, int tid, int cpu) {
    name_current_thread("wf", tid);
    cpu_helpers::pin_current_thread(cpu);

    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    outer_list_node* curr = head;

    while(curr != nullptr) {
        int work_on = tid;
        bool next_is_null = true;

        for(int _=0; _<num_t; _++) {
            if(curr->done[work_on] || curr->buckets[work_on].size == 0) {
                work_on++;
                if(work_on>=num_t) work_on -= num_t;
                continue;
            }

            if(next_is_null && curr->next == nullptr) {
                outer_list_node* new_node = new outer_list_node;
                new_node->buckets.resize(num_t);
                new_node->done.resize(num_t, 0);
                new_node->progress.resize(num_t, bucket_progress{-1});
                outer_list_node* expected = nullptr;
                if(!atomic_compare_exchange_strong(&(curr->next), &expected, new_node)) {
                    delete new_node;
                }
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

        curr = curr->next;
    }
}

[[gnu::noinline]] void init(int N, int M) {
    adj.resize(N);
    dist.resize(N, -1);
    vis.resize(N, 0);
}

[[gnu::noinline]] void read_graph(FILE* f_in) {
    for(int i=0; i<M; i++) {
        int x, y;
        fscanf(f_in, "%d %d", &x, &y);
        x--; y--;
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
}

[[gnu::noinline]] void clean_up(outer_list_node* head, int tid, int cpu) {
    name_current_thread("cleanup", tid);
    cpu_helpers::pin_current_thread(cpu);

    outer_list_node* curr = head;
    while(curr != nullptr) {
        outer_list_node* temp = curr;
        temp->buckets[tid].clear();
        curr = curr->next;
    }
}

[[gnu::noinline]] void init_and_start_threads(
        const vector<int>& cpu_ids) {
    outer_list_node* head = new outer_list_node;
    head->buckets.resize(num_t);
    head->done.resize(num_t, 0);
    head->progress.resize(num_t, bucket_progress{-1});
    head->buckets[0].push_back(0);
    dist[0] = 0;

    vector<thread> th;
    for(int i=0; i<num_t; i++) {
        th.push_back(thread(wf_bfs, head, i, cpu_ids[i]));
    }
    for(int i=0; i<num_t; i++) {
        th[i].join();
    }

    th.clear();
    for(int i=0; i<num_t; i++) {
        th.push_back(thread(clean_up, head, i, cpu_ids[i]));
    }
    for(int i=0; i<num_t; i++) {
        th[i].join();
    }

    while(head != nullptr) {
        outer_list_node* temp = head;
        head = head->next;
        delete temp;
    }
}

[[gnu::noinline]] void write_output() {
    FILE* f_out = fopen("wf-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);
}

[[gnu::noinline]] int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);

    if(argc != 3) {
        cout << "Usage: " << argv[0] << " <input_file> <num_threads>\n";
        return 1;
    }

    FILE* f_in = fopen(argv[1], "r");

    if(f_in == nullptr) {
        cout << "Error: Failed to open input file\n";
        return 1;
    }

    fscanf(f_in, "%d %d", &N, &M);

    init(N, M);

    read_graph(f_in);

    fclose(f_in);

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

    constexpr int repetitions = 20;
    long long total_duration = 0;

    control_perf("enable");

    for(int i=0; i<repetitions; i++) {
        fill(dist.begin(), dist.end(), -1);
        fill(vis.begin(), vis.end(), 0);

        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        init_and_start_threads(cpu_ids);
        high_resolution_clock::time_point t2 = high_resolution_clock::now();

        total_duration += duration_cast<microseconds>(t2 - t1).count();
    }

    control_perf("disable");

    auto average_duration = total_duration / repetitions;

    write_output();

    cout << average_duration << "\n";

    return 0;
}