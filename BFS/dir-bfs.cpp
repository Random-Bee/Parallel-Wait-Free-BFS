#include <bits/stdc++.h>
#include <semaphore.h>
#include <omp.h>
using namespace std;
using namespace chrono;

#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")

int N, M;
vector<vector<int>> adj;
vector<int> dist;

int num_t;

double mf, mu, nf, old_nf;

bool topDown(int &level, int nov) {
	cout << "Top Down at level " << level << " with mf = " << mf << ", mu = " << mu << "\n";
	bool improvement = false;

	#pragma omp parallel
	{
		#pragma omp for reduction(+:mf) reduction(||:improvement) schedule(guided, 32)
		for (int i = 0; i < nov; i++) {
			if (dist[i] == level) {
				for (int j = 0; j < adj[i].size(); j++) {
					int u = adj[i][j];
					if (dist[u] < 0) {
						mf += adj[u].size();
						dist[u] = level + 1;
						improvement = true;
					}
				}
			}
		}
	}

	if (improvement) {
		level++;
	}
	return improvement;
}

bool bottomUp(int &level, int nov, int *unvisited, int uvSize) {
	bool improvement = false;
	nf = 0;

	#pragma omp parallel for reduction(+:nf) reduction(||:improvement) schedule(guided, 32)
	for (int i = 0; i < uvSize; i++) {
		int v = unvisited[i];
		if (dist[v] < 0) {
			for (int j = 0; j < adj[v].size(); j++) {
				int u = adj[v][j];
				if (dist[u] == level) {
					dist[v] = level + 1;
					nf++;
					improvement = true;
					break;
				}
			}
		}
	}

	if (improvement) {
		level++;
	}
	return improvement;
}

void dir_opt_bfs(int src) {
	omp_set_num_threads(num_t);

	mf = adj[src].size(); //number of edges on frontier
	mu = 2*M;  // total number of edges
	dist[src] = 0;

	double alpha = 14.0;
	double beta = 24.0;

	int level = 0;
	bool improvement = true;

	int *unvisited = new int[N];
	int uvSize = 0;

	while (improvement) {
		if (mf > mu / alpha) {
			uvSize = 0;
			for (int i = 0; i < N; i++) {
				if (dist[i] < 0) {
					unvisited[uvSize++] = i;
				}
				else if (dist[i] == level) {
					nf++;
				}
			}

			do {
				old_nf = nf;
				improvement = bottomUp(level, N, unvisited, uvSize);
			} while (improvement && ((mf > mu / alpha) || (nf >= old_nf || nf > double(N) / beta)));
		}

		else {
			mu -= mf;
			improvement = topDown(level, N);
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
        // x--; y--; ///////////////////////////////////////////// for sina weibo
        // x-=101; y-=101; ///////////////////////////////////////////// for friendster
        if(x>=N || y>=N) continue;
        adj[x].push_back(y);
        adj[y].push_back(x);
    }
    
    fclose(f_in);

    cin >> num_t;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    dir_opt_bfs(0);

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    FILE* f_out = fopen("dir-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);

    cout << duration << "\n";
    
    return 0;
}
