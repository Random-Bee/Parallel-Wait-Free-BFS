#include <bits/stdc++.h>
#include <semaphore.h>
#include <omp.h>
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
vector<int> dist, vis;

int num_t;

// void BFS_parallel_frontier_expansion(int N, int src, long diameter, const vector<vector<int>>& adj) {

//     vector<int> S;  // Frontier list
//     vector<long> start;  // Start of each BFS phase
//     vector<long> pSCount;  // Per-thread vertex count

//     omp_set_num_threads(num_t);

//     vector<omp_lock_t> vLock(N);  // Locks for each vertex

//     long phase_num, numPhases;

//     #pragma omp parallel
//     {
//         vector<int> pS;  // Per-thread frontier
//         long pCount = 0, pS_size;
//         long v, w;
//         int tid, nthreads;
//         long start_iter, end_iter;    
//         long vert, n;
//         int myLock;

//         tid = omp_get_thread_num();
//         nthreads = omp_get_num_threads();


//         if (tid == 0)  
//             numPhases = diameter + 1;
//         n = N;

//         pS_size = n / nthreads + 1;
//         pS.resize(pS_size);

//         if (tid == 0) {  
//             S.resize(n);
//             start.resize(numPhases + 2, 0);
//             pSCount.resize(nthreads + 1, 0);
//             for (int i = 0; i < N; ++i) {
//                 omp_init_lock(&vLock[i]);
//             }
//         }

//         #pragma omp barrier

//         if (tid == 0) {
//             S[0] = src;
//             vis[src] = 1;
//             dist[src] = 0;
//             phase_num = 0;
//             start[0] = 0;
//             start[1] = 1;
//         }

//         #pragma omp barrier

//         while (start[phase_num + 1] - start[phase_num] > 0) {

//             pCount = 0;

//             start_iter = start[phase_num];
//             end_iter = start[phase_num + 1];
//             #pragma omp for schedule(dynamic)
//             for (vert = start_iter; vert < end_iter; vert++) {

//                 v = S[vert];
//                 for (int neighbor : adj[v]) {
//                     w = neighbor;
//                     if (v == w)
//                         continue;
//                     myLock = omp_test_lock(&vLock[w]);
//                     if (myLock) {
//                         if (!vis[w]) {
//                             vis[w] = 1;
//                             dist[w] = dist[v] + 1;
//                             if (pCount == pS_size) {
//                                 /* Resize pS */
//                                 pS_size *= 2;
//                                 pS.resize(pS_size);
//                             }
//                             pS[pCount++] = w;
//                         }
//                         omp_unset_lock(&vLock[w]);
//                     }
//                 }
//             }


//             #pragma omp barrier
            
//             pSCount[tid + 1] = pCount;

//             #pragma omp barrier

//             if (tid == 0) {
//                 pSCount[0] = start[phase_num + 1];
//                 for (int k = 1; k <= nthreads; ++k) {
//                     pSCount[k] = pSCount[k - 1] + pSCount[k];
//                 }
//                 start[phase_num + 2] = pSCount[nthreads];
//                 phase_num++;
//             }

//             #pragma omp barrier

//             for (long k = pSCount[tid]; k < pSCount[tid + 1]; ++k) {
//                 S[k] = pS[k - pSCount[tid]];
//             }

//             #pragma omp barrier
//         }  /* End of search */
 
//         #pragma omp barrier
//         #pragma omp for schedule(static)
//         for (int i = 0; i < N; ++i) {
//             omp_destroy_lock(&vLock[i]);
//         }
//         #pragma omp barrier

//     }
// }


int failure = 5, sleep_time;

void BFS_parallel_frontier_expansion(int N, int src, long diameter, const vector<vector<int>>& adj, vector<bool> fail) {

    long* S;
    long *start;
    long *pSCount;
    omp_lock_t* vLock;

    omp_set_num_threads(num_t);

    long phase_num, numPhases;

    #pragma omp parallel
    {
        mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
        exponential_distribution<double> expRandObj(1.0/sleep_time);

        long *pS, *pSt;
        long pCount, pS_size;
        long v, w;
        int tid, nthreads;
        long start_iter, end_iter;    
        long j, k, vert, n;
        int myLock;
 
        long i;
        tid = omp_get_thread_num();
        nthreads = omp_get_num_threads();

        if (tid == 0)  
            numPhases = diameter + 1;
        n = N;

        pS_size = n/nthreads + 1;
        pS = (long *) malloc(pS_size*sizeof(long));
        assert(pS != NULL);

        if (tid == 0) {  
            S = (long *) malloc(n*sizeof(long));
            start = (long *) calloc((numPhases+2), sizeof(long));
            pSCount = (long *) malloc((nthreads+1)*sizeof(long));
            vLock = (omp_lock_t *) malloc(n*sizeof(omp_lock_t));
        }

        #pragma omp barrier

        #pragma omp for
        for (i=0; i<n; i++) {
            omp_init_lock(&vLock[i]);
        }

        #pragma omp barrier

        if (tid == 0) {
            S[0] = src;
            vis[src] = 1;
            dist[src] = 0;
            phase_num = 0;
            start[0] = 0;
            start[1] = 1;
        }

        #pragma omp barrier

        while (start[phase_num+1] - start[phase_num] > 0) {
            
            if(fail[tid]) {
                double st = expRandObj(rng);
                usleep(st*1000);
            }

            pCount = 0;

            start_iter = start[phase_num];
            end_iter = start[phase_num+1];
            #pragma omp for
            for (vert=start_iter; vert<end_iter; vert++) {

                v = S[vert];
                // for (j=G->numEdges[v]; j<G->numEdges[v+1]; j++) {
                for (j=0; j<adj[v].size(); j++) {
                    // w = G->endV[j]; 
                    w = adj[v][j];
                    if (v == w)
                        continue;
                    myLock = omp_test_lock(&vLock[w]);
                    if (myLock) {
                        if (vis[w] != 1) { 
                            vis[w] = 1;
                            dist[w] = dist[v] + 1;
                            if (pCount == pS_size) {
                                /* Resize pS */
                                pSt = (long *) malloc(2*pS_size*sizeof(long));
                                memcpy(pSt, pS, pS_size*sizeof(long));
                                free(pS);
                                pS = pSt;
                                pS_size = 2*pS_size;
                            }
                            pS[pCount++] = w;
                        }
                        omp_unset_lock(&vLock[w]);
                    }
                }
            }

            #pragma omp barrier
            pSCount[tid+1] = pCount;

            #pragma omp barrier

            if (tid == 0) {
                pSCount[0] = start[phase_num+1];
                for(k=1; k<=nthreads; k++) {
                    pSCount[k] = pSCount[k-1] + pSCount[k];
                }
                start[phase_num+2] = pSCount[nthreads];
                phase_num++;
            }

            #pragma omp barrier
            for (k = pSCount[tid]; k < pSCount[tid+1]; k++) {
                S[k] = pS[k-pSCount[tid]];
            } 

            #pragma omp barrier
        } /* End of search */


        free(pS);
        #pragma omp barrier
        #pragma omp for
        for (i=0; i<n; i++) {
            omp_destroy_lock(&vLock[i]);
        }
        #pragma omp barrier

        if (tid == 0) {
            free(S);
            free(start);
            free(pSCount);
            free(vLock);

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

    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    vector<bool> fails(num_t, 0);
    for(i=0; i<failure; i++) fails[i] = 1;
    shuffle(fails.begin(), fails.end(), rng);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    BFS_parallel_frontier_expansion(N, 0, N, adj, fails);

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    FILE* f_out = fopen("bar-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);

    cout << duration << "\n";
    
    return 0;
}