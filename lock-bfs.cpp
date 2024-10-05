#include <bits/stdc++.h>
using namespace std;
using namespace chrono;
#include <semaphore.h>

int N, M;
vector<vector<int>> adj;
vector<int> dist;

int num_t;

vector<int> ops;
queue<int> q;

sem_t mtx;

void lock_bfs(int tid) {
    int dpt = 0;
    while(1) {
        sem_wait(&mtx);
        if(q.empty()) {
            sem_post(&mtx);
            break;
        }
        int u = q.front(), d = dist[u];
        q.pop();
        sem_post(&mtx);
        for(auto v: adj[u]) {
            ops[tid]++;
            if(dist[v] == -1) {
                dist[v] = d + 1;
                sem_wait(&mtx);
                q.push(v);
                sem_post(&mtx);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int i, j;
    
    FILE* f_in = fopen(argv[1], "r");

    fscanf(f_in, "%d %d %d", &N, &M, &num_t);

    adj.resize(N);
    dist.resize(N, -1);
    ops.resize(num_t, 0);

    for(i=0; i<M; i++) {
        int x, y;
        fscanf(f_in, "%d %d", &x, &y);
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);

    dist[0] = 0;
    q.push(0);

    sem_init(&mtx,0,1);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    vector<thread> th;
    for(i=0; i<num_t; i++) {
        th.push_back(thread(lock_bfs, i));
    }
    for(i=0; i<num_t; i++) {
        th[i].join();
    }

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    FILE* f_out = fopen("lock-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);

    for(auto x: ops) cout << x << " "; cout << "\n";
    cout << duration << "\n";
    
    return 0;
}