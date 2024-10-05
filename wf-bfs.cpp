#include <bits/stdc++.h>
#include <semaphore.h>
using namespace std;
using namespace chrono;

mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

int N, M;
vector<vector<int>> adj;
vector<int> dist;

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
    vector<inner_list_node*> lists, completed_lists;
    atomic<outer_list_node*> next = nullptr;

    void add_inner_list_node(int val, int id) {
        inner_list_node* node = new inner_list_node;
        node->data = val;
        node->next = lists[id];
        lists[id] = node;
    }

    void print(int id) {
        sem_wait(&mtx);
        inner_list_node* tmp = lists[id];
        cout << "T" << id << " : ";
        while(tmp!=nullptr) {
            cout << tmp->data << " ";
            tmp = tmp->next;
        }
        cout << "\n";
        sem_post(&mtx);
    }
};

vector<int> ops;

void wf_bfs(outer_list_node* head, int tid) {
    outer_list_node* curr = head;
    int dpt = 0;
    while(1) {
        int null_count = 0, work_on = tid;
        bool next_is_null = true;
        while(null_count != num_t) {
            inner_list_node* head = curr->lists[work_on];
            inner_list_node* temp = head;

            if(head == nullptr) {
                null_count++;
                work_on = (work_on+1)%num_t;
                continue;
            }

            if(next_is_null && curr->next == nullptr) {
                outer_list_node* new_node = new outer_list_node;
                new_node->lists.resize(num_t, nullptr);
                new_node->completed_lists.resize(num_t, nullptr);

                // ops[tid] += num_t;

                outer_list_node* expected = nullptr;
                if(!atomic_compare_exchange_strong(&(curr->next), &expected, new_node)) {
                    delete new_node;
                }
            }
            next_is_null = false;

            while(temp!=nullptr) {
                if(!temp->done) {
                    vector<int> neighbours = adj[temp->data];
                    shuffle(neighbours.begin(), neighbours.end(), rng);
                    for(auto v: neighbours) {
                        ops[tid]++;
                        if(dist[v] == -1) {
                            curr->next.load()->add_inner_list_node(v, tid);
                            dist[v] = dist[temp->data] + 1;
                        }
                    }
                    temp->done = 1;
                }
                temp = temp->next;
            }

            curr->completed_lists[work_on] = head;
            curr->lists[work_on] = nullptr;
            null_count++;
            work_on = (work_on+1)%num_t;
        }

        if(curr->next != nullptr) {
            curr = curr->next;
        }
        else break;
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
    
    outer_list_node* head = new outer_list_node;
    head->lists.resize(num_t, nullptr);
    head->completed_lists.resize(num_t, nullptr);
    head->add_inner_list_node(0, 0);
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
        for(i=0; i<num_t; i++) {
            inner_list_node* inner_head = temp->completed_lists[i];
            while(inner_head != nullptr) {
                inner_list_node* itmp = inner_head;
                inner_head = inner_head->next;
                delete itmp;
            }
        }
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

    return 0;
}