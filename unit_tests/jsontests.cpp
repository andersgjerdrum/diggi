

#include "JSONParser.h"
#include <string.h>
#include <gtest/gtest.h>
#include <string>
#include <fstream>
#include <streambuf>
using namespace std;

static string exampleDataPoint = "{"
   " \"header\": {"
        "\"id\": \"123e4567-e89b-12d3-a456-426655440000\","
       " \"creation_date_time\": \"2013-02-05T07:25:00Z\","
        "\"schema_id\": {"
            "\"namespace\": \"omh\","
            "\"name\": \"physical-activity\","
            "\"version\": 1.0"
      "  },"
       " \"acquisition_provenance\": {"
            "\"source_name\": \"RunKeeper\","
            "\"modality\": \"sensed\""
        "},"
        "\"user_id\": \"joe\""
   " },"
    "\"body\": {"
        "\"activity_name\": \"walking\","
        "\"distance\": {"
            "\"value\": 1.5,"
            "\"unit\": \"mi\","
            "\"version\": \"mi\""
		"},"
        "\"reported_activity_intensity\": \"moderate\","
        "\"effective_time_frame\": {"
            "\"time_interval\": {"
                "\"date\": \"2013-02-05\","
                "\"part_of_day\": \"morning\""
            "}"
        "}"
    "}"
"}";

TEST(jsontests,jsonparsertraversalsinglematch)
{

	zcstring zcstr(exampleDataPoint);
	json_document doc(zcstr);
	json_match_object matcher(TYPE, false);
	matcher.set_type(FLOAT);
	json_traversal traversal(matcher);
	doc.traverse(traversal);
	auto zcstr1 = doc.debug_get_string();
	EXPECT_TRUE(traversal.results.size() == 2);
}

TEST(jsontests,jsonparsertraversalrestrictivematch)
{

	zcstring zcstr(exampleDataPoint);
	json_document doc(zcstr);
	json_match_object matcher(KEY | TYPE, true);
	matcher.set_key("version");
	matcher.set_type(FLOAT);
	json_traversal traversal(matcher);
	doc.traverse(traversal);
	EXPECT_TRUE(traversal.results.size() == 1);
}

TEST(jsontests,jsonparseregressiontest_cannothandlenewline_coredump)
{
    string test = "F1\r\n"
    "{\r\n"
    "\"access_token\": \"35233d18-f22b-4dda-bcbf-5bdb85a0e761\",\r\n"
    "\"token_type\": \"bearer\",\r\n"
    "\"refresh_token\": \"e4197cbb-4957-48d6-8c0c-9b0f9a9c211f\",\r\n"
    "\"expires_in\": 28067,\r\n"
    "\"scope\": \"delete_data_points read_data_points write_data_points\"\r\n"
    "}\r\n";
	zcstring zcstr(test);

	json_document doc(zcstr);

    json_match_object matcher(KEY | TYPE, true);
    matcher.set_key("access_token");
    matcher.set_type(STRING);
    json_traversal traversal(matcher);
    doc.traverse(traversal);

    EXPECT_TRUE(traversal.results.size() == 1);

    string expected = "35233d18-f22b-4dda-bcbf-5bdb85a0e761";
    EXPECT_TRUE(expected == traversal.results[0].value.tostring());
}

TEST(jsontests,jsonparser_stringify_traverse_test)
{
    ifstream test("unit_tests/testdata/json_array_input.json");
    string str((std::istreambuf_iterator<char>(test)),
                 std::istreambuf_iterator<char>());
	zcstring zcstr(str);
    json_document doc(zcstr);

    ifstream test1("unit_tests/testdata/json_array_expected.json");
    string str1((std::istreambuf_iterator<char>(test1)),
                 std::istreambuf_iterator<char>());
    EXPECT_TRUE(str1.compare(doc.debug_get_string()) == 0);
    json_match_object matcher(KEY, false);
    matcher.set_key("body");
    json_traversal traversal(matcher);
    doc.traverse(traversal);
    EXPECT_TRUE(traversal.results.size() == 1);
}
TEST(jsontests,jsonparser_largejsondatatest)
{
    ifstream test("unit_tests/testdata/json_large.json");
    string str((std::istreambuf_iterator<char>(test)),
                 std::istreambuf_iterator<char>());

	zcstring zcstr(str);
	json_document doc(zcstr);
	json_match_object matcher(KEY, false);
    matcher.set_key("body");
    json_traversal traversal(matcher);
    doc.traverse(traversal);
    EXPECT_TRUE(traversal.results.size() == 15);
}

