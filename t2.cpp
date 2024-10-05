#pragma GCC optimize("O3,unroll-loops")
#include <bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
using namespace std;
using namespace __gnu_pbds;
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

int a = 0;

void work() {
    for(int i=0; i<100; i++) {
        a = i;
    }
}

int main() {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    ll c = 0, n = 100;
    vector<pair<ll,ll>> arr;
    for(int i=1; i<=n; i++) {
        c++;
        // cout << 0 << " " << c << "\n";
        arr.push_back({0,c});
    }
    for(int i=1; i<=n; i++) {
        for(int j=0; j<n; j++) {
            c++;
            // cout << i << " " << c << nl;
            arr.push_back({i,c});
        }
    }
    cout << c+1 << " " << arr.size() << " " << 1 << nl;
    for(auto p: arr) {
        cout << p.first << " " << p.second << nl;
    }
    return 0;
}