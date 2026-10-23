namespace asitic::detail {
// Native fault boundary from original 0x0804fede. Separate from the numerical
// caller so the regression gate can corrupt and independently audit this path.
[[noreturn]] void model_control_word_fault() {
    unsigned short saved,temporary;long long integer;
    asm volatile("fnstcw %0" : "=m"(saved));temporary=(saved&255u)|0x0c00u;
    asm volatile("fldcw %1; fistpll %0; fldcw %3" : "=m"(integer) : "m"(temporary),"t"(2.0L),
        "m"(*reinterpret_cast<const unsigned short*>(0xfffffffcu)) : "st");
    __builtin_unreachable();
}
}