TEST(jsontests,jsonparser_typetest)
{
    ifstream test("unit_tests/testdata/json_typetesting.json");
    string str((std::istreambuf_iterator<char>(test)),
                 std::istreambuf_iterator<char>());

	zcstring zcstr(str);
	json_document doc(zcstr);
	vector<json_node> res = doc.head.children[0].children;
    EXPECT_TRUE(res[0].type == STRING);
    EXPECT_TRUE(res[1].type == INT);
    EXPECT_TRUE(res[2].type == INT);
    EXPECT_TRUE(res[3].type == STRING);
    EXPECT_TRUE(res[4].type == INT);
    EXPECT_TRUE(res[5].type == FLOAT);
    EXPECT_TRUE(res[6].type == STRING);
}

TEST(jsontests,jsoncompose_test)
{
    json_node schema_node;
    schema_node.key = "schema_id";
    schema_node.type = JSON;
    schema_node << json_node("namespace","omh");
    string str = schema_node.debug_get_string();
    string expected = "\"schema_id\":{\"namespace\":\"omh\"}";
    EXPECT_TRUE(expected == str);
}

TEST(jsontests,string_instansiation)
{
    zcstring json = "\"schema_id\":{\"namespace\":\"lol\",\"name\":\"lol\",\"version\":{\"major\":1,\"minor\":0,\"qualifier\":\"null\"}}";

	json_document node(json);

    string res = node.debug_get_string();
    EXPECT_TRUE(res == json.tostring());
}

TEST(jsontests,string_not_integer)
{
	zcstring json = "\"schema_id\":{\"namespace\":\"bam\",\"name\":\"trololo\",\"version\":\"1.0\"}";
	json_document node(json);
	auto res = node.debug_get_string();
	EXPECT_TRUE(res== json.tostring());
}

TEST(jsontests, regression_arrays_with_square_brackets_simple_elements) {
	ifstream test("unit_tests/testdata/json_list_regression.json");
	string str((std::istreambuf_iterator<char>(test)),
		std::istreambuf_iterator<char>());
	zcstring zcstr(str);
	json_document doc(zcstr);
	EXPECT_TRUE(str.compare(doc.debug_get_string()) == 0);
}

TEST(jsontests, array_elements_test)
{
	ifstream test("unit_tests/testdata/json_array_elements.json");
	string str((std::istreambuf_iterator<char>(test)),
		std::istreambuf_iterator<char>());
	zcstring zcstr(str);

	json_node doc(zcstr);
	EXPECT_TRUE(doc["elements"].children[0].value == "one");
	EXPECT_TRUE(doc["elements"].children[1].value == "two");
	EXPECT_TRUE(doc["elements"].children[2].value == "three");
	EXPECT_TRUE(doc["elements"].children[3].value == "four");
	EXPECT_TRUE(doc["elements"].children[4].value == "five");



} 

TEST(jsontests, multi_array_elements_test)
{
	ifstream test("unit_tests/testdata/json_multiarrayregression.json");
	string str((std::istreambuf_iterator<char>(test)),
		std::istreambuf_iterator<char>());
	zcstring zcstr(str);

	json_node doc(zcstr);
	EXPECT_TRUE(str.compare(doc.debug_get_string()) == 0);
}



TEST(jsontests, configuration_subobject_regression_test)
{
	ifstream test("unit_tests/testdata/conf.json");
	string str((std::istreambuf_iterator<char>(test)),
		std::istreambuf_iterator<char>());
	zcstring zcstr(str);

	json_node doc(zcstr);

	EXPECT_TRUE(doc["structure"].children[1].key == "process-2");

}