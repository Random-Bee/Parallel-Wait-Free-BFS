#include <bits/stdc++.h>
using namespace std;
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

class customList {
    public:
    int* list;
    int size = 0;
    int capacity = 1;
    int head = 0;
    vector<int*> prevPointers;

    customList() {
        cout << "customList constructor called\n";
        list = new int[capacity];
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

    // void clear() {
    //     cout << "customList clear called\n";
    //     delete[] list;
    //     for(auto p: prevPointers) {
    //         delete[] p;
    //     }
    // }

    ~customList() {
        cout << "customList destructor called\n";
        delete[] list;
        for(auto p: prevPointers) {
            delete[] p;
        }
    }
};

class outer_list_node {
    public:
    vector<customList> lists;
    vector<bool> done;
    atomic<outer_list_node*> next = nullptr;
};

void solve() {
    int i;
    outer_list_node* head = new outer_list_node;
    head->lists.resize(4);
    head->done.resize(4, 0);
    delete head;
    head = nullptr;
    delete head;
}

int main() {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int t = 1;
    // cin >> t;
    for (int i = 0; i < t; i++) {
        solve();
    }
    return 0;
}