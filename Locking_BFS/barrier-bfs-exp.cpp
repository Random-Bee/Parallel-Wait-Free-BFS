#include <bits/stdc++.h>
#include <x86intrin.h>
#include <semaphore.h>
#include <omp.h>
using namespace std;
using namespace chrono;

int N, M;
vector<vector<int>> adj;
vector<int> dist, vis;

int num_t;

unsigned long long max_waiting_time = 0;
vector<unsigned long long> tot_waiting_time(100, 0), tot_work_time(100, 0);

void BFS_parallel_frontier_expansion(int N, int src, long diameter, const vector<vector<int>>& adj) {

    long* S;
    long *start;
    long *pSCount;
    omp_lock_t* vLock;

    omp_set_num_threads(num_t);

    long phase_num, numPhases;

    #pragma omp parallel
    {
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

        long long tot_wait = 0, tot_work = 0;

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

        high_resolution_clock::time_point t1, t2;
        auto duration = duration_cast<microseconds>(t2 - t1).count();

        // t1 = high_resolution_clock::now();

        #pragma omp barrier

        // t2 = high_resolution_clock::now();
        // duration = duration_cast<microseconds>(t2 - t1).count();
        // tot_wait += duration;

        #pragma omp for
        for (i=0; i<n; i++) {
            omp_init_lock(&vLock[i]);
        }

        // t1 = high_resolution_clock::now();

        #pragma omp barrier

        // t2 = high_resolution_clock::now();
        // duration = duration_cast<microseconds>(t2 - t1).count();
        // tot_wait += duration;

        if (tid == 0) {
            S[0] = src;
            vis[src] = 1;
            dist[src] = 0;
            phase_num = 0;
            start[0] = 0;
            start[1] = 1;
        }

        // t1 = high_resolution_clock::now();

        #pragma omp barrier

        // t2 = high_resolution_clock::now();
        // duration = duration_cast<microseconds>(t2 - t1).count();
        // tot_wait += duration;

        while (start[phase_num+1] - start[phase_num] > 0) {

            pCount = 0;

            start_iter = start[phase_num];
            end_iter = start[phase_num+1];

            // t1 = high_resolution_clock::now();
            #pragma omp for
            for (vert=start_iter; vert<end_iter; vert++) {
                v = S[vert];
                for (j=0; j<adj[v].size(); j++) {
                    w = adj[v][j];
                    if (v == w)
                        continue;

                    // unsigned long long start = __rdtsc();

                    myLock = omp_test_lock(&vLock[w]);

                    // unsigned long long end = __rdtsc();

                    // tot_wait += end - start;
                    
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

            // t2 = high_resolution_clock::now();
            // duration = duration_cast<microseconds>(t2 - t1).count();
            // tot_work += duration;

            // t1 = high_resolution_clock::now();

            #pragma omp barrier
            
            // t2 = high_resolution_clock::now();
            // duration = duration_cast<microseconds>(t2 - t1).count();
            // tot_wait += duration;
            
            pSCount[tid+1] = pCount;

            // t1 = high_resolution_clock::now();

            #pragma omp barrier

            // t2 = high_resolution_clock::now();
            // duration = duration_cast<microseconds>(t2 - t1).count();
            // tot_wait += duration;

            if (tid == 0) {
                pSCount[0] = start[phase_num+1];
                for(k=1; k<=nthreads; k++) {
                    pSCount[k] = pSCount[k-1] + pSCount[k];
                }
                start[phase_num+2] = pSCount[nthreads];
                phase_num++;
            }

            // t1 = high_resolution_clock::now();

            #pragma omp barrier

            // t2 = high_resolution_clock::now();
            // duration = duration_cast<microseconds>(t2 - t1).count();
            // tot_wait += duration;

            for (k = pSCount[tid]; k < pSCount[tid+1]; k++) {
                S[k] = pS[k-pSCount[tid]];
            } 

            // t1 = high_resolution_clock::now();

            #pragma omp barrier

            // t2 = high_resolution_clock::now();
            // duration = duration_cast<microseconds>(t2 - t1).count();
            // tot_wait += duration;
        } /* End of search */


        free(pS);

        // t1 = high_resolution_clock::now();

        #pragma omp barrier

        // t2 = high_resolution_clock::now();
        // duration = duration_cast<microseconds>(t2 - t1).count();
        // tot_wait += duration;


        #pragma omp for
        for (i=0; i<n; i++) {
            omp_destroy_lock(&vLock[i]);
        }

        // t1 = high_resolution_clock::now();

        #pragma omp barrier

        // t2 = high_resolution_clock::now();
        // duration = duration_cast<microseconds>(t2 - t1).count();
        // tot_wait += duration;

        if (tid == 0) {
            free(S);
            free(start);
            free(pSCount);
            free(vLock);
        }

        tot_waiting_time[tid] = tot_wait;
        tot_work_time[tid] = tot_work;
    }
}

void getStats() {
    for(int i=0; i<num_t; i++) {
        max_waiting_time = max(max_waiting_time, tot_waiting_time[i]);
    }
    cout << "Max waiting time: " << max_waiting_time << "\n";
    double tot = 0;
    for(int i=0; i<num_t; i++) {
        tot += tot_waiting_time[i];
    }
    cout << "Avg waiting time: " << tot/num_t << "\n";
    tot = 0;
    for(int i=0; i<num_t; i++) {
        tot += tot_work_time[i];
    }
    cout << "Avg work time: " << tot/num_t << "\n";
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

    cin >> num_t;

    // high_resolution_clock::time_point t1 = high_resolution_clock::now();

    unsigned long long t1 = __rdtsc();

    BFS_parallel_frontier_expansion(N, 0, N, adj);

    // high_resolution_clock::time_point t2 = high_resolution_clock::now();
    // auto duration = duration_cast<microseconds>(t2 - t1).count();

    unsigned long long t2 = __rdtsc();
    unsigned long long duration = t2 - t1;

    FILE* f_out = fopen("bar-out.txt", "w");

    for(auto d: dist) {
        fprintf(f_out, "%d\n", d);
    }
    fprintf(f_out, "\n");
    fprintf(f_out, "%d\n", *max_element(dist.begin(), dist.end()));

    fclose(f_out);

    cout << duration << "\n";

    getStats();
    
    return 0;
}