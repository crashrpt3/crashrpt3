#include "stdafx.h"
#include "LastErrorThreaded.h"

LastErrorThreaded& LastErrorThreaded::thisThread()
{
    static thread_local LastErrorThreaded ndc;
    return ndc;
}

void LastErrorThreaded::push(LPCWSTR szLog)
{
    if (szLog)
    {
        m_stack.push_back(szLog);
    }
}

CString LastErrorThreaded::toString() const
{
    CString result;
    for (auto it = m_stack.begin(); it != m_stack.end(); ++it)
    {
        if (it != m_stack.begin())
        {
            result.AppendChar(L'\n');
        }
        result.Append(*it);
    }
    return result;
}

void LastErrorThreaded::clear()
{
    m_stack.clear();
}
