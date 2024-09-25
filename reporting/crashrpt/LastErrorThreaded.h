#pragma once

class LastErrorThreaded
{
public:
    static LastErrorThreaded& thisThread();
    void push(LPCWSTR szLog);
    CString toString() const;
    void clear();

private:
    std::vector<CString> m_stack;
};

#define crLastErrorClear() LastErrorThreaded::thisThread().clear();
#define crLastErrorAdd(msg) LastErrorThreaded::thisThread().push(msg)
