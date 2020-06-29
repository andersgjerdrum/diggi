/**
 * @file zero_copy_string.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief implementation of zero copy virtual contiguous buffer based on mbuf
 * this class is further heavily used by jsonparser.cpp
 * @see mbuf.cpp
 * @see jsonparser.cpp
 * @version 0.1
 * @date 2020-02-05
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "ZeroCopyString.h"

/**
 * @brief static literal operator overload.
 * strings declared as literal statics may be assinged to zcstring buffers.
 */
zcstring operator"" _M(const char *arr, size_t size) { return zcstring(arr, size); }

/**
 * @brief Construct a new zcstring::zcstring object
 * construct new zcstring based on existing buffer.
 * @param head 
 * @param size 
 */
zcstring::zcstring(char *head, size_t size) : length(size), offset(0), reserved(0)
{
    mbuf = mbuf_new();
    mbuf_append_tail(mbuf, head, size, length + offset);
}
/**
 * @brief Construct a new zcstring::zcstring object
 * based on constant zero-terminated buffer.
 * @warning invokes a traversal of buffer to find zero-termination-character using strlen
 * @param head 
 */
zcstring::zcstring(const char *head) : length(0), offset(0), reserved(0)
{
    mbuf = mbuf_new();
    mbuf_append_tail(mbuf, head, length + offset);
    if (mbuf->head != nullptr)
    {
        length = mbuf->head->size;
    }
    else
    {
        length = 0;
    }
}
/**
 * @brief Construct a new zcstring::zcstring object
 * based on constant buffer and size
 * @param head 
 * @param size 
 */
zcstring::zcstring(const char *head, size_t size) : length(size), offset(0), reserved(0)
{
    mbuf = mbuf_new();
    mbuf_append_tail(mbuf, head, size, length + offset);
}
/**
 * @brief Construct a new zcstring::zcstring object
 * based on STL string
 * @warning incurr a copy of string
 * @param str STL string
 * @param type unused parameter, future work
 */
zcstring::zcstring(std::string str, zcstring_reference_type_t type) : length(0), offset(0), reserved(0)
{
    append(str, type);
}
/**
 * @brief Construct a new zcstring::zcstring object
 * based on constant STL string input as reference.
 * @param str 
 */
zcstring::zcstring(const std::string &str) : length(str.size()), offset(0), reserved(0)
{
    mbuf = mbuf_new();
    mbuf_append_tail(mbuf, str.c_str(), str.size(), length + offset);
}
/**
 * @brief Construct a new zcstring::zcstring object
 * based on STL string reference
 * @param str 
 */
zcstring::zcstring(std::string &str) : length(str.size()), offset(0), reserved(0)
{
    mbuf = mbuf_new();
    mbuf_append_tail(mbuf, str.c_str(), str.size(), length + offset);
}
/**
 * @brief Construct a new zcstring::zcstring object
 * based on existing mbuf with given size.
 * Creates a new internal mbuf struct pointing to head of input mbuf chain and invokes an incref.
 * @param head 
 * @param size 
 */
zcstring::zcstring(mbuf_t *head, size_t size) : length(size), offset(0), reserved(0)
{
    mbuf = mbuf_new();
    mbuf->head = head->head;
    mbuf_incref(mbuf);
}
/**
 * @brief Construct a new zcstring::zcstring object
 * Copy constructor for const zcsting
 * @warning causes traversal of input zcstring, may be costly for fragmented zcstring
 * @warning does NOT invoke a copy on underlying data.
 * @param obj 
 */
zcstring::zcstring(const zcstring &obj) : length(obj.length), offset(obj.offset), reserved(0)
{
    mbuf = mbuf_new();
    mbuf->head = obj.mbuf->head;
    if (mbuf->head != nullptr)
    {
        /*
			Copy constructor for empty zcstring
		*/

        mbuf_incref(mbuf);
    }
    else
    {
        DIGGI_ASSERT(obj.length == 0);
        DIGGI_ASSERT(obj.offset == 0);
    }
}
/**
 * @brief Construct a new zcstring::zcstring object
 * based on existing mbuf with an offset into it.
 * Creates a new internal mbuf struct pointing to offset mbuf_node_t of input mbuf chain and invokes an incref.
 * @warning causes traversal of input mbuf, may be costly for fragmented zcstring
 * @param head 
 * @param size 
 * @param offst 
 */
