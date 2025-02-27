#include <bits/stdc++.h>
#include <semaphore.h>
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
vector<int> dist, vis;

int num_t;

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
    vector<customList> buckets;
    vector<bool> done;
    atomic<outer_list_node*> next;
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

void wf_bfs(outer_list_node* head, int tid) {
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
                outer_list_node* expected = nullptr;
                if(!atomic_compare_exchange_strong(&(curr->next), &expected, new_node)) {
                    delete new_node;
                }
            }
            next_is_null = false;

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

            curr->done[work_on] = true;
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
    head->buckets.resize(num_t);
    head->done.resize(num_t, 0);
    head->buckets[0].push_back(0);
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

    FILE* f_out = fopen("wf-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);
    
    cout << duration << "\n";

    return 0;
}