#include <bits/stdc++.h>
#include <semaphore.h>
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
vector<int> dist, vis;

int num_t;

class outer_list_node {
    public:
    vector<vector<int>> lists;
    vector<bool> done;
    atomic<outer_list_node*> next = nullptr;
};

int threshold = 10;

int failure = 5, sleep_time;

int completed = 0;

void wf_bfs(outer_list_node* head, int tid, bool fail) {
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    exponential_distribution<double> expRandObj(1.0/sleep_time);
    outer_list_node* curr = head;

    while(curr != nullptr) {
        int null_count = 0, work_on = tid;
        bool next_is_null = true;

        while(null_count != num_t) {
            if(curr->done[work_on] || curr->lists[work_on].empty()) {
                null_count++;
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

            vector<int> list = curr->lists[work_on];

            bool fl = 1;
            if(list.size()>=threshold && work_on != tid) {
                shuffle(list.begin(), list.end(), rng);
                fl = 0;
            }

            if(fail) {
                double st = expRandObj(rng);
                usleep(st*1000);
            }

            for(auto u: list) {
                if(vis[u]) continue;
                if(fl && work_on!=tid) {
                    vector<int> neighbours = adj[u];
                    shuffle(neighbours.begin(), neighbours.end(), rng);
                    for(auto v: neighbours) {
                        if(dist[v] == -1) {
                            next_node->lists[tid].push_back(v);
                            dist[v] = dist[u]+1;
                        }
                    }
                }
                else {
                    for(auto v: adj[u]) {
                        if(dist[v] == -1) {
                            next_node->lists[tid].push_back(v);
                            dist[v] = dist[u]+1;
                        }
                    }
                }
                vis[u] = 1;
            }

            curr->done[work_on] = true;
            null_count++;
            work_on++;
            if(work_on>=num_t) work_on -= num_t;
        }

        curr = curr->next;
    }
    completed = 1;
}

high_resolution_clock::time_point t1, t2;

void work() {
    while(completed == 0);
    t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();
    cout << duration << "\n";
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
        // x--; y--; /////////////////////////////////////////////
        if(x>=N || y>=N) continue;
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);
    
    cin >> num_t >> sleep_time;

    outer_list_node* head = new outer_list_node;
    head->lists.resize(num_t);
    head->done.resize(num_t, 0);
    head->lists[0].push_back(0);
    dist[0] = 0;

    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    vector<bool> fails(num_t, 0);
    for(i=0; i<failure; i++) fails[i] = 1;
    shuffle(fails.begin(), fails.end(), rng);

    t1 = high_resolution_clock::now();
    
    vector<thread> th;
    for(i=0; i<num_t; i++) {
        th.push_back(thread(wf_bfs, head, i, fails[i]));
    }
    thread tcheck(work);
    for(i=0; i<num_t; i++) {
        th[i].join();
    }
    tcheck.join();

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

    return 0;
}