zcstring::zcstring(mbuf_t *head, size_t size, size_t offst) : length(size), reserved(0)
{

    mbuf = mbuf_new();
    unsigned base;
    auto node = mbuf_node_at_pos(head, offst, &base);
    mbuf->head = node;
    mbuf_incref(mbuf);
    offset = offst - base;
}
/**
 * @brief Construct a new zcstring::zcstring object
 * Empty zcstring with new mbuf internal representation.
 * 
 */
zcstring::zcstring() : length(0), offset(0), reserved(0)
{
    mbuf = mbuf_new();
}
/**
 * @brief Destroy the zcstring::zcstring object
 * destroy zcstring, invokes mbuf_destroy, which in turn decreases reference counting on object.
 * If no more references to data in zcstring, internal structures will be deallocated
 * @warning does not free data in zcstring unless explicitly reserved by zcstring::reserve()
 */
zcstring::~zcstring()
{
    mbuf_destroy(mbuf, offset, length);
    mbuf = nullptr;
    length = 0;
    offset = 0;
}
/**
 * @brief check if zcstring is empty
 * 
 * @return true 
 * @return false 
 */
bool zcstring::empty()
{
    return (size() == 0);
}
/**
 * @brief check zcstring size
 * 
 * @return size_t 
 */
size_t zcstring::size() const
{
    return length;
}
/**
 * @brief reserver buffer of given size for management in mbuf implementation
 * allcoates buffer of given size and returns pointer to buffer.
 * Must be subsequently used by append to be added.
 * used in network buffer recieve to reduce copy operations, where it is not possible to predict buffer size.
 * @param size 
 * @return void* 
 */
void *zcstring::reserve(size_t size)
{
    DIGGI_ASSERT(reserved == 0);
    reserved = size;
    return malloc(size);
}
/**
 * @brief if buffer is not neccesary, free it.
 * @param ptr 
 */
void zcstring::abort_reserve(void *ptr)
{
    DIGGI_ASSERT(ptr);
    DIGGI_ASSERT(reserved > 0);
    free(ptr);
    reserved = 0;
}

/**
 * @brief index operator overload, return byte at given position
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param index 
 * @return char& 
 */
char &zcstring::operator[](unsigned index) const
{
    //assert is acceptable as out of bounds values should cause segfault.
    DIGGI_ASSERT(index < size());

    index += offset;
    unsigned nodebase = 0;

    mbuf_node_t *node = mbuf_node_at_pos(mbuf, index, &nodebase);

    return node->data[index - nodebase];
}
/**
 * @brief find  substring in zcstring 
 * expects zero terminated constant character buffer.
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param right string to match
 * @return unsigned returns index of first letter in string, return UINT_MAX if not found
 */
unsigned zcstring::find(const char *right)
{
    zcstring ztr(right);
    return indexof(&ztr);
}
/**
 * @brief find string starting from index
 * expect constant STL string as input
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param right string, by reference(no copy)
 * @param index index to start search
 * @return unsigned returns index of first letter in string, return UINT_MAX if not found
 */
unsigned zcstring::find(const std::string &right, unsigned index)
{
    zcstring ztr((char *)right.c_str(), right.size());
    return indexof(&ztr, index, false);
}
/**
 * @brief find string starting from index
 * expect null terminated constant char buffer
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param right string to search for
 * @param index index to start search
 * @return unsigned returns index of first letter in string, return UINT_MAX if not found
 */
unsigned zcstring::find(const char *right, unsigned index)
{
    zcstring ztr(right);
    return indexof(&ztr, index, false);
}
/**
 * @brief find last instance of string matching input criterion
 * expects null terminated constant char buffer
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param str string to search for 
 * @return unsigned index of last instance, return UINT_MAX if not found
 */
