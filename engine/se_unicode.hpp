#ifndef _SE_UNICODE_HPP_
#define _SE_UNICODE_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/subsystems/se_debug.hpp"

struct SeUtf8Char
{
    char data[4];

    operator const char* () const { return data; }

    bool operator == (SeUtf8Char other) const
    {
        return
            (data[0] == other.data[0]) &
            (data[1] == other.data[1]) &
            (data[2] == other.data[2]) &
            (data[3] == other.data[3]);
            
    }
};

using SeUtf32Char = uint32_t;

struct SeUnicodeSpecialCharacters
{
    static constexpr const SeUtf8Char ESCAPE_UTF8           = { "\x1b" };
    static constexpr const SeUtf8Char BACKSPACE_UTF8        = { "\b" };
    static constexpr const SeUtf8Char CARRIAGE_RETURN_UTF8  = { "\r" };
    static constexpr const SeUtf8Char TAB_UTF8              = { "\t" };

    static constexpr const SeUtf32Char ESCAPE_CODEPOINT             = 0x0000001b;
    static constexpr const SeUtf32Char BACKSPACE_CODEPOINT          = 0x00000008;
    static constexpr const SeUtf32Char CARRIAGE_RETURN_CODEPOINT    = 0x0000000D;
    static constexpr const SeUtf32Char TAB_CODEPOINT                = 0x00000009;
};

namespace unicode
{
    inline bool is_special_character(SeUtf8Char character)
    {
        return
            (character == SeUnicodeSpecialCharacters::ESCAPE_UTF8) ||
            (character == SeUnicodeSpecialCharacters::BACKSPACE_UTF8) ||
            (character == SeUnicodeSpecialCharacters::CARRIAGE_RETURN_UTF8) ||
            (character == SeUnicodeSpecialCharacters::TAB_UTF8);
    }

    inline bool is_special_character(SeUtf32Char character)
    {
        return
            (character == SeUnicodeSpecialCharacters::ESCAPE_CODEPOINT) ||
            (character == SeUnicodeSpecialCharacters::BACKSPACE_CODEPOINT) ||
            (character == SeUnicodeSpecialCharacters::CARRIAGE_RETURN_CODEPOINT) ||
            (character == SeUnicodeSpecialCharacters::TAB_CODEPOINT);
    }

    SeUtf32Char convert(SeUtf8Char character)
    {
        SeUtf32Char result;
        if ((character.data[0] & 0x80) == 0)
        {
            result = SeUtf32Char(character.data[0]);
        }
        else if ((character.data[0] & 0xe0) == 0xc0)
        {
            result =
                ((SeUtf32Char(character.data[0]) & 0x1f) << 6)   |
                ((SeUtf32Char(character.data[1]) & 0x3f));
        }
        else if ((character.data[0] & 0xf0) == 0xe0)
        {
            result =
                ((SeUtf32Char(character.data[0]) & 0x0f) << 12)  |
                ((SeUtf32Char(character.data[1]) & 0x3f) << 6)   |
                ((SeUtf32Char(character.data[2]) & 0x3f));
        }
        else
        {
            result =
                ((SeUtf32Char(character.data[0]) & 0x07) << 18)  |
                ((SeUtf32Char(character.data[1]) & 0x3f) << 12)  |
                ((SeUtf32Char(character.data[2]) & 0x3f) << 6)   |
                ((SeUtf32Char(character.data[3]) & 0x3f));
        }
        return result;
    }
}

// =======================================================================
//
// Iterator for utf-8 codepoints
//
// =======================================================================

struct SeUtf8CodepointIterator
{
    const char* data;
};

struct SeUtf8CodepointIteratorInstance
{
    const uint8_t* data;
    size_t index;

    bool                                operator != (const SeUtf8CodepointIteratorInstance& other) const;
    SeUtf32Char                         operator *  ();
    SeUtf8CodepointIteratorInstance&    operator ++ ();
};

SeUtf8CodepointIteratorInstance begin(SeUtf8CodepointIterator data)
{
    return { (const uint8_t*)data.data, 0 };
}

