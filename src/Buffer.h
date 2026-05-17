
#pragma once
#include <sys/types.h>
#include <sys/uio.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "Endian.h"
#include "SocketOps.h"

class stringPiece
{
   public:
    constexpr stringPiece(const char* data, size_t size) : data_(data), size_(size) {}
    stringPiece(const std::string& msg) : data_(msg.data()), size_(msg.size()) {}
    stringPiece(const char* data) : data_(data), size_(strlen(data)) {}

    const char* data() const { return data_; }
    size_t size() const { return size_; }

    void clear()
    {
        data_ = nullptr;
        size_ = 0;
    }

    bool operator==(const char* s)
    {
        const char* a = data_;
        for (char c = *s; c; ++s, ++a, c = *s)
        {
            if (c != *a) return false;
        }
        return true;
    }

   private:
    const char* data_;
    size_t size_;
};

class Buffer
{
   public:
    static const size_t CheapPrepend = 8;
    static const size_t InitialSize  = 1024;

    Buffer(size_t initSize = InitialSize) : buf(CheapPrepend + initSize), read_idx(CheapPrepend), write_idx(CheapPrepend)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == initSize);
        assert(prependableBytes() == CheapPrepend);
    }

    size_t readableBytes() const { return write_idx - read_idx; }
    size_t writableBytes() { return buf.size() - write_idx; }
    size_t prependableBytes() { return read_idx; }

    void swap(Buffer& rhs)
    {
        buf.swap(rhs.buf);
        std::swap(read_idx, rhs.read_idx);
        std::swap(write_idx, rhs.write_idx);
    }

    const char* readBegin() const { return buf.data() + read_idx; }
    const char* writeBegin() const { return buf.data() + write_idx; }
    char* writeBegin() { return buf.data() + write_idx; }
    const char* findCRLF() const
    {
        const char* crlf = (char*)memmem(readBegin(), readableBytes(), CRLF, 2);
        return crlf == writeBegin() ? nullptr : crlf;
    }

    const char* findCRLF(const char* start) const
    {
        assert(start >= readBegin());
        assert(start <= writeBegin());
        const char* crlf = (char*)memmem(start, readableBytes() - (start - readBegin()), CRLF, 2);
        return crlf == writeBegin() ? nullptr : crlf;
    }

    const char* findEOL() const { return (const char*)memchr(readBegin(), '\n', readableBytes()); }

    const char* findEOL(const char* start) const
    {
        assert(start <= readBegin());
        assert(start <= writeBegin());
        return (const char*)memchr(start, '\n', writeBegin() - start);
    }

    // after read all
    void retrieve(size_t len)
    {
        assert(len <= readableBytes());
        if (len < readableBytes())
        {
            read_idx += len;
        }
        else
        {
            retrieveAll();
        }
    }

    void retrieveUntil(const char* end)
    {
        assert(end >= readBegin());
        assert(end <= writeBegin());
        retrieve(end - readBegin());
    }

    void retrieveInt64() { retrieve(sizeof(int64_t)); }

    void retrieveInt32() { retrieve(sizeof(int32_t)); }

    void retrieveInt16() { retrieve(sizeof(int16_t)); }

    void retrieveInt8() { retrieve(sizeof(int8_t)); }

    // after read all
    void retrieveAll()
    {
        read_idx  = CheapPrepend;
        write_idx = CheapPrepend;
    }

    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }

    std::string retrieveAsString(size_t len)
    {
        assert(len <= readableBytes());
        std::string result(readBegin(), len);
        retrieve(len);
        return result;
    }

    stringPiece toStringPiece() const { return stringPiece(readBegin(), static_cast<int>(readableBytes())); }

    void append(const stringPiece& str) { append(str.data(), str.size()); }

    void append(const char* /*restrict*/ data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, writeBegin());
        hasWritten(len);
    }

    void append(const void* /*restrict*/ data, size_t len) { append(static_cast<const char*>(data), len); }

    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            addCap(len - writableBytes());
        }
        assert(writableBytes() >= len);
    }

    void hasWritten(size_t len)
    {
        assert(len <= writableBytes());
        write_idx += len;
    }

    void unwrite(size_t len)
    {
        assert(len <= readableBytes());
        write_idx -= len;
    }

    ///
    /// Append int64_t using network endian
    ///
    void appendInt64(int64_t x)
    {
        int64_t be64 = sockOption::hostToNetwork64(x);
        append(&be64, sizeof be64);
    }

    ///
    /// Append int32_t using network endian
    ///
    void appendInt32(int32_t x)
    {
        int32_t be32 = sockOption::hostToNetwork32(x);
        append(&be32, sizeof be32);
    }

    void appendInt16(int16_t x)
    {
        int16_t be16 = sockOption::hostToNetwork16(x);
        append(&be16, sizeof be16);
    }

    void appendInt8(int8_t x) { append(&x, sizeof x); }

    ///
    /// Read int64_t from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int32_t)
    int64_t readInt64()
    {
        int64_t result = peekInt64();
        retrieveInt64();
        return result;
    }

    ///
    /// Read int32_t from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int32_t)
    int32_t readInt32()
    {
        int32_t result = peekInt32();
        retrieveInt32();
        return result;
    }

    int16_t readInt16()
    {
        int16_t result = peekInt16();
        retrieveInt16();
        return result;
    }

    int8_t readInt8()
    {
        int8_t result = peekInt8();
        retrieveInt8();
        return result;
    }

    ///
    /// Peek int64_t from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int64_t)
    int64_t peekInt64() const
    {
        assert(readableBytes() >= sizeof(int64_t));
        int64_t be64 = 0;
        ::memcpy(&be64, readBegin(), sizeof(be64));
        return sockOption::networkToHost64(be64);
    }

    ///
    /// Peek int32_t from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int32_t)
    int32_t peekInt32() const
    {
        assert(readableBytes() >= sizeof(int32_t));
        int32_t be32 = 0;
        ::memcpy(&be32, readBegin(), sizeof(be32));
        return sockOption::networkToHost32(be32);
    }

    int16_t peekInt16() const
    {
        assert(readableBytes() >= sizeof(int16_t));
        int16_t be16 = 0;
        ::memcpy(&be16, readBegin(), sizeof be16);
        return sockOption::networkToHost16(be16);
    }

    int8_t peekInt8() const
    {
        assert(readableBytes() >= sizeof(int8_t));
        int8_t x = *readBegin();
        return x;
    }

    ///
    /// Prepend int64_t using network endian
    ///
    void prependInt64(int64_t x)
    {
        int64_t be64 = sockOption::hostToNetwork64(x);
        prepend(&be64, sizeof be64);
    }

    ///
    /// Prepend int32_t using network endian
    ///
    void prependInt32(int32_t x)
    {
        int32_t be32 = sockOption::hostToNetwork32(x);
        prepend(&be32, sizeof be32);
    }

    void prependInt16(int16_t x)
    {
        int16_t be16 = sockOption::hostToNetwork16(x);
        prepend(&be16, sizeof be16);
    }

    void prependInt8(int8_t x) { prepend(&x, sizeof x); }

    void prepend(const void* /*restrict*/ data, size_t len)
    {
        assert(len <= prependableBytes());
        read_idx -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d + len, buf.begin() + read_idx);
    }

    void shrink(size_t reserve)
    {
        // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
        Buffer other;
        other.ensureWritableBytes(readableBytes() + reserve);
        other.append(toStringPiece());
        swap(other);
    }

    size_t internalCapacity() const { return buf.capacity(); }

    /// Read data directly into buffer.
    ///
    /// It may implement with readv(2)
    /// @return result of read(2), @c errno is saved
    ssize_t readFd(int fd, int* savedErrno)
    {
        char extra[65536];
        struct iovec iov[2];
        size_t writable = writableBytes();
        iov[0].iov_base = begin() + write_idx;
        iov[0].iov_len  = writable;
        iov[1].iov_base = extra;
        iov[1].iov_len  = sizeof(extra);

        int iovcnt = writableBytes() < sizeof(extra) ? 2 : 1;
        ssize_t n  = readv(fd, iov, iovcnt);

        if (n < 0)
        {
            *savedErrno = sockOption::getSocketError(fd);
        }
        else if ((size_t)n <= writableBytes())
        {
            write_idx += n;
        }
        else
        {
            write_idx = buf.size();
            append(extra, n - writable);
        }
        return n;
    }

   private:
    char* begin() { return &*buf.begin(); }

    void addCap(size_t len)
    {
        if (read_idx >= len + CheapPrepend)
        {
            buf.resize(len + buf.size());
        }

        size_t readable = write_idx - read_idx;
        memmove(buf.data() + CheapPrepend, buf.data() + read_idx, readable);
        read_idx  = CheapPrepend;
        write_idx = CheapPrepend + readable;
    }

    std::vector<char> buf;
    size_t read_idx;
    size_t write_idx;

    static constexpr char CRLF[] = "\r\n";
};