unsigned zcstring::find_last_of(const char *str)
{
    zcstring strz(str);
    return indexof(&strz, 0, true);
}
/**
 * @brief find last instance of string matching input criterion
 * expects constant STL string buffer
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param str string to search for
 * @return unsigned index of last instance, return UINT_MAX if not found 
 */
unsigned zcstring::find_last_of(const std::string &str)
{
    zcstring strz(str);
    return indexof(&strz, 0, true);
}
/**
 * @brief find index of string
 * expects pointer to zcstring holding patern to match for
 * @warning must be physically contigous
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param right string to search for
 * @return unsigned index of instance, return UINT_MAX if not found
 */
unsigned zcstring::indexof(zcstring *right)
{
    return indexof(right, 0, false);
}

/**
 * @brief find index of zcstring pattern
 * assume single contiguous memory region for right zcstring
 * supports non contiguous left zcstring
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param right string to search for
 * @param index index to start 
 * @param last check for last occurence.
 * @return unsigned index of instance, return UINT_MAX if not found
 */
unsigned zcstring::indexof(zcstring *right, unsigned index, bool last)
{

    unsigned last_mark = UINT_MAX;
    if (empty())
    {
        return last_mark;
    }
    if (size() < right->size())
    {
        return last_mark;
    }
    DIGGI_ASSERT(index < length);

    index += offset;
    unsigned n = 0, x = 0;
    DIGGI_ASSERT(right->mbuf->head->next == nullptr);
    DIGGI_ASSERT(right->mbuf->head != nullptr);
    DIGGI_ASSERT(right->size() > 0);
    DIGGI_ASSERT(right->offset == 0);
    DIGGI_ASSERT(mbuf);
    auto leftnode = mbuf_node_at_pos(mbuf, index, &x);
    DIGGI_ASSERT(leftnode != nullptr);

    auto right_data = right->mbuf->head->data;
    DIGGI_ASSERT(right_data != nullptr);
    bool jumpto = (x > 0) || (offset > 0) || (index > 0);

    while (true)
    {
        for (unsigned i = 0; i < leftnode->size; i++)
        {
            //Initial condition if start is not at begining of array
            if (jumpto)
            {
                i = index - x;
                jumpto = false;
            }

            if (((x + i + 1)) > length + offset)
            {
                break;
            }

            if (right_data[n] == leftnode->data[i])
            {
                n++;
                if (n == right->size())
                {
                    if (!last)
                    {
                        return ((x + i + 1) - n) - offset;
                    }
                    else
                    {
                        last_mark = (x + i + 1) - n;
                        n = 0;
                    }
                }
            }
            else
            {
                n = 0;
            }
        }
        x += leftnode->size;
        if ((leftnode->next == nullptr) || (x > length + offset))
        {
            break;
        }
        leftnode = leftnode->next;
    }

    if (last_mark == UINT_MAX)
    {
        /*
			not found condition
		*/
        return UINT_MAX;
    }

    return last_mark - offset;
}
/**
 * @brief check for occurence of character
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param c character to find
 * @param index index to start
 * @param last find last occurence
 * @return unsigned index of occurence, return UINT_MAX if not found
 */
unsigned zcstring::indexof(char c, unsigned index, bool last)
{
    ALIGNED_CHAR(8)
    r = c;
    zcstring strz(&r, 1);
    return indexof(&strz, index, last);
}

/**
 * @brief find the first of the last instance of any of the characters in input array
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param ar input array to search for
 * @param s size of character array
 * @param index index to start search
 * @return unsigned index of first of the last instance, return UINT_MAX if none are found
 */
unsigned zcstring::find_last_of(const char *ar, size_t s, unsigned index)
{
    unsigned int min = index + offset;
    for (unsigned int i = 0; i < s; i++)
    {
        auto temp = indexof(ar[i], index, true);
        min = (min < temp) ? temp : min;
    }
    return min;
}
/**
 * @brief find first instance instance of any of the characters in input array
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param ar input array to search for
 * @param s size of array
 * @param index index to start
 * @return unsigned index of first instance of character, return UINT_MAX if none are found
 */
