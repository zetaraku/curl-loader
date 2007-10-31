#ifndef _VTESTRUNNER_H_
#define _VTESTRUNNER_H_

typedef enum {
  VTEST_STATUS_OK  = 1,
  VTEST_STATUS_FAILED = 0,
}
  VTEST_STATUS;

typedef void (*VTEST_RUNNER_report_test_suite) (const char *suite_name, int is_start);
typedef void (*VTEST_RUNNER_report_test_start) (const char *suite_name, const char *test_name, int iteration);
typedef void (*VTEST_RUNNER_report_test_result)(const char *suite_name, const char *test_name, VTEST_STATUS status, const char *fail_cond, const char *fail_file,int fail_line);
typedef void (*VTEST_RUNNER_report_wrapup)	   (int tests_passed, int tests_failed);

typedef struct tagVTEST_RUNNER_IMPL {
	
	VTEST_RUNNER_report_test_suite  suite_start_finish;
	VTEST_RUNNER_report_test_start  test_start;
	VTEST_RUNNER_report_test_result test_result;
	VTEST_RUNNER_report_wrapup      wrapup;

	int				  current_test_state;
	VTEST_TEST_SUITE *suite;
	VTEST_TEST		 *test;

} VTEST_RUNNER_IMPL;

void VTEST_test_runner(VTEST_TEST_SUITE *suite, VTEST_RUNNER_IMPL *impl);


#endif