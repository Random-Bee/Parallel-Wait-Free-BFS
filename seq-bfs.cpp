#include <bits/stdc++.h>
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
vector<int> dist;

int ops = 0;

void bfs() {
    queue<int> q;
    dist[0] = 0;
    q.push(0);
    while(!q.empty()) {
        int u = q.front(), d = dist[u];
        q.pop();
        for(auto v: adj[u]) {
            ops++;
            if(dist[v] == -1) {
                dist[v] = d + 1;
                q.push(v);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int i, j;
    
    FILE* f_in = fopen(argv[1], "r");

    fscanf(f_in, "%d %d", &N, &M);

    adj.resize(N);
    dist.resize(N, -1);

    for(i=0; i<M; i++) {
        int x, y;
        fscanf(f_in, "%d %d", &x, &y);
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    bfs();

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    FILE* f_out = fopen("seq-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);

    cout << ops << "\n";
    cout << duration << "\n";
    
    return 0;
}