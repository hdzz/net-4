#pragma once

#include <cassert>
#include <istream>

#include <boost/iostreams/categories.hpp>
#include <boost/cstdint.hpp>

namespace net
{

class BinaryReadStream
{
public:
    typedef char char_type;
    typedef boost::iostreams::seekable_device_tag category;

    BinaryReadStream(const void* data, std::size_t size)
        : m_Data(reinterpret_cast<const char*>(data))
        , m_Size(size)
        , m_Read()
    {
    }

    std::streamsize read(char_type* s, std::streamsize n)
    {
        if (m_Read + n >= m_Size)
            n = m_Size - m_Read; // this is last block data

        std::copy(m_Data + m_Read, m_Data + m_Read + n, s);

        m_Read += static_cast<std::size_t>(n);
        return n;
    }

    std::streampos seek(std::streamoff offset, std::ios::seekdir dir)
    {
        if (dir == std::ios::cur)
            m_Read += offset;
        else
        if (dir == std::ios::beg)
            m_Read = offset;
        else
            m_Read = m_Size - offset;

        return m_Read;
    }

    std::streamsize write(const char_type* /*s*/, std::streamsize /*n*/)
    {
        assert(!"not implemented");
        return 0;
    }

private:
    const char* m_Data;
    const std::size_t m_Size;
    std::streamsize m_Read;
};


} // namespace net
