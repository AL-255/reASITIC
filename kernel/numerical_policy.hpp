#pragma once
namespace reasitic {
bool reference_numerics();
class ReferenceNumericsScope {
    bool saved;
public:
    explicit ReferenceNumericsScope(bool enabled=true);
    ~ReferenceNumericsScope();
    ReferenceNumericsScope(const ReferenceNumericsScope&)=delete;
    ReferenceNumericsScope& operator=(const ReferenceNumericsScope&)=delete;
};
}
