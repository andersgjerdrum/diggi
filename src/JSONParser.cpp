/**
 * @file jsonparser.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief Implementation of a functional Zero Copy(in-place) JSON parser, used throughout diggi as configuration storage and retrieval.
 * Uses zcstring as a virtual contiguous buffer to allow input buffer to be reference directly in json objects.
 * @version 0.1
 * @date 2020-02-04
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "JSONParser.h"
#include "messaging/Util.h"
#define MAGIC_PATTERN "<666420>"
/**
 * @brief Construct a new json node::json node object
 * Constructor of a json node, accepting a reference type of zcstring(virtual contiguous buffer) containing string representation of raw JSON
 * Will recursively parse sub clauses of json until done.
 * Default type of node is set to STRING, altered if subobjects.
 * @param raw 
 */
json_node::json_node(
    zcstring &raw) : type(STRING)
{
    parse(raw);
}

json_node::~json_node()
{
    children.clear();
}
/**
 * @brief Construct a new json node::json node object
 * Noop constructor of empty node, will not parse anything.
 */
json_node::json_node() : type(STRING) {}
/**
 * @brief Construct a new json node::json node object
 * Constructor of leaf node json, using key and value.
 * Will attempt rudimentary type detection.
 * @param key_i 
 * @param value_i 
 */
json_node::json_node(
    zcstring key_i,
    zcstring value_i) : type(STRING),
                        key(key_i),
                        value(value_i)
{
    type_detector(false);
}
/**
 * @brief internal parse method invoked on raw json string representation
 * Recursively invokes self until all clauses are leaf nodes.
 * @param raw virtual contiguous buffer representing sub json clause for parsing(initially entire json)
 */
void json_node::parse(zcstring &raw)
{
    bool colonobserved = false;
    bool bracketobserved = false;
    bool quoteobserved = false;
    bool string_hint = false;
    while (raw.size() != 0)
    {
        if (raw.find(MAGIC_PATTERN) == 0)
        {
            raw.pop_front(strlen(MAGIC_PATTERN));
            unsigned next_magic = raw.find(MAGIC_PATTERN);
            DIGGI_ASSERT(next_magic != UINT_MAX);
            DIGGI_ASSERT(next_magic != 0);
            value = raw.substr(strlen(MAGIC_PATTERN), next_magic - strlen(MAGIC_PATTERN));
            type = BLOB;
            raw.pop_front(next_magic + strlen(MAGIC_PATTERN));
            break;
        }
        if (!quoteobserved)
        {
            get_next_valid_char(raw);
        }

        if (raw[0] == '\"')
        {
            quoteobserved = !quoteobserved;
            if (colonobserved)
            {
                if (quoteobserved)
                {
                    string_hint = true;
                }
            }
        }
        else if (raw[0] == '[')
        {

            raw.pop_front(1);
            type = ARRAY;
            get_next_valid_char(raw);
            if (raw[0] != ']')
            {

                bracketobserved = true;
                children.push_back(json_node(raw));
                continue;
            }
        }
        else if (raw[0] == ']')
        {

            bracketobserved = false;
            if (type == ARRAY)
            {
                raw.pop_front(1);
            }

            if (!colonobserved)
            {
                /*Array element*/
                value = key;
                key = "";
            }
            break;
        }
        else if (raw[0] == '{')
        {

            raw.pop_front(1);
            type = JSON;
            get_next_valid_char(raw);

            if (raw[0] != '}')
            {
                bracketobserved = true;
                children.push_back(json_node(raw));
                continue;
            }
            else
            {
                raw.pop_front(1);
                break;
            }
        }
        else if (raw[0] == ',' && !bracketobserved)
        {

            if (!colonobserved)
            {
                /*Array element*/
                value = key;
                key = "";
            }
            break;
        }
        else if (raw[0] == ',' && bracketobserved)
        {

            raw.pop_front(1);
            children.push_back(json_node(raw));
            //it--;
            continue;
        }
        else if (raw[0] == '}')
        {
            bracketobserved = false;
            if (type == JSON)
            {
                raw.pop_front(1);
            }
            break;
        }
        else if (raw[0] == ':' && !colonobserved)
        {
            colonobserved = true;
        }
        else if (colonobserved && !bracketobserved)
        {
            unsigned next_bracket = 0;

            if (quoteobserved)
            {
                next_bracket = raw.find_first_of("\"", 1, 0);
            }
            else
            {
                next_bracket = raw.find_first_of("\0\r\n ,\"}]", 8, 0);
            }
            DIGGI_ASSERT(next_bracket < UINT_MAX);
            DIGGI_ASSERT(next_bracket > 0);
            value = raw.substr(0, next_bracket);
            raw.pop_front(next_bracket);
            continue;
        }
        else if (!colonobserved && !bracketobserved)
        {
            unsigned next_bracket = 0;

            if (quoteobserved)
            {
                next_bracket = raw.find_first_of("\"", 1, 0);
            }
            else
            {
                next_bracket = raw.find_first_of("\0\r\n ,\"}]", 8, 0);
            }
            DIGGI_ASSERT(next_bracket > 0);

            key = raw.substr(0, next_bracket);

            raw.pop_front(next_bracket);
            continue;
        }

        raw.pop_front(1);
    }
    type_detector(string_hint);
}
/**
 * @brief add child for JSON type node (non leaf)
 * 
 * @param node 
 */
