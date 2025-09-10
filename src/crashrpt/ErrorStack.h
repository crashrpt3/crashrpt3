#pragma once

class ErrorStack
{
public:
    static void clear();
    static void push(const wchar_t* log);
    static const std::wstring dump();

private:
    static ErrorStack& current();
    ErrorStack();;
    ErrorStack(const ErrorStack&) = delete;
    ErrorStack& operator=(const ErrorStack&) = delete;

private:
    std::vector<std::wstring> m_stack;
};
