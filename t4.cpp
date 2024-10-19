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

int main() {
    ios_base::sync_with_stdio(false); cin.tie(0); cout.tie(0);
    cout << thread::hardware_concurrency() << nl;
    return 0;
}