void json_node::addChild(json_node node)
{
    children.push_back(node);
}
/**
 * @brief add child for JSON type node (non leaf)(operator overload)
 * 
 * @param input 
 * @return json_node& 
 */
json_node &json_node::operator<<(const json_node &input)
{
    addChild(input);
    return *this;
}
/**
 * @brief check children of JSON type node for key 
 * Index operator overload
 * @param input key to check for.
 * @return json_node return subnode (may be JSON type)
 */
json_node json_node::operator[](zcstring input)
{
    for (json_node &var : children)
    {
        if (var.key == input)
        {
            return var;
        }
    }
    return json_node();
}
/**
 * @brief check check for existence of JSON type node for key 
 * 
 * @param input key to check for.
 * @return true 
 * @return false 
 */
bool json_node::contains(zcstring input)
{
    for (json_node &var : children)
    {
        if (var.key == input)
        {
            return true;
        }
    }
    return false;
}
/**
 * @brief internal helper method for parsing, continues reading zcstring until alphanumeric character appears on head
 * 
 * @param it 
 */
void json_node::get_next_valid_char(zcstring &it)
{
    while (it[0] == ' ' || (it[0] == '\r') || (it[0] == '\n'))
    {
        it.pop_front(1);
    }
}
/**
 * @brief internal helper to detect type of leaf json node after parsing.
 * attempts to detect float, int or string. Cannot be JSON.
 * 
 * @param string_hint force string type
 */
