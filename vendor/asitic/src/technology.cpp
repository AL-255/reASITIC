#include "asitic/technology.hpp"
#include "x87.hpp"

namespace asitic {
namespace {
void invalid_layer(int index,WarningSink warnings) {
    char message[80],digits[12];char* out=message;
    const char prefix[]="Error:  Eddy layer ",suffix[]=" does not exist.";
    for(const char* p=prefix;*p;++p)*out++=*p;
    unsigned value=static_cast<unsigned>(index),count=0;
    if(index<0) { *out++='-';value=0u-value; }
    do { digits[count++]=static_cast<char>('0'+value%10);value/=10; }while(value);
    while(count)*out++=digits[--count];
    for(const char* p=suffix;*p;++p)*out++=*p;
    *out=0;warnings.emit(message);
}
}
void configure_green_layers(GreenIntegrationState& state,int& cached_count,const SubstrateLayer* substrate,
    int substrate_count,const int* indices,int count,WarningSink warnings) {
    for(int i=0;i<count;++i)if(indices[i]>=substrate_count) {
        invalid_layer(indices[i],warnings);cached_count=0;return;
    }
    cached_count=count;
    if(count==1) {
        state.layers.layer1=x87::binary64(1000000.0L/substrate[indices[0]].resistivity);
        state.kernel=GreenKernel::reflection;
    } else if(count==2) {
        int high=indices[0],low=indices[1];
        if(high<low) { high=indices[1];low=indices[0]; }
        state.layers.layer1=x87::binary64(1000000.0L/substrate[high].resistivity);
        state.layers.layer2=x87::binary64(1000000.0L/substrate[low].resistivity);
        state.layers.depth=x87::binary64(0x1.0c6f7a0b5ed8dp-20L*substrate[high].thickness);
        state.kernel=GreenKernel::oscillating;
    } else {
        state.layers.layer1=x87::binary64(1000000.0L/substrate[indices[0]].resistivity);
        state.layers.layer2=state.layers.layer1;state.layers.depth=0x1.a36e2eb1c432dp-14;
        state.kernel=GreenKernel::oscillating;
    }
}
}