SeUtf8CodepointIteratorInstance end(SeUtf8CodepointIterator data)
{
    return { (const uint8_t*)data.data, strlen(data.data) };
}

bool SeUtf8CodepointIteratorInstance::operator != (const SeUtf8CodepointIteratorInstance& other) const
{
    return index != other.index;
}

SeUtf32Char SeUtf8CodepointIteratorInstance::operator * ()
{
    const uint8_t* const utf8Sequence = data + index;
    SeUtf32Char result;
    if ((utf8Sequence[0] & 0x80) == 0)
    {
        result = SeUtf32Char(utf8Sequence[0]);
    }
    else if ((utf8Sequence[0] & 0xe0) == 0xc0)
    {
        result =
            ((SeUtf32Char(utf8Sequence[0]) & 0x1f) << 6)     |
            ((SeUtf32Char(utf8Sequence[1]) & 0x3f));
    }
    else if ((utf8Sequence[0] & 0xf0) == 0xe0)
    {
        result =
            ((SeUtf32Char(utf8Sequence[0]) & 0x0f) << 12)    |
            ((SeUtf32Char(utf8Sequence[1]) & 0x3f) << 6)     |
            ((SeUtf32Char(utf8Sequence[2]) & 0x3f));
    }
    else
    {
        result =
            ((SeUtf32Char(utf8Sequence[0]) & 0x07) << 18)    |
            ((SeUtf32Char(utf8Sequence[1]) & 0x3f) << 12)    |
            ((SeUtf32Char(utf8Sequence[2]) & 0x3f) << 6)     |
            ((SeUtf32Char(utf8Sequence[3]) & 0x3f));
    }
    return result;
}

SeUtf8CodepointIteratorInstance& SeUtf8CodepointIteratorInstance::operator ++ ()
{
    // This is based on https://github.com/skeeto/branchless-utf8/blob/master/utf8.h
    static const char lengths[] =
    {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
    };
    const uint8_t firstByte = data[index];
    index += lengths[firstByte >> 3];
    return *this;
}

// =======================================================================
//
// Iterator for utf-8 characters
//
// =======================================================================

struct SeUtf8CharacterIterator
{
    const char* data;
};

struct SeUtf8CharacterIteratorInstance
{
    const char* data;
    size_t index;

    bool                                operator != (const SeUtf8CharacterIteratorInstance& other) const;
    SeUtf8Char                          operator *  ();
    SeUtf8CharacterIteratorInstance&    operator ++ ();
};

SeUtf8CharacterIteratorInstance begin(SeUtf8CharacterIterator data)
{
    return { data.data, 0 };
}

SeUtf8CharacterIteratorInstance end(SeUtf8CharacterIterator data)
{
    return { data.data, strlen(data.data) };
}

bool SeUtf8CharacterIteratorInstance::operator != (const SeUtf8CharacterIteratorInstance& other) const
{
    return index != other.index;
}

SeUtf8Char SeUtf8CharacterIteratorInstance::operator * ()
{
    const char* const utf8Sequence = data + index;
    if ((utf8Sequence[0] & 0x80) == 0)
    {
        return { utf8Sequence[0] };
    }
    else if ((utf8Sequence[0] & 0xe0) == 0xc0)
    {
        return { utf8Sequence[0], utf8Sequence[1] };
    }
    else if ((utf8Sequence[0] & 0xf0) == 0xe0)
    {
        return { utf8Sequence[0], utf8Sequence[1], utf8Sequence[2] };
    }
    else
    {
        return { utf8Sequence[0], utf8Sequence[1], utf8Sequence[2], utf8Sequence[3] };
    }
}

SeUtf8CharacterIteratorInstance& SeUtf8CharacterIteratorInstance::operator ++ ()
{
    // This is based on https://github.com/skeeto/branchless-utf8/blob/master/utf8.h
    static const char lengths[] =
    {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
    };
    const uint8_t firstByte = ((const uint8_t*)data)[index];
    index += lengths[firstByte >> 3];
    return *this;
}

#endif