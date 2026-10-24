#pragma once
namespace reasitic {
constexpr unsigned green_table_budget_bytes=128u*1024u*1024u;
// Admitted dimensions bound each index product below 2^53, so binary64
// represents the exact product without the original signed-32-bit wrap.
inline double potential_index_product(int a,int b){return static_cast<double>(static_cast<unsigned long long>(a)*static_cast<unsigned>(b));}
// Each packed metal pair has one (nx+1)-by-(ny+1) table and two
// even-reflected axis tables. Cells contain two binary64 components.
inline bool potential_grid_fits(int nx,int ny,int metals,bool historical=false){
    if(nx<1||ny<1||(nx&(nx-1))||(ny&(ny-1))||metals<1||metals>32)return false;
    const auto x=static_cast<unsigned long long>(nx),y=static_cast<unsigned long long>(ny),largest=x>y?x:y;
    const auto cells=historical?(largest+1)*(largest+1)+4*largest:(x+1)*(y+1)+2*x+2*y;
    const unsigned pairs=metals*(metals+1)/2;
    // Divide the budget first: even hostile signed-32-bit dimensions cannot
    // overflow a byte count before admission rejects the allocation.
    return cells<=green_table_budget_bytes/(16u*pairs);
}
}
