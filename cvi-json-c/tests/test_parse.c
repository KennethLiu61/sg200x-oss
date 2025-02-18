#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cvi_json.h"
#include "cvi_json_tokener.h"
#include "cvi_json_visit.h"

static void test_basic_parse(void);
static void test_utf8_parse(void);
static void test_verbose_parse(void);
static void test_incremental_parse(void);

int main(void)
{
	MC_SET_DEBUG(1);

	static const char separator[] = "==================================";
	test_basic_parse();
	puts(separator);
	test_utf8_parse();
	puts(separator);
	test_verbose_parse();
	puts(separator);
	test_incremental_parse();
	puts(separator);

	return 0;
}

static cvi_json_c_visit_userfunc clear_serializer;
static void do_clear_serializer(cvi_json_object *jso);

static void single_incremental_parse(const char *test_string, int clear_serializer)
{
	int ii;
	int chunksize = atoi(getenv("TEST_PARSE_CHUNKSIZE"));
	struct cvi_json_tokener *tok;
	enum cvi_json_tokener_error jerr;
	cvi_json_object *all_at_once_obj, *new_obj;
	const char *all_at_once_str, *new_str;

	assert(chunksize > 0);
	all_at_once_obj = cvi_json_tokener_parse(test_string);
	if (clear_serializer)
		do_clear_serializer(all_at_once_obj);
	all_at_once_str = cvi_json_object_to_cvi_json_string(all_at_once_obj);

	tok = cvi_json_tokener_new();
	int test_string_len = strlen(test_string) + 1; // Including '\0' !
	for (ii = 0; ii < test_string_len; ii += chunksize)
	{
		int len_to_parse = chunksize;
		if (ii + chunksize > test_string_len)
			len_to_parse = test_string_len - ii;

		if (getenv("TEST_PARSE_DEBUG") != NULL)
			printf(" chunk: %.*s\n", len_to_parse, &test_string[ii]);
		new_obj = cvi_json_tokener_parse_ex(tok, &test_string[ii], len_to_parse);
		jerr = cvi_json_tokener_get_error(tok);
		if (jerr != cvi_json_tokener_continue || new_obj)
			break;
	}
	if (clear_serializer && new_obj)
		do_clear_serializer(new_obj);
	new_str = cvi_json_object_to_cvi_json_string(new_obj);

	if (strcmp(all_at_once_str, new_str) != 0)
	{
		printf("ERROR: failed to parse (%s) in %d byte chunks: %s != %s\n", test_string,
		       chunksize, all_at_once_str, new_str);
	}
	cvi_json_tokener_free(tok);
	if (all_at_once_obj)
		cvi_json_object_put(all_at_once_obj);
	if (new_obj)
		cvi_json_object_put(new_obj);
}

