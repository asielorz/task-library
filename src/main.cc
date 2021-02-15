#define CATCH_CONFIG_RUNNER
#include "catch/catch.hpp"

int main()
{
	int const tests_result = Catch::Session().run();
	if (tests_result != 0)
		system("pause");
}
