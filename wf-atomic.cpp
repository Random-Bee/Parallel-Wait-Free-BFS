#include <bits/stdc++.h>
#include <semaphore.h>
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
atomic<int>* dist;
atomic<bool>* vis;

int num_t;

class customList {
    public:
    atomic<int*> list;
    atomic<int> size;
    atomic<int> capacity;
    atomic<int> head;
    atomic<int*>* prevPointers;
    int prevPointersSize = 0;
    int prevPointersCapacity = 1;

    customList() {
        list.store(new int[1], memory_order_relaxed);
        prevPointers = new atomic<int*>[1];
        capacity.store(1, memory_order_relaxed);
        size.store(0, memory_order_relaxed);
        head.store(0, memory_order_relaxed);
    }

    void push_back(int x) {
        if(size == capacity) {
            int new_capacity = capacity.load(memory_order_relaxed) * 2;
            capacity.store(new_capacity, memory_order_relaxed);

            int* new_list = new int[new_capacity];

            int* ptr = list.load(memory_order_relaxed);
            int sz = size.load(memory_order_relaxed);
            std::copy(ptr, ptr+sz, new_list);
            

            if(prevPointersSize == prevPointersCapacity) {
                int new_capacity = prevPointersCapacity * 2;
                atomic<int*> *new_prevPointers = new atomic<int*>[new_capacity];
                for(int i=0; i<prevPointersSize; i++) {
                    new_prevPointers[i].store(prevPointers[i].load(memory_order_relaxed), memory_order_relaxed);
                }
                delete[] prevPointers;
                prevPointers = new_prevPointers;
                prevPointersCapacity = new_capacity;
            }
            prevPointers[prevPointersSize++].store(ptr, memory_order_relaxed);

            list.store(new_list, memory_order_relaxed);
        }
        list[size++] = x;
    }

    void copy(int* arr, int sz) {
        int* ptr = list.load(memory_order_relaxed);
        std::copy(ptr, ptr+sz, arr);
    }

    void clear() {
        delete[] list;
        for(int i=0; i<prevPointersSize; i++) {
            delete[] prevPointers[i].load(memory_order_relaxed);
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
    customList* lists;
    atomic<bool>* done;
    atomic<outer_list_node*> next;

    outer_list_node(int num_t) {
        lists = new customList[num_t];
        done = new atomic<bool>[num_t];
        for(int i=0; i<num_t; i++) {
            done[i].store(0, memory_order_relaxed);
        }
        next.store(nullptr, memory_order_relaxed);
    }

    ~outer_list_node() {
        delete[] lists;
        delete[] done;
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

int threshold = 10;

void wf_bfs(outer_list_node* head, int tid) {
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    outer_list_node* curr = head;

    while(curr != nullptr) {
        int work_on = tid;
        bool next_is_null = true;

        for(int _=0; _<num_t; _++) {
            if(curr->done[work_on].load(memory_order_relaxed) || curr->lists[work_on].size.load(memory_order_relaxed) == 0) {
                work_on++;
                if(work_on>=num_t) work_on -= num_t;
                continue;
            }

            if(next_is_null && curr->next == nullptr) {
                outer_list_node* new_node = new outer_list_node(num_t);
                outer_list_node* expected = nullptr;
                if(!atomic_compare_exchange_strong(&(curr->next), &expected, new_node)) {
                    delete new_node;
                }
            }
            next_is_null = false;

            outer_list_node* next_node = curr->next.load();

            int sz = curr->lists[work_on].size.load(memory_order_relaxed);
            raiiArray list(sz);
            curr->lists[work_on].copy(list.arr, sz);

            bool fl = 1;
            if(sz>=threshold && work_on != tid) {
                shuffle(list.arr, list.arr+sz, rng);
                fl = 0;
            }

            for(int i=0; i<sz; i++) {
                int u = list.arr[i], d = dist[u].load(memory_order_relaxed);
                if(vis[u].load(memory_order_relaxed)) continue;
                if(fl && work_on!=tid) {
                    vector<int> neighbours = adj[u];
                    shuffle(neighbours.begin(), neighbours.end(), rng);
                    for(auto v: neighbours) {
                        if(dist[v].load(memory_order_relaxed) == -1) {
                            next_node->lists[tid].push_back(v);
                            dist[v].store(d+1, memory_order_relaxed);
                        }
                    }
                }
                else {
                    for(auto v: adj[u]) {
                        if(dist[v].load(memory_order_relaxed) == -1) {
                            next_node->lists[tid].push_back(v);
                            dist[v].store(d+1, memory_order_relaxed);
                        }
                    }
                }
                vis[u].store(1, memory_order_relaxed);
            }
            
            curr->done[work_on].store(1, memory_order_relaxed);
            work_on++;
            if(work_on>=num_t) work_on -= num_t;
        }

        curr = curr->next;
    }
}

void cleanUp(outer_list_node* head, int tid) {
    outer_list_node* curr = head;
    while(curr != nullptr) {
        outer_list_node* temp = curr;
        temp->lists[tid].clear();
        curr = curr->next;
    }
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int i, j;
    
    FILE* f_in = fopen(argv[1], "r");

    fscanf(f_in, "%d %d", &N, &M);

    adj.resize(N);
    dist = new atomic<int>[N];
    vis = new atomic<bool>[N];
    for(i=0; i<N; i++) {
        dist[i].store(-1, memory_order_relaxed);
        vis[i].store(0, memory_order_relaxed);
    }

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

    outer_list_node* head = new outer_list_node(num_t);
    head->lists[0].push_back(0);
    dist[0] = 0;

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

    FILE* f_out = fopen("wfa-out.txt", "w");

    int mx = 0;
    for(i=0; i<N; i++) {
        fprintf(f_out, "%d\n", dist[i].load(memory_order_relaxed));
        mx = max(mx, dist[i].load(memory_order_relaxed));
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", mx);

    fclose(f_out);

    delete[] dist;
    delete[] vis;
    
    cout << duration << "\n";

    return 0;
}