unsigned zcstring::find_first_of(const char *ar, size_t s, unsigned index)
{
    unsigned int min = UINT_MAX;
    for (unsigned int i = 0; i < s; i++)
    {
        auto temp = indexof(ar[i], index, false);
        min = (min > temp) ? temp : min;
    }
    return min;
}
/**
 * @brief check if zcstring contains input zcstring
 * uses indexof internally
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param zcs 
 * @return true 
 * @return false 
 */
bool zcstring::contains(zcstring *zcs)
{
    return (indexof(zcs) != size());
}
/**
 * @brief compare two zcstrings
 * uses indexof() internally
 * input string must be physically contiguous due to implementation limitatins of indexof.
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param zc 
 * @return true 
 * @return false 
 */
bool zcstring::compare(zcstring *zc)
{
    if (zc->size() != size())
    {
        return false;
    }
    return (indexof(zc) == 0);
}
/**
 * @brief compare two strings, input STL string
 * uses indexof() internally
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param str 
 * @return true 
 * @return false 
 */
bool zcstring::compare(const std::string &str)
{
    return compare(str.c_str(), str.size());
}
/**
 * @brief compare two strings, input STL string
 * uses indexof() internally
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param str 
 * @return true 
 * @return false 
 */
bool zcstring::compare(std::string &str)
{
    return compare(str.c_str(), str.size());
}
/**
 * @brief compare to const char string
 * uses indexof() internally
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param zc 
 * @param size 
 * @return true 
 * @return false 
 */
bool zcstring::compare(const char *zc, size_t size)
{
    if (length != size)
    {
        return false;
    }
    zcstring converter(zc, size);
    return (indexof(&converter) == 0);
}
/**
 * @brief compare zcstrings
 * uses indexof() internally
 * @warning input string must be physically contigous die to limitations in indexof() implementation
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param c 
 * @return true 
 * @return false 
 */
bool zcstring::compare(zcstring &c)
{
    return (indexof(&c, 0, false) == 0);
}
/**
 * @brief compare const char string
 * uses indexof() internally
 * must be nullterminated.
 * @warning will incurr traversal of input string via strlen to discover length
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param zc 
 * @return true 
 * @return false 
 */
bool zcstring::compare(const char *zc)
{
    zcstring converter(zc);
    return (indexof(&converter) == 0);
}
/**
 * @brief equality operator overload to compare string litteral to zcstring
 * must be nullterminated
 * @warning will incurr traversal of input string via strlen to discover length
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param zc 
 * @return true 
 * @return false 
 */
bool zcstring::operator==(const char *zc)
{
    return compare(zc);
}
/**
 * @brief equality operator overload for zcstring
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param zc 
 * @return true 
 * @return false 
 */
bool zcstring::operator==(zcstring &zc)
{
    return compare(zc);
}
/**
 * @brief not-equality operator overload for string litteral
 * must be nullterminated
 * @warning will incurr traversal of input string via strlen to discover length
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param zc 
 * @return true 
 * @return false 
 */
bool zcstring::operator!=(const char *zc)
{
    return !compare(zc);
}
/**
 * @brief append const char to zcstring
 * must be nullterminated
 * @warning will incurr traversal of input string via strlen to discover length
 * @warning causes traversal, may be costly for fragmented zcstring

 * @param zc 
 */
void zcstring::append(const char *zc)
{
    DIGGI_ASSERT(reserved == 0);
    auto constlen = strlen(zc);
    DIGGI_ASSERT(constlen);
    auto out = mbuf_append_tail(mbuf, zc, constlen, length + offset);
    DIGGI_ASSERT(out == (length + offset));
    length += constlen;
}
/**
 * @brief append STL string to zcstring
 * 
 * @param str string by reference
 * @param type not used parameter
 */
