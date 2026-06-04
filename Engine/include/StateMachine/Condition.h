#pragma once

template<typename T>
class Condition
{
public:
    virtual ~Condition() = default;

    virtual bool OnTest(T* pOwner) = 0;

    bool Expected = true;

    bool Test(T* pOwner)
    {
        return Expected == OnTest(pOwner);
    }
};