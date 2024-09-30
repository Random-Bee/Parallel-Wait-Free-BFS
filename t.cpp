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

mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
ll random(ll a, ll b) {
    return uniform_int_distribution<ll>(a, b)(rng);
}

void solve() {
    ll n = 1e4, i, j, k, m = 100, t = 16;
    ld tot = 0;
    ll mx = 0;
    for(i=0; i<n; i++) {
        vector<vector<ll>> pers(t, vector<ll>(m));
        for(j=0; j<t; j++) {
            iota(all(pers[j]), 0);
            shuffle(all(pers[j]), rng);
        }
        vector<ll> vis(m, 0), used;
        for(j=0; j<m; j++) {
            for(k=0; k<t; k++) {
                if(!vis[pers[k][j]]) {
                    used.push_back(pers[k][j]);
                }
            }
            for(k=0; k<t; k++) {
                vis[pers[k][j]] = 1;
            }
        }
        tot += used.size();
        mx = max(mx, (ll)used.size());
    }
    tot /= n;
    cout << tot << " " << mx << nl;
}

int main() {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    int t = 1;
    cin >> t;
    for (int i = 0; i < t; i++) {
        solve();
    }
    return 0;
}