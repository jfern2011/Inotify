#include "Fd.h"

#include <cstdio>
#include <utility>

int main()
{
	Fd fd1;
	if (fd1)  std::printf("error1\n");

	fd1.reset(1);

	if (!fd1) std::printf("error2\n");

	Fd fd2(std::move(fd1));

	if (fd1)  std::printf("error3\n");

	if (!fd2) std::printf("error4\n");

	Fd fd3 = std::move(fd1);

	if (fd3)  std::printf("error5\n");

	fd3 = std::move(fd2);

	if (!fd3) std::printf("error6\n");

	if (fd3.release() != 1)
		std::printf("error7\n");

	if (fd3)  std::printf("error8\n");

	fd3.reset(2);

	if (!fd3) std::printf("error9\n");

	fd1.reset(0);

	fd1.swap(fd3);

	if (fd1.get() != 2)
		std::printf("error10\n");

	if (fd3.get() != 0)
		std::printf("error11\n");

	std::printf("tests complete.\n");
	std::fflush(stdout);
	return 0;
}