void zcstring::append(std::string &str, zcstring_reference_type_t type)
{
    this->append((void *)str.c_str(), str.size(), type);
}
/**
 * @brief append raw buffer of given length to zcstring
 * @warning will incur copy operation, avoid using this.
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param ptr pointer to buffer.
 * @param len length of buffer
 * @param type unused parameter.
 */
void zcstring::append(void *ptr, size_t len, zcstring_reference_type_t type)
{
    auto dst = this->reserve(len);
    memcpy(dst, ptr, len);
    auto out = mbuf_append_tail(mbuf, (char *)dst, len, true, length + offset);
    DIGGI_ASSERT(out == (length + offset));
}
/**
 * @brief append buffer to zcstring
 * If allocated by zcstring::reserve(), clear internal pending reserve state.
 * added to mbuf as internally managed memory buffer (freed by mbuf).
 * If not allocated by zcstring::reserve(), buffer is managed externally.
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param zc buffer
 * @param len length of buffer.
 */
void zcstring::append(char *zc, size_t len)
{
    DIGGI_ASSERT(len != 0);

    if (reserved > 0)
    {
        DIGGI_ASSERT(len <= reserved);
        auto out = mbuf_append_tail(mbuf, zc, len, true, length + offset);
        DIGGI_ASSERT(out == (length + offset));
        reserved = 0;
    }
    else
    {
        auto out = mbuf_append_tail(mbuf, zc, len, length + offset);
        DIGGI_ASSERT(out == (length + offset));
    }
    length += len;
}
/**
 * @brief append constant STL string to zcstring buffer
 * by reference, no copy
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param str 
 */
void zcstring::append(const std::string &str)
{

    DIGGI_ASSERT(reserved == 0);
    auto out = mbuf_append_tail(mbuf, str.c_str(), str.size(), length + offset);
    DIGGI_ASSERT(out == (length + offset));
    length += str.size();
}
/**
 * @brief append zcstring buffer to zcstring
 * if appended zcstring has a non-zero offset, this method must duplciate mbuf at given offset.
 * @warning, cannot be appended if pending reserve.
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param str 
 */
void zcstring::append(zcstring &str)
{
    DIGGI_ASSERT(reserved == 0);
    //to remove prefix offset
    if (str.offset != 0)
    {
        mbuf_t placeholder;
        /*
			TODO: Refcounting is lost, 
			we must do copy of string
		 */
        placeholder.head = mbuf_duplicate_offset(str.mbuf, str.offset);
        auto replacement = zcstring(&placeholder, str.length);
        length += replacement.length;

        mbuf_concat(mbuf, replacement.mbuf);
    }
    else
    {
        length += str.length;
        mbuf_concat(mbuf, str.mbuf);
        /*not good for performance, adds one extra traversal of mbuf
			please fix
		*/
    }
}

/**
 * @brief get pointer to raw data at given offset
 * @warning Dangerous, expected chunk length may differ from retreved
 * @warning causes traversal, may be costly for fragmented zcstring
 * @param expected_chunk_length 
 * @return char* 
 */
char *zcstring::getptr(size_t expected_chunk_length)
{
    unsigned int base = 0;
    auto node = mbuf_node_at_pos(mbuf, offset, &base);
    DIGGI_ASSERT(node);
    DIGGI_ASSERT(base < offset);
    auto intra_mbuf_offt = offset - base;
    DIGGI_ASSERT(expected_chunk_length < (node->size - intra_mbuf_offt));
    return &node->data[intra_mbuf_offt];
}
/**
 * @brief get internal mbuf
 * 
 * @return mbuf_t* 
 */
mbuf_t *zcstring::getmbuf()
{
    return mbuf;
}
/**
 * @brief get offset of current zcstring(relative to underlying mbuf)
 * 
 * @return size_t 
 */
size_t zcstring::getoffset()
{
    return offset;
}
/**
 * @brief get substring extracted by range
 * does not copy.
 * cause an underlying incref of mbuf structures.
 * @warning does not clean up tail pointers, returning string might be longer than expected
 * @param index start index
 * @param size lengt of substring
 * @return zcstring 
 */
