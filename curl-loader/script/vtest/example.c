#include "vtest.h"

void always_fails()
{ 
}

void always_pass()
{
	VASSERT( 1 == 0 );
}

/*
extern VTEST_TEST_SUITE *get_vtest_TEST()
{
  static VTEST_TEST_SUITE test;
  static VTEST_TEST test_cases[] = {
		{ "kuku", (VTEST_ACTION) always_fails, 1 }, 	
		{ 0, 0, 0 }
	};
  test.test_cases = (VTEST_TEST *) test_cases;
  test.next_suite = 0;

  return &test;
}
*/

VTEST_DEFINE_SUITE( FIRSTTEST, 0, 0, LASTTEST)
	VTEST_TEST( "alwayspass", always_pass)
	VTEST_TEST( "failtest", always_fails)
	VTEST_TEST( "alwayspass", always_pass)
VTEST_END_SUITE

VTEST_DEFINE_LAST_SUITE( LASTTEST, 0, 0)
	VTEST_TEST( "failtest", always_fails)
	VTEST_TEST( "alwayspass", always_pass)
VTEST_END_SUITE