static void single_basic_parse(const char *test_string, int clear_serializer)
{
	cvi_json_object *new_obj;

	new_obj = cvi_json_tokener_parse(test_string);
	if (clear_serializer)
		do_clear_serializer(new_obj);
	printf("new_obj.to_string(%s)=%s\n", test_string, cvi_json_object_to_cvi_json_string(new_obj));
	cvi_json_object_put(new_obj);

	if (getenv("TEST_PARSE_CHUNKSIZE") != NULL)
		single_incremental_parse(test_string, clear_serializer);
}
static void test_basic_parse()
{
	single_basic_parse("\"\003\"", 0);
	single_basic_parse("/* hello */\"foo\"", 0);
	single_basic_parse("// hello\n\"foo\"", 0);
	single_basic_parse("\"foo\"blue", 0);
	single_basic_parse("\'foo\'", 0);
	single_basic_parse("\"\\u0041\\u0042\\u0043\"", 0);
	single_basic_parse("\"\\u4e16\\u754c\\u00df\"", 0);
	single_basic_parse("\"\\u4E16\"", 0);
	single_basic_parse("\"\\u4e1\"", 0);
	single_basic_parse("\"\\u4e1@\"", 0);
	single_basic_parse("\"\\ud840\\u4e16\"", 0);
	single_basic_parse("\"\\ud840\"", 0);
	single_basic_parse("\"\\udd27\"", 0);
	// Test with a "short" high surrogate
	single_basic_parse("[9,'\\uDAD", 0);
	single_basic_parse("null", 0);
	single_basic_parse("NaN", 0);
	single_basic_parse("-NaN", 0); /* non-sensical, returns null */

	single_basic_parse("Inf", 0); /* must use full string, returns null */
	single_basic_parse("inf", 0); /* must use full string, returns null */
	single_basic_parse("Infinity", 0);
	single_basic_parse("infinity", 0);
	single_basic_parse("-Infinity", 0);
	single_basic_parse("-infinity", 0);
	single_basic_parse("{ \"min\": Infinity, \"max\": -Infinity}", 0);

	single_basic_parse("Infinity!", 0);
	single_basic_parse("Infinitynull", 0);
	single_basic_parse("InfinityXXXX", 0);
	single_basic_parse("-Infinitynull", 0);
	single_basic_parse("-InfinityXXXX", 0);
	single_basic_parse("Infinoodle", 0);
	single_basic_parse("InfinAAA", 0);
	single_basic_parse("-Infinoodle", 0);
	single_basic_parse("-InfinAAA", 0);

	single_basic_parse("True", 0);
	single_basic_parse("False", 0);

	/* not case sensitive */
	single_basic_parse("tRue", 0);
	single_basic_parse("fAlse", 0);
	single_basic_parse("nAn", 0);
	single_basic_parse("iNfinity", 0);

	single_basic_parse("12", 0);
	single_basic_parse("12.3", 0);

	/* Even though, when using cvi_json_tokener_parse() there's no way to
	 *  know when there is more data after the parsed object,
	 *  an object is successfully returned anyway (in some cases)
	 */

	single_basic_parse("12.3.4", 0);
	single_basic_parse("2015-01-15", 0);
	single_basic_parse("12.3xxx", 0);
	single_basic_parse("12.3{\"a\":123}", 0);
	single_basic_parse("12.3\n", 0);
	single_basic_parse("12.3 ", 0);

	single_basic_parse("{\"FoO\"  :   -12.3E512}", 0);
	single_basic_parse("{\"FoO\"  :   -12.3e512}", 0);
	single_basic_parse("{\"FoO\"  :   -12.3E51.2}", 0);   /* non-sensical, returns null */
	single_basic_parse("{\"FoO\"  :   -12.3E512E12}", 0); /* non-sensical, returns null */
	single_basic_parse("[\"\\n\"]", 0);
	single_basic_parse("[\"\\nabc\\n\"]", 0);
	single_basic_parse("[null]", 0);
	single_basic_parse("[]", 0);
	single_basic_parse("[false]", 0);
	single_basic_parse("[\"abc\",null,\"def\",12]", 0);
	single_basic_parse("{}", 0);
	single_basic_parse("{ \"foo\": \"bar\" }", 0);
	single_basic_parse("{ \'foo\': \'bar\' }", 0);
	single_basic_parse("{ \"foo\": \"bar\", \"baz\": null, \"bool0\": true }", 0);
	single_basic_parse("{ \"foo\": [null, \"foo\"] }", 0);
	single_basic_parse("{ \"abc\": 12, \"foo\": \"bar\", \"bool0\": false, \"bool1\": true, "
	                   "\"arr\": [ 1, 2, 3, null, 5 ] }",
	                   0);
	single_basic_parse("{ \"abc\": \"blue\nred\\ngreen\" }", 0);

	// Clear serializer for these tests so we see the actual parsed value.
	single_basic_parse("null", 1);
	single_basic_parse("false", 1);
	single_basic_parse("[0e]", 1);
	single_basic_parse("[0e+]", 1);
	single_basic_parse("[0e+-1]", 1);
	single_basic_parse("\"hello world!\"", 1);

	// uint64/int64 range test
	single_basic_parse("[9223372036854775806]", 1);
	single_basic_parse("[9223372036854775807]", 1);
	single_basic_parse("[9223372036854775808]", 1);
	single_basic_parse("[-9223372036854775807]", 1);
	single_basic_parse("[-9223372036854775808]", 1);
	single_basic_parse("[-9223372036854775809]", 1);
	single_basic_parse("[18446744073709551614]", 1);
	single_basic_parse("[18446744073709551615]", 1);
	single_basic_parse("[18446744073709551616]", 1);
}

