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

void met1() {
    ll c = 0, n, d, t;
    cin >> n >> d;
    vector<pair<ll,ll>> arr;
    vector<vector<ll>> brr(d+1);
    brr[0] = {0};
    for(int i=1; i<=d; i++) {
        for(auto j: brr[i-1]) {
            for(int k=0; k<n; k++) {
                c++;
                arr.push_back({j, c});
                brr[i].push_back(c);
            }
        }
    }
    cout << c+1 << " " << arr.size() << nl;
    for(auto p: arr) {
        cout << p.first << " " << p.second << nl;
    }
}

void met2() {
    ll n, m, i, j;
    cin >> n >> m;
    cout << n << " " << m << nl;
    for(i=0; i<m; i++) {
        ll x = random(0, n-1), y = random(0, n-1);
        cout << x << " " << y << nl;
    }
}

int main() {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    met1();
    // met2();
    return 0;
}