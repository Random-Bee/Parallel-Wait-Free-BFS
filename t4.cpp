#include <bits/stdc++.h>
using namespace std;
using namespace chrono;
typedef long long ll;
typedef unsigned long long ull;
typedef long double ld;
#define nl "\n"
#define maxe(a) (*max_element(a.begin(), a.end()))
#define mine(a) (*min_element(a.begin(), a.end()))
#define all(a) a.begin(), a.end()
#define lsb(n) (n & -n)
#define msb(n) (1LL << (63 - __builtin_clzll(n)))
#define nsb(n) (__builtin_popcountll(n))
#define len(n) (to_string(n).size())

// vector<ll> v;

class node{
    public:
    int data;
    node* next = nullptr;
};

class linkedList{
    public:
    node* head = nullptr;
    node* tail = nullptr;
    // atomic<node*> head = nullptr;
    // atomic<node*> tail = nullptr;

    void push_back(int v) {
        node* new_node = new node;
        new_node->data = v;
        if(head == nullptr) {
            head = new_node;
            tail = new_node;
        }
        else {
            tail->next = new_node;
            // tail.load()->next = new_node;
            tail = new_node;
        }
    }

    void clear() {
        node* curr = head;
        while(curr != nullptr) {
            node* temp = curr;
            curr = curr->next;
            delete temp;
        }
        head = nullptr;
        tail = nullptr;
    }
};

// linkedList v;

vector<int> v;

void work1() {
    for(int i=0; i<=1e6; i++) {
        v.push_back(i);
    }
}

void work2() {
    // vector<int> arr = v;
    // while(arr.back() != 1e6) {
    //     arr = v;
    // }

    // node* curr = v.head;
    // while(curr != nullptr && curr->data != 1e6) {
    //     curr = curr->next;
    // }

    vector<int> arr = {0};
    while(arr.back() != 1e6) {
        int sz = v.size();
        arr = vector<int>(sz);
        for(int i=0; i<sz; i++) {
            arr[i] = v[i];
        }
    }
}

void work() {
    int N = 1e8;
    // vector<int> v;
    // for(int i=0; i<N; i++) {
    //     v.push_back(i);
    // }

    // int* arr = new int[N];
    // for(int i=0; i<N; i++) {
    //     arr[i] = i;
    // }
    // delete[] arr;

    // int arr[N];
    // for(int i=0; i<N; i++) {
    //     arr[i] = i;
    // }

    // vector<int> v;
    // v.resize(N);
    // for(int i=0; i<N; i++) {
    //     v[i] = i;
    // }

    // Vector to array
    // vector<int> v;
    // v.resize(N, 0);
    // for(int i=0; i<N; i++) {
    //     v[i] = i;
    // }
    // int* arr = new int[N];
    // for(int i=0; i<N; i++) {
    //     arr[i] = v[i];
    // }
    // delete[] arr;

    int *arr = (int*)malloc(N*sizeof(int));
    for(int i=0; i<N; i++) {
        arr[i] = i;
    }
    free(arr);
}

int main() {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    
    work();

    // thread th1(work1);
    // // usleep(100000);
    // thread th2(work2);
    // th1.join(); th2.join();

    // v.clear();

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    cout << duration << nl;

    return 0;
}