#include <bits/stdc++.h>
#include <semaphore.h>
using namespace std;
using namespace chrono;

// Compiler optimization hints
#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")

int N, M;
vector<vector<int>> adj;
vector<int> dist, vis, vis_forw, unvis_nodes_edges;

int num_t;

int a = 14, b = 24;

class outer_list_node {
    public:
    int depth;
    vector<bool> done;
    atomic<outer_list_node*> next;
    vector<int> frontier_node_cnt;
    vector<int> frontier_edge_cnt;
    vector<bool> done_calcs;
    outer_list_node() {
        next = nullptr;
    }
};


void add_node(outer_list_node* curr) {
    outer_list_node* new_node = new outer_list_node;
    new_node->depth = curr->depth + 1;
    // Reserve space to avoid multiple allocations
    new_node->done.reserve(num_t);
    new_node->frontier_node_cnt.reserve(num_t);
    new_node->frontier_edge_cnt.reserve(num_t);
    new_node->done_calcs.reserve(num_t);
    
    new_node->done.resize(num_t, 0);
    new_node->frontier_node_cnt.resize(num_t, 0);
    new_node->frontier_edge_cnt.resize(num_t, 0);
    new_node->done_calcs.resize(num_t, 0);
    outer_list_node* expected = nullptr;
    if(!atomic_compare_exchange_strong(&(curr->next), &expected, new_node)) {
        delete new_node;
    }
}

void populate_forward(outer_list_node* curr, int tid, int work_on, mt19937 &rng) {
    const int start_node = (N/num_t)*work_on;
    const int end_node = work_on == num_t-1 ? N : (N/num_t)*(work_on+1);
    const int curr_depth = curr->depth;

    // Use static thread_local storage to avoid repeated allocations
    static thread_local vector<int> nodes;
    const int sz = end_node - start_node;
    nodes.resize(sz);
    iota(nodes.begin(), nodes.end(), start_node);

    if(work_on != tid) shuffle(nodes.begin(), nodes.end(), rng);

    // Cache-friendly access with prefetching hints
    for(int i = 0; i < sz; ++i) {
        const int u = nodes[i];
        if(__builtin_expect(vis[u] >= curr_depth, 0)) continue;
        if(__builtin_expect(dist[u] != curr_depth, 0)) continue;
        
        const auto& adj_u = adj[u];
        for(const int v : adj_u) {
            if(dist[v] == -1) {
                dist[v] = dist[u]+1;
            }
        }
        vis[u] = curr_depth;
    }
}

void fetch_from_back(outer_list_node* curr, int tid, int work_on, mt19937 &rng) {
    const int start_node = (N/num_t)*work_on;
    const int end_node = work_on == num_t-1 ? N : (N/num_t)*(work_on+1);
    const int curr_depth = curr->depth;

    // Use static thread_local storage to avoid repeated allocations
    static thread_local vector<int> nodes;
    const int sz = end_node - start_node;
    nodes.resize(sz);
    iota(nodes.begin(), nodes.end(), start_node);

    if(work_on != tid) shuffle(nodes.begin(), nodes.end(), rng);

    // Cache-friendly access with prefetching hints
    for(int i = 0; i < sz; ++i) {
        const int u = nodes[i];
        if(__builtin_expect(vis_forw[u] >= curr_depth, 0)) continue;
        if(__builtin_expect(dist[u] != -1, 0)) continue;
        
        const auto& adj_u = adj[u];
        for(const int v : adj_u) {
            if(dist[v] == curr_depth) {
                dist[u] = curr_depth + 1;
                break;
            }
        }
        vis_forw[u] = curr_depth;
    }
}

