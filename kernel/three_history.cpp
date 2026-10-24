#include "three_history.hpp"
namespace {
reasitic::ThreePortHistory* active=nullptr;
void* get(unsigned bytes,void* user) {
    auto& h=*static_cast<reasitic::ThreePortHistory*>(user);const unsigned expected[]={static_cast<unsigned>(h.n)*8u,static_cast<unsigned>(h.n)*h.n*64u,static_cast<unsigned>(h.n)*96u};const int order[]={4,2,3};
    const int i=h.allocation++;if(i<3 && bytes==expected[i])return h.blocks[order[i]];
    h.contract_matches=false;return h.memory.arrays.get(bytes);
}
void put(void* p,void* user) {
    auto& h=*static_cast<reasitic::ThreePortHistory*>(user);for(int i=2;i<5;++i)if(p==h.blocks[i])return;h.memory.put(p);
}
bool same_bytes(const void* a,const void* b,unsigned bytes) {
    const auto* p=static_cast<const unsigned char*>(a);const auto* q=static_cast<const unsigned char*>(b);for(unsigned i=0;i<bytes;++i)if(p[i]!=q[i])return false;return true;
}
bool prepare(reasitic::ThreePortHistory& h,const asitic::ComplexMatrix& cap,const asitic::ComplexMatrix& ind,int n) {
    if(n<1 || n>512 || h.cells!=n*n || !h.addresses)return false;
    if(!same_bytes(cap.data,h.capacitance,cap.elements*16u) || !same_bytes(ind.data,h.inductance,ind.elements*16u)){h.prefix_matches=false;return false;}
    struct Range{unsigned long long first,last;int id;};Range ranges[5];
    const unsigned sizes[]={static_cast<unsigned>(n)*n*16u,static_cast<unsigned>(n)*n*16u,static_cast<unsigned>(n)*n*64u,static_cast<unsigned>(n)*96u,static_cast<unsigned>(n)*8u};
    for(int i=0;i<5;++i){ranges[i]={h.addresses[i],static_cast<unsigned long long>(h.addresses[i])+sizes[i],i};if(!ranges[i].first || ranges[i].last>0x100000000ULL)return false;}
    const unsigned char* input_bytes[]={reinterpret_cast<const unsigned char*>(h.capacitance),reinterpret_cast<const unsigned char*>(h.inductance)};
    for(int a=0;a<2;++a)for(int b=a+1;b<4;++b){
        const auto begin=ranges[a].first>ranges[b].first?ranges[a].first:ranges[b].first,end=ranges[a].last<ranges[b].last?ranges[a].last:ranges[b].last;
        for(auto address=begin;address<end;++address){const auto value=input_bytes[a][static_cast<unsigned>(address-ranges[a].first)];
            if(b<2){if(value!=input_bytes[b][static_cast<unsigned>(address-ranges[b].first)])return false;}else if(value!=0)return false;
        }
    }
    for(int i=0;i<5;++i)for(int j=i+1;j<5;++j)if(ranges[j].first<ranges[i].first){const auto temp=ranges[i];ranges[i]=ranges[j];ranges[j]=temp;}
    for(int begin=0;begin<5;){int end=begin+1;auto last=ranges[begin].last;while(end<5 && ranges[end].first<=last){if(last<ranges[end].last)last=ranges[end].last;++end;}
        const auto bytes=last-ranges[begin].first;if(bytes>32u*1024u*1024u)return false;auto* arena=static_cast<unsigned char*>(h.memory.arrays.get(static_cast<unsigned>(bytes)));
        for(int i=begin;i<end;++i)h.blocks[ranges[i].id]=arena+static_cast<unsigned>(ranges[i].first-ranges[begin].first);begin=end;
    }
    __builtin_memcpy(h.blocks[0],h.capacitance,h.cells*16u);__builtin_memcpy(h.blocks[1],h.inductance,h.cells*16u);h.n=n;return true;
}
}
namespace reasitic {
ThreePortHistoryScope::ThreePortHistoryScope(ThreePortHistory* h):previous(active){active=h;}
ThreePortHistoryScope::~ThreePortHistoryScope(){active=previous;}
}
namespace asitic {
bool run_three_port_equations(ComplexMatrix& cap,const ComplexMatrix& ind,int first,int second,int n,ThreePortImpedance& output,SolverMemory memory,const NodeEquationOptions& options) {
    if(!active){
        // Both solvers already eliminated reference conductors. Their actual
        // dimension is the node-system contract; a reference cannot create
        // extra current unknowns without corresponding matrix coefficients.
        if(n<1 || cap.rows!=n || cap.columns!=n || ind.rows!=n || ind.columns!=n || cap.stride!=n || ind.stride!=n){
            options.warnings.emit("Three-port matrices do not match the node-system dimension.");return false;
        }
        return solve_three_port_equations(cap.data,ind.data,first,second,n,output,memory,options);
    }
    auto& h=*active;if(!prepare(h,cap,ind,n)){h.contract_matches=false;return false;}
    const SolverMemory overlay{{get,&h},put};return solve_three_port_equations(static_cast<ComplexValue*>(h.blocks[0]),static_cast<const ComplexValue*>(h.blocks[1]),first,second,n,output,overlay,options);
}
}
