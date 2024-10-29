#include <bits/stdc++.h>
#include <barrier>
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
vector<int> comps;
vector<bool> vis;

int num_t;

class customList {
    public:
    int* list;
    int size = 0;
    int capacity = 1;
    int head = 0;
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
    vector<customList> lists;
    vector<bool> done;
    atomic<outer_list_node*> next = nullptr;
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

int comp = 0;

outer_list_node* head = nullptr;

void wf_bfs(int tid) {
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    outer_list_node* curr = head;

    while(curr != nullptr) {
        int work_on = tid;
        bool next_is_null = true;

        for(int _=0; _<num_t; _++) {
            if(curr->done[work_on] || curr->lists[work_on].size == 0) {
                work_on++;
                if(work_on>=num_t) work_on -= num_t;
                continue;
            }

            if(next_is_null && curr->next == nullptr) {
                outer_list_node* new_node = new outer_list_node;
                new_node->lists.resize(num_t);
                new_node->done.resize(num_t, 0);
                outer_list_node* expected = nullptr;
                if(!atomic_compare_exchange_strong(&(curr->next), &expected, new_node)) {
                    delete new_node;
                }
            }
            next_is_null = false;

            outer_list_node* next_node = curr->next.load();

            int sz = curr->lists[work_on].size;
            raiiArray list(sz);
            curr->lists[work_on].copy(list.arr, sz);

            bool fl = 1;
            if(sz>=threshold && work_on != tid) {
                shuffle(list.arr, list.arr+sz, rng);
                fl = 0;
            }

            for(int i=0; i<sz; i++) {
                int u = list.arr[i];
                if(vis[u]) continue;
                if(fl && work_on!=tid) {
                    vector<int> neighbours = adj[u];
                    shuffle(neighbours.begin(), neighbours.end(), rng);
                    for(auto v: neighbours) {
                        if(comps[v] == -1) {
                            next_node->lists[tid].push_back(v);
                            comps[v] = comp;
                        }
                    }
                }
                else {
                    for(auto v: adj[u]) {
                        if(comps[v] == -1) {
                            next_node->lists[tid].push_back(v);
                            comps[v] = comp;
                        }
                    }
                }
                vis[u] = 1;
            }

            curr->done[work_on] = true;
            work_on++;
            if(work_on>=num_t) work_on -= num_t;
        }

        curr = curr->next;
    }
}

void cleanUp(int tid) {
    outer_list_node* curr = head;
    while(curr != nullptr) {
        curr->lists[tid].clear();
        curr = curr->next;
    }
}

pthread_barrier_t bar_bfs, bar_clean, bar, bar1;

// main thread calls this function
void scc(int tid) {
    int i, j;
    for(i=0; i<N; i++) {
        if(comps[i] == -1) {
            while(head != nullptr) {
                outer_list_node* temp = head;
                head = head->next;
                delete temp;
            }
            head = new outer_list_node;
            head->lists.resize(num_t);
            head->done.resize(num_t, 0);
            head->lists[0].push_back(i);
            comps[i] = comp;

            pthread_barrier_wait(&bar_bfs);

            wf_bfs(tid);

            pthread_barrier_wait(&bar_clean);

            cleanUp(tid);

            pthread_barrier_wait(&bar);

            comp++;
        }
    }

    while(head != nullptr) {
        outer_list_node* temp = head;
        head = head->next;
        delete temp;
    }
    head = nullptr;
    pthread_barrier_wait(&bar_bfs);
    comp = -1;
    pthread_barrier_wait(&bar_clean);
    pthread_barrier_wait(&bar);
}

// Other threads call this function
void scc_helper(int tid) {
    while(comp != -1) {
        pthread_barrier_wait(&bar_bfs);

        wf_bfs(tid);

        pthread_barrier_wait(&bar_clean);

        cleanUp(tid);

        pthread_barrier_wait(&bar);
    }
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int i, j;
    
    FILE* f_in = fopen(argv[1], "r");

    fscanf(f_in, "%d %d", &N, &M);

    adj.resize(N);
    comps.resize(N, -1);
    vis.resize(N, 0);

    for(i=0; i<M; i++) {
        int x, y;
        fscanf(f_in, "%d %d", &x, &y);
        // x--; y--; /////////////////////////////////////////////
        if(x>=N || y>=N) continue;
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);

    cin >> num_t;

    // bar = std::barrier(num_t);
    pthread_barrier_init(&bar, NULL, num_t);
    pthread_barrier_init(&bar1, NULL, num_t);
    pthread_barrier_init(&bar_bfs, NULL, num_t);
    pthread_barrier_init(&bar_clean, NULL, num_t);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    vector<thread> th;
    th.push_back(thread(scc, 0));
    for(i=1; i<num_t; i++) {
        th.push_back(thread(scc_helper, i));
    }

    for(i=0; i<num_t; i++) {
        th[i].join();
    }

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    FILE* f_out = fopen("par-out.txt", "w");

    for(auto c: comps) {
        fprintf(f_out, "%d\n", c);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(comps.begin(), comps.end()));

    fclose(f_out);

    cout << duration << "\n";
    
    return 0;
}