static void test_utf8_parse()
{
	// cvi_json_tokener_parse doesn't support checking for byte order marks.
	// It's the responsibility of the caller to detect and skip a BOM.
	// Both of these checks return null.
	char *utf8_bom = "\xEF\xBB\xBF";
	char *utf8_bom_and_chars = "\xEF\xBB\xBF{}";
	single_basic_parse(utf8_bom, 0);
	single_basic_parse(utf8_bom_and_chars, 0);
}

// Clear the re-serialization information that the tokener
// saves to ensure that the output reflects the actual
// values we parsed, rather than just the original input.
static void do_clear_serializer(cvi_json_object *jso)
{
	cvi_json_c_visit(jso, 0, clear_serializer, NULL);
}

static int clear_serializer(cvi_json_object *jso, int flags, cvi_json_object *parent_jso,
                            const char *jso_key, size_t *jso_index, void *userarg)
{
	if (jso)
		cvi_json_object_set_serializer(jso, NULL, NULL, NULL);
	return JSON_C_VISIT_RETURN_CONTINUE;
}

static void test_verbose_parse()
{
	cvi_json_object *new_obj;
	enum cvi_json_tokener_error error = cvi_json_tokener_success;

	new_obj = cvi_json_tokener_parse_verbose("{ foo }", &error);
	assert(error == cvi_json_tokener_error_parse_object_key_name);
	assert(new_obj == NULL);

	new_obj = cvi_json_tokener_parse("{ foo }");
	assert(new_obj == NULL);

	new_obj = cvi_json_tokener_parse("foo");
	assert(new_obj == NULL);
	new_obj = cvi_json_tokener_parse_verbose("foo", &error);
	assert(new_obj == NULL);

	/* b/c the string starts with 'f' parsing return a boolean error */
	assert(error == cvi_json_tokener_error_parse_boolean);

	puts("cvi_json_tokener_parse_verbose() OK");
}

