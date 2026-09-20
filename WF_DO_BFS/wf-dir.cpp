#include <bits/stdc++.h>
#include <semaphore.h>
using namespace std;
using namespace chrono;

#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")

int N, M;
vector<vector<int>> adj;
vector<int> dist, vis, vis_forw, unvis_nodes_edges;

int num_t;

int a = 14;

class customList {
    public:
    int* list;
    int size = 0;
    int capacity = 1;
    vector<int*> prevPointers;

    customList() {
        list = new int[capacity];
    }

    void push_back(int x) {
        if(size == capacity) {
            capacity *= 2;
            int* new_list = new int[capacity];
            std::copy(list, list+size, new_list);
            prevPointers.push_back(list);
            list = new_list;
        }
        list[size++] = x;
    }

    void copy(int* arr, int sz) {
        int* ptr = list;
        std::copy(ptr, ptr+sz, arr);
    }

    void clear() {
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

class outer_list_node {
    public:
    int depth;
    vector<customList> buckets;
    vector<bool> done;
    atomic<outer_list_node*> next;
    vector<int> frontier_node_cnt;
    vector<int> frontier_edge_cnt;
    vector<bool> done_calcs;
    outer_list_node() {
        next = nullptr;
    }
};

class raiiArray {
    public:
    int* arr;
    raiiArray(int sz) {
        arr = new int[sz];
    }
    ~raiiArray() {
        delete[] arr;
    }
};

void add_node(outer_list_node* curr) {
    outer_list_node* new_node = new outer_list_node;
    new_node->depth = curr->depth + 1;
    new_node->buckets.resize(num_t);
    new_node->done.resize(num_t, 0);
    new_node->frontier_node_cnt.resize(num_t, -1);
    new_node->frontier_edge_cnt.resize(num_t, -1);
    new_node->done_calcs.resize(num_t, 0);
    outer_list_node* expected = nullptr;
    if(!atomic_compare_exchange_strong(&(curr->next), &expected, new_node)) {
        delete new_node;
    }
}

void populate_forward(outer_list_node* curr, int tid, int work_on, mt19937 &rng) {
    outer_list_node* next_node = curr->next.load();

    int sz = curr->buckets[work_on].size;

    if(work_on == tid) {
        for(int i=0; i<sz; i++) {
            int u = curr->buckets[work_on].list[i];
            if(vis[u]) continue;
            for(auto v: adj[u]) {
                if(dist[v] == -1) {
                    next_node->buckets[tid].push_back(v);
                    dist[v] = dist[u]+1;
                }
            }
            vis[u] = 1;
        }
    }
    else {
        raiiArray list(sz);
        curr->buckets[work_on].copy(list.arr, sz);
        shuffle(list.arr, list.arr+sz, rng);
        for(int i=0; i<sz; i++) {
            int u = list.arr[i];
            if(vis[u]) continue;
            for(auto v: adj[u]) {
                if(dist[v] == -1) {
                    next_node->buckets[tid].push_back(v);
                    dist[v] = dist[u]+1;
                }
            }
            vis[u] = 1;
        }
    }
}

void fetch_from_back(outer_list_node* curr, int tid, int work_on, mt19937 &rng) {
    outer_list_node* next_node = curr->next.load();

    int start_node = (N/num_t)*work_on;
    int end_node = work_on == num_t-1 ? N : (N/num_t)*(work_on+1);

    if(tid == work_on) {
        for(int u=start_node; u<end_node; u++) {
            if(vis_forw[u] >= curr->depth) continue;
            if(dist[u] != -1) continue;
            for(auto v: adj[u]) {
                if(dist[v] == curr->depth) {
                    next_node->buckets[tid].push_back(u);
                    dist[u] = curr->depth + 1;
                    break;
                }
            }
            vis_forw[u] = curr->depth;
        }
    }
    else {
        int sz = end_node - start_node;
        raiiArray list(sz);
        for(int i=start_node; i<end_node; i++) {
            list.arr[i - start_node] = i;
        }
        shuffle(list.arr, list.arr+sz, rng);
        for(int i=0; i<sz; i++) {
            int u = list.arr[i];
            if(vis_forw[u] >= curr->depth) continue;
            if(dist[u] != -1) continue;
            for(auto v: adj[u]) {
                if(dist[v] == curr->depth) {
                    next_node->buckets[tid].push_back(u);
                    dist[u] = curr->depth + 1;
                    break;
                }
            }
            vis_forw[u] = curr->depth;
        }
    }
}

bool switched = false, switched_back = false;

void wf_bfs(outer_list_node* head, int tid) {
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    outer_list_node* curr = head;

    while(true) {
        int work_on = tid;
        bool next_is_null = true;

        // int mf = 0, nf = 0;
        // for(int i=0; i<num_t; i++) {
        //     if(curr->frontier_node_cnt[i] != -1) {
        //         mf += curr->frontier_edge_cnt[i];
        //     }
        //     if(curr->done_calcs[i]) {
        //         nf += curr->frontier_node_cnt[i];
        //     }
        // }
        // unvis_nodes_edges[tid] -= mf;
        // int mu = unvis_nodes_edges[tid];

        // if(!switched_back && mf + nf > N + mu/a) {
        if(curr->depth >= 3 && curr->depth <= 7) {
            switched = true;
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

                fetch_from_back(curr, tid, work_on, rng);
                // populate_forward(curr, tid, work_on, rng);

                curr->done[work_on] = true;
                work_on++;
                if(work_on>=num_t) work_on -= num_t;
            }
        }
        else {
            if(switched) {
                switched_back = true;
            }
            for(int _=0; _<num_t; _++) {
                if(curr->done[work_on] || curr->buckets[work_on].size == 0) {
                    work_on++;
                    if(work_on>=num_t) work_on -= num_t;
                    continue;
                }

                if(next_is_null && curr->next == nullptr) {
                    add_node(curr);
                }
                next_is_null = false;

                populate_forward(curr, tid, work_on, rng);

                curr->done[work_on] = true;
                work_on++;
                if(work_on>=num_t) work_on -= num_t;
            }
        }
        curr = curr->next;

        if(curr == nullptr) break;

        // if(switched_back) continue;

        // for(int _=0; _<num_t; _++) {
        //     if(!curr->done_calcs[work_on]) {
        //         int node_cnt = 0, edge_cnt = 0;
        //         // int start_node = (N/num_t)*work_on;
        //         // int end_node = work_on == num_t-1 ? N : (N/num_t)*(work_on+1);
        //         // for(int u=start_node; u<end_node; u++) {
        //         //     if(dist[u] == curr->depth) {
        //         //         node_cnt++;
        //         //         edge_cnt += adj[u].size();
        //         //     }
        //         // }
        //         node_cnt = curr->buckets[work_on].size;
        //         for(int i=0; i<node_cnt; i++) {
        //             int u = curr->buckets[work_on].list[i];
        //             edge_cnt += adj[u].size();
        //         }
        //         curr->frontier_node_cnt[work_on] = node_cnt;
        //         curr->frontier_edge_cnt[work_on] = edge_cnt;
        //         curr->done_calcs[work_on] = true;
        //     }
        //     work_on++;
        //     if(work_on>=num_t) work_on -= num_t;
        // }

    }
}

void cleanUp(outer_list_node* head, int tid) {
    outer_list_node* curr = head;
    while(curr != nullptr) {
        outer_list_node* temp = curr;
        temp->buckets[tid].clear();
        curr = curr->next;
    }
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int i, j;
    
    FILE* f_in = fopen(argv[1], "r");

    fscanf(f_in, "%d %d", &N, &M);

    adj.resize(N);
    dist.resize(N, -1);
    vis.resize(N, 0);
    vis_forw.resize(N, 0);

    for(i=0; i<M; i++) {
        int x, y;
        fscanf(f_in, "%d %d", &x, &y);
        // x--; y--; ///////////////////////////////////////////// for sina weibo
        // x-=101; y-=101; ///////////////////////////////////////////// for friendster
        if(x>=N || y>=N) continue;
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);
    
    cin >> num_t;

    outer_list_node* head = new outer_list_node;
    head->depth = 0;
    head->buckets.resize(num_t);
    head->done.resize(num_t, 0);
    head->frontier_node_cnt.resize(num_t, -1);
    head->frontier_edge_cnt.resize(num_t, -1);
    head->done_calcs.resize(num_t, 0);
    head->buckets[0].push_back(0);
    head->frontier_node_cnt[0] = 1;
    head->frontier_edge_cnt[0] = adj[0].size();
    dist[0] = 0;

    unvis_nodes_edges.resize(num_t, 2*M);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    
    vector<thread> th;
    for(i=0; i<num_t; i++) {
        th.push_back(thread(wf_bfs, head, i));
    }
    for(i=0; i<num_t; i++) {
        th[i].join();
    }

    th.clear();
    for(i=0; i<num_t; i++) {
        th.push_back(thread(cleanUp, head, i));
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

    FILE* f_out = fopen("wfd-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);
    
    cout << duration << "\n";

    return 0;
}