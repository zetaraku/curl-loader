#ifndef __VTEST_CUI_H_
#define __VTEST_CUI_H_

#ifdef  __cplusplus
extern "C" {
#endif

void VTEST_CUI_test_runner(VTEST_TEST_SUITE *suite);

void VTEST_CUI_test_runner_cmdline(VTEST_TEST_SUITE *suite, int argc, const char *argv[]);

#ifdef  __cplusplus
}
#endif

#endif