void wf_bfs(outer_list_node* head, int tid) {
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    outer_list_node* curr = head;

    while(true) {
        int work_on = tid;
        bool next_is_null = true;
        bool imp = false;

        int mf = 0, nf = 0;
        for(int i=0; i<num_t; i++) {
            mf += curr->frontier_edge_cnt[i];
            nf += curr->frontier_node_cnt[i];
            if(curr->frontier_node_cnt[i]) imp |= true;
        }
        unvis_nodes_edges[tid] -= mf;
        int mu = unvis_nodes_edges[tid];

        if(!imp) break;

        for(int _=0; _<num_t; _++) {
            if(curr->done[work_on]) {
                work_on++;
                if(work_on>=num_t) work_on -= num_t;
                continue;
            }

            if(next_is_null && curr->next == nullptr) {
                add_node(curr);
            }
            next_is_null = false;

            if(mf > mu/a || nf > N / b) {
                fetch_from_back(curr, tid, work_on, rng);
            }
            else {
                populate_forward(curr, tid, work_on, rng);
            }

            curr->done[work_on] = true;
            work_on++;
            if(work_on>=num_t) work_on -= num_t;
        }
        curr = curr->next;

        for(int _=0; _<num_t; _++) {
            if(!curr->done_calcs[work_on]) {
                int node_cnt = 0, edge_cnt = 0;
                const int start_node = (N/num_t)*work_on;
                const int end_node = work_on == num_t-1 ? N : (N/num_t)*(work_on+1);
                const int target_depth = curr->depth;
                
                // Unroll loop for better performance
                for(int u=start_node; u<end_node; u++) {
                    if(__builtin_expect(dist[u] == target_depth, 0)) {
                        node_cnt++;
                        edge_cnt += adj[u].size();
                    }
                }
                curr->frontier_node_cnt[work_on] = node_cnt;
                curr->frontier_edge_cnt[work_on] = edge_cnt;
                curr->done_calcs[work_on] = true;
            }
            work_on++;
            if(__builtin_expect(work_on>=num_t, 0)) work_on -= num_t;
        }

    }
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int i, j;
    
    // Optimized file reading with larger buffer
    FILE* f_in = fopen(argv[1], "r");
    setvbuf(f_in, nullptr, _IOFBF, 32768); // 32KB buffer

    fscanf(f_in, "%d %d", &N, &M);

    // Reserve space to avoid multiple reallocations
    adj.resize(N);
    dist.resize(N, -1);
    vis.resize(N, -1);
    vis_forw.resize(N, 0);

    // Pre-reserve adjacency list space to reduce allocations
    for(i = 0; i < N; i++) {
        adj[i].reserve(8); // Reasonable starting capacity
    }

    for(i=0; i<M; i++) {
        int x, y;
        fscanf(f_in, "%d %d", &x, &y);
        // x--; y--; ///////////////////////////////////////////// for sina weibo
        // x-=101; y-=101; ///////////////////////////////////////////// for friendster
        if(__builtin_expect(x>=N || y>=N, 0)) continue;
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);
    
    cin >> num_t;

    outer_list_node* head = new outer_list_node;
    head->depth = 0;
    // Reserve space to avoid multiple allocations
    head->done.reserve(num_t);
    head->frontier_node_cnt.reserve(num_t);
    head->frontier_edge_cnt.reserve(num_t);
    head->done_calcs.reserve(num_t);
    
    head->done.resize(num_t, 0);
    head->frontier_node_cnt.resize(num_t, -1);
    head->frontier_edge_cnt.resize(num_t, -1);
    head->done_calcs.resize(num_t, 0);
    head->frontier_node_cnt[0] = 1;
    head->frontier_edge_cnt[0] = adj[0].size();
    dist[0] = 0;

    unvis_nodes_edges.resize(num_t, 2*M);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    
    // Reserve space to avoid reallocations during thread creation
    vector<thread> th;
    th.reserve(num_t);
    for(i=0; i<num_t; i++) {
        th.emplace_back(wf_bfs, head, i);
    }
    for(i=0; i<num_t; i++) {
        th[i].join();
    }

    while(head != nullptr) {
        outer_list_node* temp = head;
        head = head->next;
        delete temp;
    }

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    FILE* f_out = fopen("wfd2-out.txt", "w");
    setvbuf(f_out, nullptr, _IOFBF, 32768); // 32KB buffer for faster output

    for(const auto& d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);
    
    cout << duration << "\n";

    return 0;
}