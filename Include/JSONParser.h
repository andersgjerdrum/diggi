#ifndef JSONPARSER_H
#define JSONPARSER_H

#include <vector>

#include "ZeroCopyString.h"

enum json_type
{
	INT,
	FLOAT,
	STRING,
    BLOB,
	UINT,
	URL,
	DATETIME,
	JSON, //value is children
	ARRAY,
};


class json_node
{
public:
	json_type				type;
	zcstring				key;
	zcstring				value;
	std::vector<json_node>	children;
	json_node();
	json_node(zcstring key_i, zcstring value_i);
	json_node(zcstring &raw);
    ~json_node();

	void parse(zcstring &raw);

	void addChild(json_node node);

	json_node& operator<<(const json_node& input);
    bool contains(zcstring input);

	json_node operator[](zcstring input);

	void get_next_valid_char(zcstring &it);
	void type_detector(bool string_hint);
	std::string debug_get_string();
	void serialize(zcstring &retstring);
};

enum  json_match_type: unsigned int
{
	PATTERN = 1,
	TYPE = 2,
	RANGE = 4,
	KEY = 8,

};

class json_match_object
{

	json_type type;
	zcstring pattern;
	unsigned int mtype;
	zcstring key;
	bool strict;
	bool match_pattern(json_node &node);

	bool match_type(json_node &node);

	bool match_range(json_node &node);

	bool match_key(json_node &node);

public:
	json_match_object(unsigned int t, bool exclusive);
	void set_key(zcstring searchkey);
	void set_type(json_type lookfortype);
	void set_pattern(zcstring lookforpattern);
	void set_range(float from, float to);
	bool match(json_node &node);
};


class json_traversal
{
public: 
	json_match_object matching_parameters;
	std::vector<json_node> results;

	json_traversal(json_match_object matching_parameters);
	void match(json_node &nd);
};

class json_document
{
public:
	json_node head;
	zcstring raw;
	json_document(zcstring input);
	json_document(json_node node_o);
	bool empty();
	void traverse(json_traversal &traversalobject);
	std::string debug_get_string();
	zcstring get_string();

};

#endif
