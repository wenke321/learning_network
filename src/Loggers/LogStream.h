#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
template <int SIZE>
class FixedBuffer
{
   public:
    FixedBuffer() : cur(data) {}

    void append(const char* str, int len)
    {
        if (avail() >= len)
        {
            memcpy(cur, str, len);
            cur += len;
        }
    }

    std::string toString() const { return std::string(data, length()); }

    const char* data_() const { return data; }
    char* cur_() { return cur; }
    int length() const { return cur - data; }
    int avail() { return end() - cur; }
    void reset() { cur = data; }
    void add(int off) { cur += off; }
    void bzero() { memset(data, 0, sizeof(data)); }

   private:
    const char* end() { return data + sizeof(data); }

    char data[SIZE];
    char* cur;
};

const char digits[]     = "9876543210123456789";
static const char* zero = digits + 9;

static const char digitsHex[] = "0123456789ABCDEF";

template <typename T>
size_t convert(char buf[], T value)
{
    T i     = value;
    char* p = buf;

    do
    {
        int lsd = static_cast<int>(i % 10);
        i /= 10;
        *p++ = zero[lsd];
    } while (i != 0);

    if (value < 0)
    {
        *p++ = '-';
    }
    *p = '\0';
    std::reverse(buf, p);

    return p - buf;
}

size_t convertHex(char buf[], uintptr_t value);

class LogStream
{
   public:
    typedef FixedBuffer<4096> Buffer;

    LogStream& operator<<(bool is)
    {
        buffer.append(is ? "1" : "0", 1);
        return *this;
    }

    LogStream& operator<<(short v)
    {
        formatedInt(static_cast<int>(v));
        return *this;
    }
    LogStream& operator<<(unsigned short v)
    {
        formatedInt(static_cast<unsigned int>(v));
        return *this;
    }
    LogStream& operator<<(int v)
    {
        formatedInt(v);
        return *this;
    }
    LogStream& operator<<(unsigned int v)
    {
        formatedInt(v);
        return *this;
    }
    LogStream& operator<<(long v)
    {
        formatedInt(v);
        return *this;
    }
    LogStream& operator<<(unsigned long v)
    {
        formatedInt(v);
        return *this;
    }
    LogStream& operator<<(long long v)
    {
        formatedInt(v);
        return *this;
    }
    LogStream& operator<<(unsigned long long v)
    {
        formatedInt(v);
        return *this;
    }

    LogStream& operator<<(float v)
    {
        *this << static_cast<double>(v);
        return *this;
    }
    LogStream& operator<<(double v)
    {
        if (buffer.avail() >= MaxNumericSize)
        {
            int len = snprintf(buffer.cur_(), MaxNumericSize, "%.12g", v);
            buffer.add(len);
        }
        return *this;
    }
    LogStream& operator<<(const void* x)
    {
        uintptr_t v = reinterpret_cast<uintptr_t>(x);
        if (buffer.avail() >= MaxNumericSize)
        {
            char* buf  = buffer.cur_();
            buf[0]     = '0';
            buf[1]     = 'x';
            size_t len = convertHex(buf + 2, v);
            buffer.add(len + 2);
        }
        return *this;
    }

    LogStream& operator<<(const char* str)
    {
        if (str)
        {
            buffer.append(str, strlen(str));
        }
        else
        {
            buffer.append("(null)", 6);
        }
        return *this;
    }
    LogStream& operator<<(const unsigned char* str)
    {
        *this << reinterpret_cast<const char*>(str);
        return *this;
    }
    LogStream& operator<<(const std::string& str)
    {
        buffer.append(str.c_str(), str.size());
        return *this;
    }
    LogStream& operator<<(const Buffer& buf_)
    {
        *this << buf_.toString();
        return *this;
    }

    LogStream& operator<<(char c)
    {
        buffer.append(&c, 1);
        return *this;
    }

    void append(const char* data_, int len_) { buffer.append(data_, len_); }
    const Buffer& buffer_() { return buffer; }
    void reset() { buffer.reset(); }

   private:
    template <typename T>
    void formatedInt(T v)
    {
        if (buffer.avail() >= MaxNumericSize)
        {
            size_t len = convert(buffer.cur_(), v);
            buffer.add(len);
        }
    }

    Buffer buffer;
    static const int MaxNumericSize = 48;
};

class Fmt  // : noncopyable
{
   public:
    template <typename T>
    Fmt(const char* fmt, T val);

    const char* data() const { return buf_; }
    int length() const { return length_; }

   private:
    char buf_[32];
    int length_;
};

inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
{
    s.append(fmt.data(), fmt.length());
    return s;
}