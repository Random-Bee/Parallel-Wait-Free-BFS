#include <bits/stdc++.h>
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
vector<int> comps;

void bfs(int src, int comp) {
    queue<int> q;
    comps[src] = comp;
    q.push(src);
    while(!q.empty()) {
        int u = q.front();
        q.pop();
        for(auto v: adj[u]) {
            if(comps[v] == -1) {
                comps[v] = comp;
                q.push(v);
            }
        }
    }
}

void scc() {
    int i, j;
    int comp = 0;
    for(i=0; i<N; i++) {
        if(comps[i] == -1) {
            bfs(i, comp);
            comp++;
        }
    }
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int i, j;
    
    FILE* f_in = fopen(argv[1], "r");

    fscanf(f_in, "%d %d", &N, &M);

    adj.resize(N);
    comps.resize(N, -1);

    for(i=0; i<M; i++) {
        int x, y;
        fscanf(f_in, "%d %d", &x, &y);
        // x--; y--; /////////////////////////////////////////////
        if(x>=N || y>=N) continue;
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    scc();

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    FILE* f_out = fopen("seq-out.txt", "w");

    for(auto c: comps) {
        fprintf(f_out, "%d\n", c);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(comps.begin(), comps.end()));

    fclose(f_out);

    cout << duration << "\n";
    
    return 0;
}