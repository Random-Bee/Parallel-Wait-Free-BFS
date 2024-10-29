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

// vector<int> v;

class customList {
    public:
    int* list;
    int size = 0;
    int capacity = 1;
    int head;
    vector<int*> prevPointers;

    customList() {
        list = new int[capacity];
        head = 0;
    }

    void push_back(int x) {
        if(size == capacity) {
            capacity *= 2;
            int* new_list = new int[capacity];
            std::copy(list, list+size, new_list);
            prevPointers.push_back(list);
            list = new_list;
        }
        list[size++] = x;
    }

    void copy(int* arr, int sz) {
        int* ptr = list;
        std::copy(ptr, ptr+sz, arr);
    }

    ~customList() {
        delete[] list;
        for(auto p: prevPointers) {
            delete[] p;
        }
    }
};

class raiiArray {
    public:
    int* arr;
    raiiArray(int sz) {
        arr = new int[sz];
    }
    ~raiiArray() {
        // cout << "Deleted\n";
        delete[] arr;
    }
};

customList v;

void work1() {
    for(int i=0; i<=1e8; i++) {
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

    // vector<int> arr = {0};
    // while(arr.back() != 1e6) {
    //     int sz = v.size();
    //     arr = vector<int>(sz);
    //     for(int i=0; i<sz; i++) {
    //         arr[i] = v[i];
    //     }
    // }

    // int* arr = new int[1]; arr[0] = 0;
    // int sz = 1;
    // while(arr[sz-1] != 1e8) {
    //     sz = v.size;
    //     delete[] arr;
    //     arr = new int[sz];
    //     v.copy(arr, sz);
    // }

    // raiiArray arr(1);
    // arr.arr[0] = 0;
    // int sz = 1;
    // // while(arr.arr[sz-1] != 1e8) {
    // for(int i=0; i<2; i++) {
    //     sz = v.size;
    //     arr.~raiiArray();
    //     new(&arr) raiiArray(sz);
    //     // v.copy(arr.arr, sz);
    // }

    for(int i=0; i<200; i++) {
        int sz = v.size;
        raiiArray arr(sz);
        v.copy(arr.arr, sz);
    }
}

void work() {
    int N = 1e8;
    // vector<int> v;
    // for(int i=0; i<N; i++) {
    //     v.push_back(i);
    // }

    int* arr = new int[N];
    for(int i=0; i<N; i++) {
        arr[i] = i;
    }
    delete[] arr;

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

    // int *arr = (int*)malloc(N*sizeof(int));
    // for(int i=0; i<N; i++) {
    //     arr[i] = i;
    // }
    // free(arr);

    // atomic<int>* arr = new atomic<int>[N];
    // for(int i=0; i<N; i++) {
    //     arr[i] = i;
    // }
    // delete[] arr;
}

atomic<int> x = 0;

void work3() {
    while(x < 1e8) {
        x++;
    }
}

int main() {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    
    // work();

    // thread th1(work1);
    // // usleep(100000);
    // thread th2(work2);
    // th1.join();
    // th2.join();

    // v.clear();

    thread th1(work3);
    // thread th2(work3);
    th1.join();
    // th2.join();

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(t2 - t1).count();

    cout << duration << nl;

    return 0;
}