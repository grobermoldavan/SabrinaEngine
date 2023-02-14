#ifndef _SE_UNICODE_HPP_
#define _SE_UNICODE_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/subsystems/se_debug.hpp"
#include "engine/subsystems/se_platform.hpp"

// @NOTE : SeUtf8Char::data will not be null-terminated if utf8 representation of unicode codepoint requires 4 bytes
struct SeUtf8Char
{
    static constexpr size_t CAPACITY = 4;
    char data[CAPACITY];

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

    SeUtf32Char to_utf32(SeUtf8Char character)
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

    // Code taken from https://stackoverflow.com/a/42013433
    SeUtf8Char to_utf8(SeUtf32Char character)
    {
        SeUtf8Char result = {};
        if (character <= 0x7F) {
            result.data[0] = char(character);
        }
        else if (character <= 0x7FF) {
            result.data[0] = 0xC0 | char(character >> 6);           /* 110xxxxx */
            result.data[1] = 0x80 | char(character & 0x3F);         /* 10xxxxxx */
        }
        else if (character <= 0xFFFF) {
            result.data[0] = 0xE0 | char(character >> 12);          /* 1110xxxx */
            result.data[1] = 0x80 | char((character >> 6) & 0x3F);  /* 10xxxxxx */
            result.data[2] = 0x80 | char(character & 0x3F);         /* 10xxxxxx */
        }
        else if (character <= 0x10FFFF) {
            result.data[0] = 0xF0 | char(character >> 18);          /* 11110xxx */
            result.data[1] = 0x80 | char((character >> 12) & 0x3F); /* 10xxxxxx */
            result.data[2] = 0x80 | char((character >> 6) & 0x3F);  /* 10xxxxxx */
            result.data[3] = 0x80 | char(character & 0x3F);         /* 10xxxxxx */
        }
        else
        {
            se_assert(!"Unsupported utf32 characrer");
        }
        return result;
    }

    inline SeUtf8Char to_utf8(wchar_t character)
    {
        SeUtf8Char result = {};
        platform::wchar_to_utf8(&character, 1, result.data, SeUtf8Char::CAPACITY);
        return result;
    }

    inline SeUtf32Char to_utf32(wchar_t character)
    {
        return SeUtf32Char(character);
    }

    inline size_t length(SeUtf8Char character)
    {
        return (character.data[0] != 0) + (character.data[1] != 0) + (character.data[2] != 0) + (character.data[3] != 0);
    }
}

// =======================================================================
//
// Iterator for utf-32 codepoints (supports utf-8 and utf-32 sources)
//
// =======================================================================

template<typename T>
struct SeUtf32Iterator
{
    const T* source; // null-terminated string
};

//
// This iterator takes utf-8 string and outputs utf-32 codepoints
//
struct SeUtf32IteratorInstanceUtf8
{
    const uint8_t* data;
    size_t index;

    bool                            operator != (const SeUtf32IteratorInstanceUtf8& other) const;
    SeUtf32Char                     operator *  ();
    SeUtf32IteratorInstanceUtf8&    operator ++ ();
};

inline SeUtf32IteratorInstanceUtf8 begin(SeUtf32Iterator<char> data)
{
    return { (const uint8_t*)data.source, 0 };
}

inline SeUtf32IteratorInstanceUtf8 end(SeUtf32Iterator<char> data)
{
    return { (const uint8_t*)data.source, strlen(data.source) };
}

inline bool SeUtf32IteratorInstanceUtf8::operator != (const SeUtf32IteratorInstanceUtf8& other) const
{
    return index != other.index;
}

SeUtf32Char SeUtf32IteratorInstanceUtf8::operator * ()
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

inline SeUtf32IteratorInstanceUtf8& SeUtf32IteratorInstanceUtf8::operator ++ ()
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

//
// This iterator takes utf-32 string and outputs utf-32 codepoints
//
struct SeUtf32IteratorInstanceUtf32
{
    const SeUtf32Char* data;
    size_t index;

    bool                            operator != (const SeUtf32IteratorInstanceUtf32& other) const;
    SeUtf32Char                     operator *  ();
    SeUtf32IteratorInstanceUtf32&   operator ++ ();
};

inline SeUtf32IteratorInstanceUtf32 begin(SeUtf32Iterator<SeUtf32Char> data)
{
    return { data.source, 0 };
}

inline SeUtf32IteratorInstanceUtf32 end(SeUtf32Iterator<SeUtf32Char> data)
{
    const size_t length = [](const SeUtf32Char* source) -> size_t
    {
        size_t result = 0;
        while (*source)
        {
            result += 1;
            source++;
        }
        return result;
    } (data.source);
    return { data.source, length };
}

inline bool SeUtf32IteratorInstanceUtf32::operator != (const SeUtf32IteratorInstanceUtf32& other) const
{
    return index != other.index;
}

inline SeUtf32Char SeUtf32IteratorInstanceUtf32::operator * ()
{
    return data[index];
}

inline SeUtf32IteratorInstanceUtf32& SeUtf32IteratorInstanceUtf32::operator ++ ()
{
    index += 1;
    return *this;
}

// =======================================================================
//
// Iterator for utf-8 characters
//
// =======================================================================

struct SeUtf8Iterator
{
    const char* utf8string;
};

struct SeUtf8teratorInstance
{
    const char* data;
    size_t index;

    bool                    operator != (const SeUtf8teratorInstance& other) const;
    SeUtf8Char              operator *  ();
    SeUtf8teratorInstance&  operator ++ ();
};

SeUtf8teratorInstance begin(SeUtf8Iterator data)
{
    return { data.utf8string, 0 };
}

SeUtf8teratorInstance end(SeUtf8Iterator data)
{
    return { data.utf8string, strlen(data.utf8string) };
}

bool SeUtf8teratorInstance::operator != (const SeUtf8teratorInstance& other) const
{
    return index != other.index;
}

SeUtf8Char SeUtf8teratorInstance::operator * ()
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

SeUtf8teratorInstance& SeUtf8teratorInstance::operator ++ ()
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