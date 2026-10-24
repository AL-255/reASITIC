// In-memory files for the original Green-table cache protocol. Enable only
// when an embedding caller explicitly requests reuse. Each arithmetic mode
// owns one bounded entry; the filename's rounded frequency is never the key.
struct PotentialMemoKey {
    int nx,ny,metals,layers;double frequency,chip_x,chip_y;
    unsigned char metal[32*236],substrate[32*40];
};
struct PotentialMemoEntry { PotentialMemoKey key;unsigned char* data;unsigned size;bool valid; };
PotentialMemoEntry potential_memos[2]{};bool potential_memo_enabled=false;
unsigned potential_memo_hits=0,potential_memo_misses=0,potential_memo_writes=0;
struct PotentialMemoFile {
    PotentialMemoKey key{};PotentialMemoEntry* entry=nullptr;unsigned char* pending=nullptr;
    unsigned position=0,capacity=0;bool writing=false,failed=false,enabled=false;
    PotentialMemoFile(const reasitic::EngineView& engine,double frequency,double x,double y,int nx,int ny){
        const unsigned pairs=engine.metal_count*(engine.metal_count+1)/2;
        const unsigned bytes=16+engine.substrate_count*40+engine.metal_count*236+pairs*((nx+1)*(ny+1)+2*nx+2*ny)*16u;
        // Production uses a consistent rectangular layout. The historical
        // diagnostic still needs padding not represented by the file protocol.
        enabled=potential_memo_enabled&&(nx==ny||!reasitic::reference_numerics())&&bytes<=16u*1024u*1024u;if(!enabled)return;
        capacity=bytes;key.nx=nx;key.ny=ny;key.metals=engine.metal_count;key.layers=engine.substrate_count;key.frequency=frequency;key.chip_x=x;key.chip_y=y;
        __builtin_memcpy(key.metal,engine.metals,engine.metal_count*236u);__builtin_memcpy(key.substrate,engine.substrate,engine.substrate_count*40u);
        entry=&potential_memos[reasitic::reference_numerics()?1:0];
    }
    ~PotentialMemoFile(){free(pending);}
    static void* open(const char*,const char* mode,void* user){
        auto& file=*static_cast<PotentialMemoFile*>(user);file.position=0;
        if(*mode=='r'){
            file.writing=false;if(file.entry->valid&&__builtin_memcmp(&file.key,&file.entry->key,sizeof(file.key))==0){++potential_memo_hits;return &file;}
            ++potential_memo_misses;return nullptr;
        }
        free(file.pending);file.pending=static_cast<unsigned char*>(calloc(1,file.capacity));file.writing=true;file.failed=!file.pending;return file.pending?&file:nullptr;
    }
    static unsigned read(void* data,unsigned size,unsigned count,void* handle,void*){
        auto& file=*static_cast<PotentialMemoFile*>(handle);if(!size||file.position>file.entry->size)return 0;
        const unsigned available=(file.entry->size-file.position)/size;if(count>available)count=available;
        __builtin_memcpy(data,file.entry->data+file.position,size*count);file.position+=size*count;return count;
    }
    static unsigned write(const void* data,unsigned size,unsigned count,void* handle,void*){
        auto& file=*static_cast<PotentialMemoFile*>(handle);if(!size||file.position>file.capacity||count>(file.capacity-file.position)/size){file.failed=true;return 0;}
        __builtin_memcpy(file.pending+file.position,data,size*count);file.position+=size*count;return count;
    }
    static int close(void* handle,void*){
        auto& file=*static_cast<PotentialMemoFile*>(handle);
        if(file.writing&&!file.failed&&file.position==file.capacity){free(file.entry->data);file.entry->data=file.pending;file.pending=nullptr;file.entry->size=file.capacity;file.entry->key=file.key;file.entry->valid=true;++potential_memo_writes;}
        return 0;
    }
    PotentialCacheFiles files(){return {open,read,write,close,sprintf,this};}
};
