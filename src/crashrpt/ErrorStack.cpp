#include "stdafx.h"
#include "ErrorStack.h"

ErrorStack::ErrorStack() {}

ErrorStack& ErrorStack::current()
{
    static thread_local ErrorStack tls;
    return tls;
}

void ErrorStack::clear()
{
    current().m_stack.clear();
}

void ErrorStack::push(const wchar_t* log)
{
    if (log)
    {
        current().m_stack.push_back(log);
    }
}

const std::wstring ErrorStack::dump()
{
    std::wstring ret;
    auto& stack = current().m_stack;
    for (auto& item : stack)
    {
        ret += item;
        ret += '\n';
    }
    if (!ret.empty())
    {
        ret.pop_back();
    }
    return ret;
}
