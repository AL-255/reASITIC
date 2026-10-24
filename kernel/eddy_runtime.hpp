#pragma once
#include "engine.hpp"
#include "asitic/eddy.hpp"
namespace reasitic {
struct EddySession {
    asitic::GreenIntegrationState state;
    asitic::GreenIntegrationWorkspace workspace;
    asitic::WarningSink warnings;
    int mode,layer_count,indices[2];
    unsigned evaluations,limit,status[8];
    bool exceeded;
    unsigned long long sample_hash,value_hash;
};
class EddyScope {
    EddySession* session;
    EddySession* previous;
public:
    explicit EddyScope(EddySession*);
    ~EddyScope();
};
bool configure_eddy(EddySession&,const EngineView&,int mode,int count,int first,int second,int budget);
asitic::EddyRunOptions eddy_options(EddySession&,const EngineView&);
char* eddy_metadata(char*,const EddySession&);
}