struct incremental_step
{
	const char *string_to_parse;
	int length;
	int char_offset;
	enum cvi_json_tokener_error expected_error;
	int reset_tokener; /* Set to 1 to call cvi_json_tokener_reset() after parsing */
	int tok_flags;     /* JSON_TOKENER_* flags to pass to cvi_json_tokener_set_flags() */
} incremental_steps[] = {

    /* Check that full cvi_json messages can be parsed, both w/ and w/o a reset */
    {"{ \"foo\": 123 }", -1, -1, cvi_json_tokener_success, 0},
    {"{ \"foo\": 456 }", -1, -1, cvi_json_tokener_success, 1},
    {"{ \"foo\": 789 }", -1, -1, cvi_json_tokener_success, 1},

    /* Check the comment parse*/
    {"/* hello */{ \"foo\"", -1, -1, cvi_json_tokener_continue, 0},
    {"/* hello */:/* hello */", -1, -1, cvi_json_tokener_continue, 0},
    {"\"bar\"/* hello */", -1, -1, cvi_json_tokener_continue, 0},
    {"}/* hello */", -1, -1, cvi_json_tokener_success, 1},
    {"/ hello ", -1, 1, cvi_json_tokener_error_parse_comment, 1},
    {"/* hello\"foo\"", -1, -1, cvi_json_tokener_continue, 1},
    {"/* hello*\"foo\"", -1, -1, cvi_json_tokener_continue, 1},
    {"// hello\"foo\"", -1, -1, cvi_json_tokener_continue, 1},

    /*  Check a basic incremental parse */
    {"{ \"foo", -1, -1, cvi_json_tokener_continue, 0},
    {"\": {\"bar", -1, -1, cvi_json_tokener_continue, 0},
    {"\":13}}", -1, -1, cvi_json_tokener_success, 1},

    /* Check the UTF-16 surrogate pair handling in various ways.
	 * Note: \ud843\udd1e is u+1D11E, Musical Symbol G Clef
	 * Your terminal may not display these correctly, in particular
	 *  PuTTY doesn't currently show this character.
	 */
    /* parse one char at every time */
    {"\"\\", -1, -1, cvi_json_tokener_continue, 0},
    {"u", -1, -1, cvi_json_tokener_continue, 0},
    {"d", -1, -1, cvi_json_tokener_continue, 0},
    {"8", -1, -1, cvi_json_tokener_continue, 0},
    {"3", -1, -1, cvi_json_tokener_continue, 0},
    {"4", -1, -1, cvi_json_tokener_continue, 0},
    {"\\", -1, -1, cvi_json_tokener_continue, 0},
    {"u", -1, -1, cvi_json_tokener_continue, 0},
    {"d", -1, -1, cvi_json_tokener_continue, 0},
    {"d", -1, -1, cvi_json_tokener_continue, 0},
    {"1", -1, -1, cvi_json_tokener_continue, 0},
    {"e\"", -1, -1, cvi_json_tokener_success, 1},
    /* parse two char at every time */
    {"\"\\u", -1, -1, cvi_json_tokener_continue, 0},
    {"d8", -1, -1, cvi_json_tokener_continue, 0},
    {"34", -1, -1, cvi_json_tokener_continue, 0},
    {"\\u", -1, -1, cvi_json_tokener_continue, 0},
    {"dd", -1, -1, cvi_json_tokener_continue, 0},
    {"1e\"", -1, -1, cvi_json_tokener_success, 1},
    /* check the low surrogate pair */
    {"\"\\ud834", -1, -1, cvi_json_tokener_continue, 0},
    {"\\udd1e\"", -1, -1, cvi_json_tokener_success, 1},
    {"\"\\ud834\\", -1, -1, cvi_json_tokener_continue, 0},
    {"udd1e\"", -1, -1, cvi_json_tokener_success, 1},
    {"\"\\ud834\\u", -1, -1, cvi_json_tokener_continue, 0},
    {"dd1e\"", -1, -1, cvi_json_tokener_success, 1},
    {"\"fff \\ud834\\ud", -1, -1, cvi_json_tokener_continue, 0},
    {"d1e bar\"", -1, -1, cvi_json_tokener_success, 1},
    {"\"fff \\ud834\\udd", -1, -1, cvi_json_tokener_continue, 0},
    {"1e bar\"", -1, -1, cvi_json_tokener_success, 1},

    /* \ud83d\ude00 is U+1F600, Grinning Face
	 * Displays fine in PuTTY, though you may need "less -r"
	 */
    {"\"fff \\ud83d\\ude", -1, -1, cvi_json_tokener_continue, 0},
    {"00 bar\"", -1, -1, cvi_json_tokener_success, 1},

    /* Check that cvi_json_tokener_reset actually resets */
    {"{ \"foo", -1, -1, cvi_json_tokener_continue, 1},
    {": \"bar\"}", -1, 0, cvi_json_tokener_error_parse_unexpected, 1},

    /* Check incremental parsing with trailing characters */
    {"{ \"foo", -1, -1, cvi_json_tokener_continue, 0},
    {"\": {\"bar", -1, -1, cvi_json_tokener_continue, 0},
    {"\":13}}XXXX", 10, 6, cvi_json_tokener_success, 0},
    {"XXXX", 4, 0, cvi_json_tokener_error_parse_unexpected, 1},

    /* Check that trailing characters can change w/o a reset */
    {"{\"x\": 123 }\"X\"", -1, 11, cvi_json_tokener_success, 0},
    {"\"Y\"", -1, -1, cvi_json_tokener_success, 1},

    /* Trailing characters should cause a failure in strict mode */
    {"{\"foo\":9}{\"bar\":8}", -1, 9, cvi_json_tokener_error_parse_unexpected, 1, JSON_TOKENER_STRICT},

    /* ... unless explicitly allowed. */
    {"{\"foo\":9}{\"bar\":8}", -1, 9, cvi_json_tokener_success, 0,
     JSON_TOKENER_STRICT | JSON_TOKENER_ALLOW_TRAILING_CHARS},
    {"{\"b\":8}ignored garbage", -1, 7, cvi_json_tokener_success, 1,
     JSON_TOKENER_STRICT | JSON_TOKENER_ALLOW_TRAILING_CHARS},

    /* To stop parsing a number we need to reach a non-digit, e.g. a \0 */
    {"1", 1, 1, cvi_json_tokener_continue, 0},
    /* This should parse as the number 12, since it continues the "1" */
    {"2", 2, 1, cvi_json_tokener_success, 0},
    {"12{", 3, 2, cvi_json_tokener_success, 1},
    /* Parse number in strict mode */
    {"[02]", -1, 3, cvi_json_tokener_error_parse_number, 1, JSON_TOKENER_STRICT},

    {"0e+0", 5, 4, cvi_json_tokener_success, 1},
    {"[0e+0]", -1, -1, cvi_json_tokener_success, 1},

    /* The behavior when missing the exponent varies slightly */
    {"0e", 2, 2, cvi_json_tokener_continue, 1},
    {"0e", 3, 2, cvi_json_tokener_success, 1},
    {"0e", 3, 2, cvi_json_tokener_error_parse_eof, 1, JSON_TOKENER_STRICT},
    {"[0e]", -1, -1, cvi_json_tokener_success, 1},
    {"[0e]", -1, 3, cvi_json_tokener_error_parse_number, 1, JSON_TOKENER_STRICT},

    {"0e+", 3, 3, cvi_json_tokener_continue, 1},
    {"0e+", 4, 3, cvi_json_tokener_success, 1},
    {"0e+", 4, 3, cvi_json_tokener_error_parse_eof, 1, JSON_TOKENER_STRICT},
    {"[0e+]", -1, -1, cvi_json_tokener_success, 1},
    {"[0e+]", -1, 4, cvi_json_tokener_error_parse_number, 1, JSON_TOKENER_STRICT},

    {"0e-", 3, 3, cvi_json_tokener_continue, 1},
    {"0e-", 4, 3, cvi_json_tokener_success, 1},
    {"0e-", 4, 3, cvi_json_tokener_error_parse_eof, 1, JSON_TOKENER_STRICT},
    {"[0e-]", -1, -1, cvi_json_tokener_success, 1},
    {"[0e-]", -1, 4, cvi_json_tokener_error_parse_number, 1, JSON_TOKENER_STRICT},

    /* You might expect this to fail, but it won't because
	   it's a valid partial parse; note the char_offset: */
    {"0e+-", 5, 3, cvi_json_tokener_success, 1},
    {"0e+-", 5, 3, cvi_json_tokener_error_parse_number, 1, JSON_TOKENER_STRICT},
    {"[0e+-]", -1, 4, cvi_json_tokener_error_parse_number, 1},

    /* Similar tests for other kinds of objects: */
    /* These could all return success immediately, since regardless of
	   what follows the false/true/null token we *will* return a cvi_json object,
       but it currently doesn't work that way.  hmm... */
    {"false", 5, 5, cvi_json_tokener_continue, 1},
    {"false", 6, 5, cvi_json_tokener_success, 1},
    {"true", 4, 4, cvi_json_tokener_continue, 1},
    {"true", 5, 4, cvi_json_tokener_success, 1},
    {"null", 4, 4, cvi_json_tokener_continue, 1},
    {"null", 5, 4, cvi_json_tokener_success, 1},

    {"Infinity", 9, 8, cvi_json_tokener_success, 1},
    {"infinity", 9, 8, cvi_json_tokener_success, 1},
    {"-infinity", 10, 9, cvi_json_tokener_success, 1},
    {"infinity", 9, 0, cvi_json_tokener_error_parse_unexpected, 1, JSON_TOKENER_STRICT},
    {"-infinity", 10, 1, cvi_json_tokener_error_parse_unexpected, 1, JSON_TOKENER_STRICT},

    {"inf", 3, 3, cvi_json_tokener_continue, 0},
    {"inity", 6, 5, cvi_json_tokener_success, 1},
    {"-inf", 4, 4, cvi_json_tokener_continue, 0},
    {"inity", 6, 5, cvi_json_tokener_success, 1},

    {"i", 1, 1, cvi_json_tokener_continue, 0},
    {"n", 1, 1, cvi_json_tokener_continue, 0},
    {"f", 1, 1, cvi_json_tokener_continue, 0},
    {"i", 1, 1, cvi_json_tokener_continue, 0},
    {"n", 1, 1, cvi_json_tokener_continue, 0},
    {"i", 1, 1, cvi_json_tokener_continue, 0},
    {"t", 1, 1, cvi_json_tokener_continue, 0},
    {"y", 1, 1, cvi_json_tokener_continue, 0},
    {"", 1, 0, cvi_json_tokener_success, 1},

    {"-", 1, 1, cvi_json_tokener_continue, 0},
    {"inf", 3, 3, cvi_json_tokener_continue, 0},
    {"ini", 3, 3, cvi_json_tokener_continue, 0},
    {"ty", 3, 2, cvi_json_tokener_success, 1},

    {"-", 1, 1, cvi_json_tokener_continue, 0},
    {"i", 1, 1, cvi_json_tokener_continue, 0},
    {"nfini", 5, 5, cvi_json_tokener_continue, 0},
    {"ty", 3, 2, cvi_json_tokener_success, 1},

    {"-i", 2, 2, cvi_json_tokener_continue, 0},
    {"nfinity", 8, 7, cvi_json_tokener_success, 1},

    {"InfinityX", 10, 8, cvi_json_tokener_success, 0},
    {"X", 1, 0, cvi_json_tokener_error_parse_unexpected, 1},

    {"Infinity1234", 13, 8, cvi_json_tokener_success, 0},
    {"1234", 5, 4, cvi_json_tokener_success, 1},

    {"Infinity9999", 8, 8, cvi_json_tokener_continue, 0},

    /* returns the Infinity loaded up by the previous call: */
    {"1234", 5, 0, cvi_json_tokener_success, 0},
    {"1234", 5, 4, cvi_json_tokener_success, 1},

    /* offset=1 because "n" is the start of "null".  hmm... */
    {"noodle", 7, 1, cvi_json_tokener_error_parse_null, 1},
    /* offset=2 because "na" is the start of "nan".  hmm... */
    {"naodle", 7, 2, cvi_json_tokener_error_parse_null, 1},
    /* offset=2 because "tr" is the start of "true".  hmm... */
    {"track", 6, 2, cvi_json_tokener_error_parse_boolean, 1},
    {"fail", 5, 2, cvi_json_tokener_error_parse_boolean, 1},

    /* Although they may initially look like they should fail,
	 * the next few tests check that parsing multiple sequential
	 * cvi_json objects in the input works as expected
	 */
    {"null123", 8, 4, cvi_json_tokener_success, 0},
    {&"null123"[4], 4, 3, cvi_json_tokener_success, 1},
    {"nullx", 6, 4, cvi_json_tokener_success, 0},
    {&"nullx"[4], 2, 0, cvi_json_tokener_error_parse_unexpected, 1},
    {"{\"a\":1}{\"b\":2}", 15, 7, cvi_json_tokener_success, 0},
    {&"{\"a\":1}{\"b\":2}"[7], 8, 7, cvi_json_tokener_success, 1},

    /*
	 * Though this may seem invalid at first glance, it
	 * parses as three separate numbers, 2015, -1 and -15
	 * Of course, simply pasting together a stream of arbitrary
	 * positive numbers won't work, since there'll be no way to
     * tell where in e.g. "2015015" the next number stats, so
	 * a reliably parsable stream must not include cvi_json_type_int
	 * or cvi_json_type_double objects without some other delimiter.
	 * e.g. whitespace
	 */
    {&"2015-01-15"[0], 11, 4, cvi_json_tokener_success, 1},
    {&"2015-01-15"[4], 7, 3, cvi_json_tokener_success, 1},
    {&"2015-01-15"[7], 4, 3, cvi_json_tokener_success, 1},
    {&"2015 01 15"[0], 11, 5, cvi_json_tokener_success, 1},
    {&"2015 01 15"[4], 7, 4, cvi_json_tokener_success, 1},
    {&"2015 01 15"[7], 4, 3, cvi_json_tokener_success, 1},

    /* Strings have a well defined end point, so we can stop at the quote */
    {"\"blue\"", -1, -1, cvi_json_tokener_success, 0},

    /* Check each of the escape sequences defined by the spec */
    {"\"\\\"\"", -1, -1, cvi_json_tokener_success, 0},
    {"\"\\\\\"", -1, -1, cvi_json_tokener_success, 0},
    {"\"\\b\"", -1, -1, cvi_json_tokener_success, 0},
    {"\"\\f\"", -1, -1, cvi_json_tokener_success, 0},
    {"\"\\n\"", -1, -1, cvi_json_tokener_success, 0},
    {"\"\\r\"", -1, -1, cvi_json_tokener_success, 0},
    {"\"\\t\"", -1, -1, cvi_json_tokener_success, 0},
    {"\"\\/\"", -1, -1, cvi_json_tokener_success, 0},
    // Escaping a forward slash is optional
    {"\"/\"", -1, -1, cvi_json_tokener_success, 0},
    /* Check wrong escape sequences */
    {"\"\\a\"", -1, 2, cvi_json_tokener_error_parse_string, 1},

    /* Check '\'' in strict model */
    {"\'foo\'", -1, 0, cvi_json_tokener_error_parse_unexpected, 1, JSON_TOKENER_STRICT},

    /* Parse array/object */
    {"[1,2,3]", -1, -1, cvi_json_tokener_success, 0},
    {"[1,2,3}", -1, 6, cvi_json_tokener_error_parse_array, 1},
    {"{\"a\"}", -1, 4, cvi_json_tokener_error_parse_object_key_sep, 1},
    {"{\"a\":1]", -1, 6, cvi_json_tokener_error_parse_object_value_sep, 1},
    {"{\"a\"::1}", -1, 5, cvi_json_tokener_error_parse_unexpected, 1},
    {"{\"a\":}", -1, 5, cvi_json_tokener_error_parse_unexpected, 1},
    {"{\"a\":1,\"a\":2}", -1, -1, cvi_json_tokener_success, 1},
    {"\"a\":1}", -1, 3, cvi_json_tokener_success, 1},
    {"{\"a\":1", -1, -1, cvi_json_tokener_continue, 1},
    {"[,]", -1, 1, cvi_json_tokener_error_parse_unexpected, 1},
    {"[,1]", -1, 1, cvi_json_tokener_error_parse_unexpected, 1},

    /* This behaviour doesn't entirely follow the cvi_json spec, but until we have
	 * a way to specify how strict to be we follow Postel's Law and be liberal
	 * in what we accept (up to a point).
	 */
    {"[1,2,3,]", -1, -1, cvi_json_tokener_success, 0},
    {"[1,2,,3,]", -1, 5, cvi_json_tokener_error_parse_unexpected, 0},

    {"[1,2,3,]", -1, 7, cvi_json_tokener_error_parse_unexpected, 1, JSON_TOKENER_STRICT},
    {"{\"a\":1,}", -1, 7, cvi_json_tokener_error_parse_unexpected, 1, JSON_TOKENER_STRICT},

    // utf-8 test
    // acsll encoding
    {"\x22\x31\x32\x33\x61\x73\x63\x24\x25\x26\x22", -1, -1, cvi_json_tokener_success, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    {"\x22\x31\x32\x33\x61\x73\x63\x24\x25\x26\x22", -1, -1, cvi_json_tokener_success, 1},
    // utf-8 encoding
    {"\x22\xe4\xb8\x96\xe7\x95\x8c\x22", -1, -1, cvi_json_tokener_success, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    {"\x22\xe4\xb8", -1, 3, cvi_json_tokener_error_parse_utf8_string, 0, JSON_TOKENER_VALIDATE_UTF8},
    {"\x96\xe7\x95\x8c\x22", -1, 0, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    {"\x22\xe4\xb8\x96\xe7\x95\x8c\x22", -1, -1, cvi_json_tokener_success, 1},
    {"\x22\xcf\x80\xcf\x86\x22", -1, -1, cvi_json_tokener_success, 1, JSON_TOKENER_VALIDATE_UTF8},
    {"\x22\xf0\xa5\x91\x95\x22", -1, -1, cvi_json_tokener_success, 1, JSON_TOKENER_VALIDATE_UTF8},
    // wrong utf-8 encoding
    {"\x22\xe6\x9d\x4e\x22", -1, 3, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    {"\x22\xe6\x9d\x4e\x22", -1, 5, cvi_json_tokener_success, 1},
    // GBK encoding
    {"\x22\xc0\xee\xc5\xf4\x22", -1, 2, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    {"\x22\xc0\xee\xc5\xf4\x22", -1, 6, cvi_json_tokener_success, 1},
    // char after space
    {"\x20\x20\x22\xe4\xb8\x96\x22", -1, -1, cvi_json_tokener_success, 1, JSON_TOKENER_VALIDATE_UTF8},
    {"\x20\x20\x81\x22\xe4\xb8\x96\x22", -1, 2, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    {"\x5b\x20\x81\x31\x5d", -1, 2, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    // char in state inf
    {"\x49\x6e\x66\x69\x6e\x69\x74\x79", 9, 8, cvi_json_tokener_success, 1},
    {"\x49\x6e\x66\x81\x6e\x69\x74\x79", -1, 3, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    // char in escape unicode
    {"\x22\x5c\x75\x64\x38\x35\x35\x5c\x75\x64\x63\x35\x35\x22", 15, 14, cvi_json_tokener_success, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    {"\x22\x5c\x75\x64\x38\x35\x35\xc0\x75\x64\x63\x35\x35\x22", -1, 8,
     cvi_json_tokener_error_parse_utf8_string, 1, JSON_TOKENER_VALIDATE_UTF8},
    {"\x22\x5c\x75\x64\x30\x30\x33\x31\xc0\x22", -1, 9, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    // char in number
    {"\x31\x31\x81\x31\x31", -1, 2, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},
    // char in object
    {"\x7b\x22\x31\x81\x22\x3a\x31\x7d", -1, 3, cvi_json_tokener_error_parse_utf8_string, 1,
     JSON_TOKENER_VALIDATE_UTF8},

    {NULL, -1, -1, cvi_json_tokener_success, 0},
};

static void test_incremental_parse()
{
	cvi_json_object *new_obj;
	enum cvi_json_tokener_error jerr;
	struct cvi_json_tokener *tok;
	const char *string_to_parse;
	int ii;
	int num_ok, num_error;

	num_ok = 0;
	num_error = 0;

	printf("Starting incremental tests.\n");
	printf("Note: quotes and backslashes seen in the output here are literal values passed\n");
	printf("     to the parse functions.  e.g. this is 4 characters: \"\\f\"\n");

	string_to_parse = "{ \"foo"; /* } */
	printf("cvi_json_tokener_parse(%s) ... ", string_to_parse);
	new_obj = cvi_json_tokener_parse(string_to_parse);
	if (new_obj == NULL)
		puts("got error as expected");

	/* test incremental parsing in various forms */
	tok = cvi_json_tokener_new();
	for (ii = 0; incremental_steps[ii].string_to_parse != NULL; ii++)
	{
		int this_step_ok = 0;
		struct incremental_step *step = &incremental_steps[ii];
		int length = step->length;
		size_t expected_char_offset;

		cvi_json_tokener_set_flags(tok, step->tok_flags);

		if (length == -1)
			length = (int)strlen(step->string_to_parse);
		if (step->char_offset == -1)
			expected_char_offset = length;
		else
			expected_char_offset = step->char_offset;

		printf("cvi_json_tokener_parse_ex(tok, %-12s, %3d) ... ", step->string_to_parse,
		       length);
		new_obj = cvi_json_tokener_parse_ex(tok, step->string_to_parse, length);

		jerr = cvi_json_tokener_get_error(tok);
		if (step->expected_error != cvi_json_tokener_success)
		{
			if (new_obj != NULL)
				printf("ERROR: invalid object returned: %s\n",
				       cvi_json_object_to_cvi_json_string(new_obj));
			else if (jerr != step->expected_error)
				printf("ERROR: got wrong error: %s\n",
				       cvi_json_tokener_error_desc(jerr));
			else if (cvi_json_tokener_get_parse_end(tok) != expected_char_offset)
				printf("ERROR: wrong char_offset %zu != expected %zu\n",
				       cvi_json_tokener_get_parse_end(tok), expected_char_offset);
			else
			{
				printf("OK: got correct error: %s\n",
				       cvi_json_tokener_error_desc(jerr));
				this_step_ok = 1;
			}
		}
		else
		{
			if (new_obj == NULL &&
			    !(step->length >= 4 && strncmp(step->string_to_parse, "null", 4) == 0))
				printf("ERROR: expected valid object, instead: %s\n",
				       cvi_json_tokener_error_desc(jerr));
			else if (cvi_json_tokener_get_parse_end(tok) != expected_char_offset)
				printf("ERROR: wrong char_offset %zu != expected %zu\n",
				       cvi_json_tokener_get_parse_end(tok), expected_char_offset);
			else
			{
				printf("OK: got object of type [%s]: %s\n",
				       cvi_json_type_to_name(cvi_json_object_get_type(new_obj)),
				       cvi_json_object_to_cvi_json_string(new_obj));
				this_step_ok = 1;
			}
		}

		if (new_obj)
			cvi_json_object_put(new_obj);

		if (step->reset_tokener & 1)
			cvi_json_tokener_reset(tok);

		if (this_step_ok)
			num_ok++;
		else
			num_error++;
	}

	cvi_json_tokener_free(tok);

	printf("End Incremental Tests OK=%d ERROR=%d\n", num_ok, num_error);
}
