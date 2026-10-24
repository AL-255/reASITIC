#pragma once
namespace reasitic {
inline char* json_string(char* out,const char* text) {
    static const char hex[]="0123456789abcdef";*out++='"';
    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text);*p;++p) {
        if(*p=='"' || *p=='\\') { *out++='\\';*out++=*p; }
        else if(*p<32) { *out++='\\';*out++='u';*out++='0';*out++='0';*out++=hex[*p>>4];*out++=hex[*p&15]; }
        else *out++=*p;
    }
    *out++='"';*out=0;return out;
}
}