void json_node::type_detector(bool string_hint)
{
    //printf("enter type_Detector()\n");
    bool dot = false,
         number = false,
         character = false;
    if (type == BLOB)
    {
        return;
    }
    if (string_hint)
    {
        type = STRING;
        return;
    }

    /*
		Not an optimal traversal technique, 
		causes uneccessary complexity
	*/
    number = (value.find_first_of("0123456789", 10, 0) != UINT_MAX);
    //printf("found number:%d\n", number);

    dot = (value.indexof('.', 0, false) != UINT_MAX);
    //printf("found dot:%d\n", dot);
    character = (value.find_first_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", 52, 0) != UINT_MAX);
    //printf("found character:%d\n", character);

    if (dot && number && !character)
    {
        type = FLOAT;
    }
    else if (number && !character && !dot)
    {
        type = INT;
    }

    //printf("key:%s,value:%s,type:%d\n", key.tostring().c_str(), value.tostring().c_str(), type);

    return;
}
/**
 * @brief serialize and return internal representation of json as string
 * 
 * @return std::string 
 */
std::string json_node::debug_get_string()
{
    zcstring ret;
    serialize(ret);
    return ret.tostring();
}
/**
 * @brief serialize a constructed json object as a virtual contugous buffer zcstring
 * Does not perform any copy operations as zcstring points directly to json object internal values.
 * 
 * @param retstring by-reference buffer returned to caller containing the serialized json
 */
void json_node::serialize(zcstring &retstring)
{
    ALIGNED_CONST_CHAR(8)
    backwards_slash[] = "\"";
    ALIGNED_CONST_CHAR(8)
    colon[] = ":";
    ALIGNED_CONST_CHAR(8)
    doublebrackets[] = "{ }";
    ALIGNED_CONST_CHAR(8)
    doublesqbrackets[] = "[ ]";
    ALIGNED_CONST_CHAR(8)
    lsqbrack[] = "[";
    ALIGNED_CONST_CHAR(8)
    rsqbrack[] = "]";
    ALIGNED_CONST_CHAR(8)
    lbrack[] = "{";
    ALIGNED_CONST_CHAR(8)
    rbrack[] = "}";
    ALIGNED_CONST_CHAR(8)
    comma[] = ",";

    if (type == JSON)
    {
        if (!key.empty())
        {
            retstring.append(backwards_slash);
            retstring.append(key);
            retstring.append(backwards_slash);
            retstring.append(colon);
        }

        if (children.size() == 0)
        {
            retstring.append(doublebrackets);
        }
        else
        {
            retstring.append(lbrack);

            for (json_node var : children)
            {
                var.serialize(retstring);
                retstring.append(comma);
            }

            retstring.pop_back(1);
            retstring.append(rbrack);
        }
    }
    else if (type == ARRAY)
    {
        if (!key.empty())
        {
            retstring.append(backwards_slash);
            retstring.append(key);
            retstring.append(backwards_slash);
            retstring.append(colon);
        }

        if (children.size() == 0)
        {
            retstring.append(doublesqbrackets);
        }
        else
        {
            retstring.append(lsqbrack);
            for (json_node var : children)
            {
                var.serialize(retstring);
                retstring.append(comma);
            }
            retstring.pop_back(1);
            retstring.append(rsqbrack);
        }
    }
    else if (key.empty() && !value.empty())
    {
        if (type == STRING)
        {
            retstring.append(backwards_slash);
            retstring.append(value);
            retstring.append(backwards_slash);
        }
        else if (type == BLOB)
        {
            retstring.append(MAGIC_PATTERN);
            retstring.append(value);
            retstring.append(MAGIC_PATTERN);
        }
        else
        {
            retstring.append(value);
        }
    }
    else
    {

        retstring.append(backwards_slash);
        retstring.append(key);
        retstring.append(backwards_slash);
        retstring.append(colon);

        if (type == STRING)
        {
            retstring.append(backwards_slash);
            if (!value.empty())
            {
                retstring.append(value);
            }
            retstring.append(backwards_slash);
        }
        else if (type == BLOB)
        {
            retstring.append(MAGIC_PATTERN);
            if (!value.empty())
            {
                retstring.append(value);
            }
            retstring.append(MAGIC_PATTERN);
        }
        else
        {

            retstring.append(value);
        }
    }
}

/**
 * @brief not implemented, should support patern matching
 * 
 * @param node 
 * @return true 
 * @return false 
 */
bool json_match_object::match_pattern(json_node &node)
{
    //TODO: implement sampleregex matching
    return false;
}
/**
 * @brief check json_node for type
 * 
 * @param node 
 * @return true 
 * @return false 
 */
bool json_match_object::match_type(json_node &node)
{
    return (node.type == type);
}
/**
 * @brief not implemented, shold match for range
 * 
 * @param node 
 * @return true 
 * @return false 
 */
bool json_match_object::match_range(json_node &node)
{
    //TODO: implement range
    return false;
}
/**
 * @brief test json node for key match.
 * 
 * @param node 
 * @return true 
 * @return false 
 */
bool json_match_object::match_key(json_node &node)
{
    return node.key.compare(key);
}
/**
 * @brief Construct a new json match object::json match object object
 * match object used as filter for json object. json nodes input to methods of this object are tested against filter.
 * not used by anything in diggi, an idea which was abandoned. however tested in Unit tests.
 * @param t 
 * @param exclusive 
 */
json_match_object::json_match_object(
    unsigned int t,
    bool exclusive) : mtype(t),
                      strict(exclusive) {}
/**
 * @brief set key filter
 * 
 * @param searchkey 
 */
void json_match_object::set_key(zcstring searchkey)
{
    key = searchkey;
}
/**
 * @brief set type filter
 * 
 * @param lookfortype 
 */
void json_match_object::set_type(json_type lookfortype)
{
    type = lookfortype;
}
/**
 * @brief set pattern filter
 * 
 * @param lookforpattern 
 */
void json_match_object::set_pattern(zcstring lookforpattern)
{
    json_match_object::pattern = lookforpattern;
}
/**
 * @brief set range filter, not implemented
 * 
 * @param from 
 * @param to 
 */
void json_match_object::set_range(float from, float to)
{

    //TODO:
}
/**
 * @brief match node against match object(filter)
 * 
 * @param node 
 * @return true 
 * @return false 
 */
bool json_match_object::match(json_node &node)
{
    unsigned int return_match = 0;
    if (((mtype & PATTERN) == PATTERN) && (match_pattern(node)))
    {
        // printf("match pattern\n");
        return_match |= PATTERN;
    }
    if (((mtype & TYPE) == TYPE) && (match_type(node)))
    {
        //printf("match type\n");
        return_match |= TYPE;
    }
    if (((mtype & RANGE) == RANGE) && (match_range(node)))
    {
        // printf("match range\n");
        return_match |= RANGE;
    }
    if (((mtype & KEY) == KEY) && (match_key(node)))
    {
        //printf("match key\n");
        return_match |= KEY;
    }
    if (strict)
    {
        return (mtype == return_match);
    }
    else
    {
        return (return_match > 0);
    }
}
/**
 * @brief Construct a new json traversal::json traversal object
 * traversal object used to traverse json tree and apply matching objects(filter)
 * Not used by anything in diggi, although tested
 * @param matching_parameters 
 */
json_traversal::json_traversal(
    json_match_object matching_parameters) : matching_parameters(matching_parameters) {}
/**
 * @brief match against json object recursively
 * 
 * @param nd 
 */
void json_traversal::match(json_node &nd)
{
    for (json_node var : nd.children)
    {
        match(var);

        if (matching_parameters.match(var))
        {
            // printf("got a match!\n");
            results.push_back(var);
        }
    }
}
/**
 * @brief Construct a new json document::json document object
 * all-up json document.
 * Holds reference to raw zcstring buffer and root json node.
 * @param input 
 */
json_document::json_document(
    zcstring input) : head(input), raw(input) {}
/**
 * @brief Construct a new json document::json document object
 * construct empty json_document, for construction.
 * @param node_o 
 */
json_document::json_document(
    json_node node_o) : head(node_o) {}
/**
 * @brief check if json document is empty
 * 
 * @return true 
 * @return false 
 */
bool json_document::empty()
{
    return head.children.size() == 0;
}
/**
 * @brief traverse json document with matching object(filter)
 * 
 * @param traversalobject 
 */
void json_document::traverse(json_traversal &traversalobject)
{
    traversalobject.match(head);
}
/**
 * @brief retrieve string based on internal representation
 * 
 * @return std::string 
 */
std::string json_document::debug_get_string()
{
    return head.debug_get_string();
}
/**
 * @brief retrieve raw representation(basis for parsing)
 * 
 * @return zcstring 
 */
zcstring json_document::get_string()
{
    return raw;
}