zcstring zcstring::substr(unsigned index, size_t size)
{

    if (length < size + index)
    {
        DIGGI_ASSERT(false);
        return zcstring();
    }
    return zcstring(mbuf, size, offset + index);
}
/**
 * @brief split string on index position
 * returns latter part
 * @param index index
 * @return zcstring latter part of string. from index and up.
 */
zcstring zcstring::substr(unsigned index)
{

    return substr(index, size() - index);
}
/**
 * @brief pop at tail of zcstring, by given count
 * deletes tail mbuf_nodes once pop completes a full contiguous chunk.
 * @warning causes traversal, may be costly for fragmented zcstring.
 * @param count 
 */
void zcstring::pop_back(size_t count)
{
    unsigned x;
    DIGGI_ASSERT(length > 0);
    auto node = mbuf_node_at_pos(mbuf, (offset + length) - count, &x);

    DIGGI_ASSERT(node->size > 0);
    DIGGI_ASSERT(node != nullptr);

    auto t_node = node->next;

    size_t clean = count;
    if (t_node != nullptr)
    {
        while (clean > 0 && t_node != nullptr)
        {
            auto tempo = t_node;
            t_node = t_node->next;
            clean -= tempo->size;
            mbuf_node_destroy(tempo, true);
        }
        node->next = nullptr;
        DIGGI_ASSERT(node->size >= clean);
        node->size -= clean;
    }
    else
    {
        DIGGI_ASSERT(node->size >= count);
        node->size -= count;
    }

    if (node->size == 0)
    {
        auto nodepres = mbuf_node_at_pos(mbuf, ((offset + length) - count) - 1, &x);
        mbuf_node_destroy(node, true);
        nodepres->next = nullptr;
    }

    length -= count;
}
/**
 * @brief pop "count" bytes from front of zcstring
 * concatenation of zcstrings which have offset values into mbuf_nodes require duplication, as detailed in zcstring::append
 * @param count 
 */
void zcstring::pop_front(size_t count)
{

    DIGGI_ASSERT(count <= length);
    unsigned traverse = count + offset;
    auto t_node = mbuf->head;
    DIGGI_ASSERT(t_node);
    unsigned x;
    auto node = mbuf_node_at_pos(mbuf, traverse, &x);

    mbuf->head = node;

    while (t_node != node && t_node != nullptr)
    {
        DIGGI_ASSERT(t_node);
        traverse -= t_node->size;
        DIGGI_ASSERT(length - t_node->size >= 0);
        auto tempo = t_node;
        t_node = t_node->next;
        mbuf_node_destroy(tempo, true);
    }

    length -= count;
    offset = traverse;
}


/**
 * @brief replace substring starting from beginning of zcstring.
 * @warning forces copy operation of input string.
 * @param str string to replace
 * @param len length of string
 */
void zcstring::replace(char *str, size_t len)
{
    DIGGI_ASSERT(len >= size());
    mbuf_replace(mbuf, length, offset, str);
}
/**
 * @brief serialize entire zcstring into a contiguous buffer.
 * 
 * @return char* 
 */
char *zcstring::copy()
{
    auto retval = (char *)malloc(size());
    mbuf_populate_buffer(mbuf, length, offset, retval);
    return retval;
}
/**
 * @brief serialize partially zcstring, into given buffer
 * 
 * @param str buffer to serialize into
 * @param len length of buffer        
 * @return char*         
 */
char *zcstring::copy(char *str, size_t len)
{
    DIGGI_ASSERT(len >= size());
    mbuf_populate_buffer(mbuf, length, offset, str);
    return str;
}
/**
 * @brief debug method to serialze into STL string
 * Provided as input to avoid redundant copy
 * 
 * @param str 
 */
void zcstring::tostring(std::string *str)
{

    mbuf_to_printable(mbuf, length, offset, str);
}

/**
 * @brief debug method ot serialize into STL string
 * @warning performs additional copy of entire buffer as return value. avoid using in production code.
 * @return std::string 
 */
std::string zcstring::tostring()
{

    std::string str;
    mbuf_to_printable(mbuf, length, offset, &str);

    return str;
}
