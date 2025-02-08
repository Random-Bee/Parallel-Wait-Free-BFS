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

vector<vector<ll>> arr;

void work(ll n, ll id) {
    for(ll i=0; i<n; i++) {
        arr[id].push_back(i);
    }
}

int main() {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    ll c = 0, n, t, i;
    cin >> n >> t;
    arr.resize(t);
    vector<thread> th;
    for(i=0; i<t; i++) {
        th.push_back(thread(work, n, i));
    }
    for(i=0; i<t; i++) {
        th[i].join();
    }
    for(auto v: arr) {
        cout << v.size() << " ";
    }
    cout << nl;
    return 0;
}