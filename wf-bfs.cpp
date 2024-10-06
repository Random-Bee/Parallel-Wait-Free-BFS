#include <bits/stdc++.h>
#include <semaphore.h>
using namespace std;
using namespace chrono;


int N, M;
vector<vector<int>> adj;
vector<int> dist, vis;

int num_t;

class inner_list_node {
    public:
    int data;
    int done = 0;
    inner_list_node* next = nullptr;
};

sem_t mtx;

class outer_list_node {
    public:
    vector<vector<int>> lists;
    vector<bool> done;
    atomic<outer_list_node*> next = nullptr;

    void print(int id) {
        sem_wait(&mtx);
        cout << "T" << id << " : ";
        for(auto x: lists[id]) cout << x << " ";
        cout << "\n";
        sem_post(&mtx);
    }
};

vector<int> ops;

vector<vector<int>> enc;

atomic<int> v1 = 0;

int th = 10;

void wf_bfs(outer_list_node* head, int tid) {
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    outer_list_node* curr = head;
    int dpt = 0;
    while(curr != nullptr) {
        int null_count = 0, work_on = tid;
        bool next_is_null = true;

        while(null_count != num_t) {
            if(curr->done[work_on]) {
                null_count++;
                work_on = (work_on+1)%num_t;
                continue;
            }

            vector<int> list = curr->lists[work_on];
            // ops[tid] += list.size();

            if(list.empty()) {
                null_count++;
                curr->done[work_on] = true;
                work_on++;
                if(work_on>=num_t) work_on -= num_t;
                continue;
            }

            if(next_is_null && curr->next == nullptr) {
                // v1++;
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

            bool fl = 1;
            if(list.size()>=th && work_on != tid) {
            // if(work_on != tid) {
                shuffle(list.begin(), list.end(), rng);
                fl = 0;
            }
            // ops[tid] += list.size();

            for(auto u: list) {
                if(vis[u]) continue;
                if(fl && work_on!=tid) {
                    vector<int> neighbours = adj[u];
                    shuffle(neighbours.begin(), neighbours.end(), rng);
                    // ops[tid] += neighbours.size();
                    for(auto v: neighbours) {
                    // for(auto v: adj[u]) {
                        // ops[tid]++;
                        // enc[tid].push_back(v);
                        if(dist[v] == -1) {
                            next_node->lists[tid].push_back(v);
                            dist[v] = dist[u]+1;
                        }
                    }
                }
                else {
                    for(auto v: adj[u]) {
                        // enc[tid].push_back(v);
                        if(dist[v] == -1) {
                            next_node->lists[tid].push_back(v);
                            dist[v] = dist[u]+1;
                        }
                    }
                }
                vis[u] = 1;
            }
            null_count++;
            curr->done[work_on] = true;
            work_on++;
            if(work_on>=num_t) work_on -= num_t;
        }

        // curr->print(tid);
        // usleep(10000);
    }
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int i, j;
    
    FILE* f_in = fopen(argv[1], "r");

    cin >> num_t;

    fscanf(f_in, "%d %d", &N, &M);

    adj.resize(N);
    dist.resize(N, -1);
    vis.resize(N, 0);
    ops.resize(num_t, 0);
    enc.resize(num_t);

    for(i=0; i<M; i++) {
        int x, y;
        fscanf(f_in, "%d %d", &x, &y);
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);
    
    outer_list_node* head = new outer_list_node;
    head->lists.resize(num_t);
    head->done.resize(num_t, 0);
    head->lists[0].push_back(0);
    dist[0] = 0;

    sem_init(&mtx,0,1);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    
    vector<thread> th;
    for(i=0; i<num_t; i++) {
        th.push_back(thread(wf_bfs, head, i));
    }
    for(i=0; i<num_t; i++) {
        th[i].join();
    }

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    while(head != nullptr) {
        outer_list_node* temp = head;
        head = head->next;
        delete temp;
    }

    FILE* f_out = fopen("wf-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);
    
    for(auto x: ops) cout << x << " "; cout << "\n";
    cout << duration << "\n";
    cout << v1 << "\n";

    // for(i=0; i<num_t; i++) {
    //     cout << "T" << i << " : ";
    //     for(auto x: enc[i]) cout << x << " "; cout << "\n";
    // }

    return 0;
}