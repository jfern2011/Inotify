
#include <iostream>

#include "Inotify.h"

class InotifyTest
{
	typedef std::list<Inotify::InotifyEvent> evt_list;

public:

	InotifyTest()
	{
	}

	~InotifyTest()
	{
	}

	bool run_api_test(const char* dir)
	{
		Inotify inotify;
		int wd = inotify.add_watch(dir, IN_CREATE | IN_DELETE);

		AbortIf(wd == -1, false);

		/*
		 * Create and delete a directory:
		 */
		std::string cmd1("mkdir ");
		std::string cmd2("rm -r ");
		cmd1 += dir;
		cmd2 += dir;
		system((cmd1 + "/test_dir").c_str());

		sleep(2);

		system((cmd2 + "/test_dir").c_str());

		sleep(2);

		evt_list list;
		inotify.poll(list);

		for (auto iter = list.begin(), end = list.end();
			 iter != end; ++iter)
				Inotify::print(*iter);


		/*
		 * Update the watch to respond only to deletions:
		 */
		AbortIf(inotify.add_watch(dir, IN_DELETE) == -1,
			false);

		cmd1 = "mkdir ";
		cmd2 = "rm -r ";
		cmd1 += dir;
		cmd2 += dir;
		system((cmd1 + "/test_dir2").c_str());

		sleep(2);

		system((cmd2 + "/test_dir2").c_str());

		sleep(2);

		list.clear();
		inotify.poll(list);
		for (auto iter = list.begin(), end = list.end();
			 iter != end; ++iter)
				Inotify::print(*iter);

		AbortIfNot(inotify.rm_watch(wd), false);

		AbortIf(inotify.add_watch(dir,
					IN_CREATE | IN_DELETE) == -1, false);

		AbortIfNot(inotify.rm_watch(dir), false);

		/*
		 * If the watch was successfully removed, this
		 * shouldn't do anything:
		 */
		std::cout << "expecting no events..."
			<< std::endl;

		cmd1 = "mkdir ";
		cmd2 = "rm -r ";
		cmd1 += dir;
		cmd2 += dir;
		system((cmd1 + "/test_dir3").c_str());

		sleep(2);

		system((cmd2 + "/test_dir3").c_str());

		sleep(2);

		list.clear();
		inotify.poll(list);
		for (auto iter = list.begin(), end = list.end();
			 iter != end; ++iter)
				Inotify::print(*iter);

		return true;
	}

	bool run_callback_test1(const char* dir1, const char* dir2)
	{
		Inotify inotify;
		int wd1 = inotify.add_watch(dir1, IN_CREATE | IN_DELETE);
		int wd2 = inotify.add_watch(dir2, IN_CREATE | IN_DELETE | IN_MODIFY);

		AbortIf(wd1 == -1 || wd2 == -1, false);

		/*
		 * 1. Provide coverage of 3 of the 6 attach() functions
		 * 2. Attach() with "compound" events
		 */
		AbortIfNot((inotify.attach<void>(wd1, IN_CREATE,
			           *this, &InotifyTest::create_callback)), false);

		AbortIfNot((inotify.attach<void>(wd1, IN_DELETE,
			           *this, &InotifyTest::delete_callback)), false);

		AbortIfNot((inotify.attach<void>(wd1, IN_CREATE | IN_DELETE,
				*this, &InotifyTest::create_delete_callback)), false);

		AbortIfNot((inotify.attach<void>(wd2, IN_MODIFY,
							  &InotifyTest::modify_callback)), false);

		/*
		 * Create directories. This will raise the 1st and 3rd signals
		 * two times:
		 */
		std::string cmd1("mkdir ");
		std::string cmd2("rm -r ");
		cmd1 += dir1;
		cmd2 += dir1;
		system((cmd1 + "/subdir1").c_str());
		system((cmd1 + "/subdir2").c_str());

		sleep(2);

		inotify.poll(wd1);

		/*
		 * Remove directories. This will raise the 2nd and 3rd signals
		 * two times:
		 */
		system((cmd2 + "/subdir1").c_str());
		system((cmd2 + "/subdir2").c_str());

		sleep(2);

		inotify.poll(wd1);

		std::cout << "expecting no events..."
			<< std::endl;

		inotify.poll();

		/*
		 * Modify a file in the 2nd directory. This will raise the 4th
		 * signal:
		 */
		cmd1 = "uname > ";
		cmd1 += dir2;
		system((cmd1 + "/test1.txt").c_str());
		sleep(1);

		std::cout << "raising..." << std::endl;
		inotify.poll(wd2);

		/*
		 * Update the 2nd watch's signal handler
		 */
		AbortIfNot(inotify.detach(wd2,IN_MODIFY), false);
		AbortIfNot((inotify.attach<void>(wd2, IN_CREATE | IN_DELETE | IN_MODIFY,
							  &InotifyTest::modify_callback)), false);

		std::cout << "...\n";

		/*
		 * Create, modify, and delete test2.txt. This will raise the 4th
		 * signal three times:
		 */
		cmd1 = "uname > ";
		cmd1 += dir2;
		system((cmd1 + "/test2.txt").c_str());
		sleep(1);

		cmd1 = "touch ";
		cmd1 += dir2;
		system((cmd1 + "/test2.txt").c_str());
		sleep(1);

		cmd1 = "rm ";
		cmd1 += dir2;
		system((cmd1 + "/test2.txt").c_str());
		sleep(1);

		inotify.poll();

		/*
		 * Change the 2nd watch's signal handler
		 */
		// Note: detach() not required here
		AbortIfNot((inotify.attach<void>(wd2, IN_CREATE | IN_DELETE | IN_MODIFY,
					*this, &InotifyTest::create_delete_callback)), false);

		std::cout << "...\n";

		/*
		 * Create, modify, and delete test2.txt. This will raise the 4th
		 * signal three times:
		 */
		cmd1 = "uname > ";
		cmd1 += dir2;
		system((cmd1 + "/test2.txt").c_str());
		sleep(1);

		cmd1 = "touch ";
		cmd1 += dir2;
		system((cmd1 + "/test2.txt").c_str());
		sleep(1);

		cmd1 = "rm ";
		cmd1 += dir2;
		system((cmd1 + "/test2.txt").c_str());
		sleep(1);

		inotify.poll();

		return true;
	}

	bool run_callback_test2(const char* dir1, const char* dir2)
	{
		Inotify inotify;
		int wd1 = inotify.add_watch(dir1, IN_CREATE | IN_DELETE | IN_MODIFY);
		int wd2 = inotify.add_watch(dir2, IN_CREATE | IN_DELETE | IN_MODIFY);

		AbortIf(wd1 == -1 || wd2 == -1, false);

		/*
		 * 1. Provide coverage of 3 of the 6 attach() functions
		 * 2. Attach() with "compound" events
		 */
		AbortIfNot((inotify.attach<InotifyTest&,InotifyTest,const std::string&, int>(wd1,
			IN_CREATE,
			*this, &InotifyTest::create_callback, "A", 2)), false);

		AbortIfNot((inotify.attach<bool,InotifyTest,std::string,void*>(wd1,
			IN_DELETE,
			*this, &InotifyTest::delete_callback, "B", NULL)), false);


		AbortIfNot((inotify.attach<void,InotifyTest,std::string,bool>(wd1,
				IN_CREATE | IN_DELETE,
				*this, &InotifyTest::create_delete_callback, "C", true)), false);

		std::string tmp = "D";
		AbortIfNot((inotify.attach<void,std::string&>(wd2,
			IN_MODIFY,
			&InotifyTest::modify_callback, tmp)), false);

		/*
		 * Create directories. This will raise the 1st and 3rd signals
		 * two times:
		 *
		 * Expecting the following:
		 *
		 * 1. create_callback(A,2)
		 * 2. create_delete_callback(C,true)
		 * 3. create_callback(A,2)
		 * 4. create_delete_callback(C,true)
		 */
		std::string cmd1("mkdir ");
		std::string cmd2("rm -r ");
		cmd1 += dir1;
		cmd2 += dir1;
		system((cmd1 + "/subdir1").c_str());
		system((cmd1 + "/subdir2").c_str());

		sleep(2);

		/*
		 * Remove directories. This will raise the 2nd and 3rd signals
		 * two times:
		 *
		 * Expecting the following:
		 *
		 * 5. delete_callback(B,NULL)
		 * 6. create_delete_callback(C,true)
		 * 7. delete_callback(B,NULL)
		 * 8. create_delete_callback(C,true)
		 */
		system((cmd2 + "/subdir1").c_str());
		system((cmd2 + "/subdir2").c_str());

		sleep(2);

		/*
		 * Modify a file in the 2nd directory. This will raise the 4th
		 * signal:
		 *
		 * Expecting the following:
		 *
		 * 1. modify_callback(D)
		 */
		cmd1 = "uname > ";
		cmd1 += dir2;
		system((cmd1 + "/test1.txt").c_str());
		sleep(2);

		inotify.poll(wd1);

		std::cout << "..." << std::endl;

		inotify.poll(wd2);

		/*
		 * Update the 1st watch's signal mask
		 *
		 * Note: Currently, wd1 has 3 handlers:
		 *
		 * 1. create_callback (IN_CREATE)
		 * 2. delete_callback (IN_DELETE)
		 * 3. create_delete_callback (IN_CREATE | IN_DELETE)
		 *
		 * After this, we'll have:
		 *
		 * 1. create_callback (IN_MODIFY)
		 * 2. delete_callback (IN_DELETE)
		 * 3. create_delete_callback (IN_CREATE | IN_DELETE)
		 */
		AbortIfNot(inotify.detach(wd1,IN_CREATE), false);
		AbortIfNot((inotify.attach<InotifyTest&,InotifyTest,const std::string&, int>(wd1,
			IN_MODIFY,
			*this, &InotifyTest::create_callback, "E", 3)), false);

		std::cout << "..." << std::endl;

		/*
		 * Create, modify, and delete test1.txt. This will raise the
		 * following:
		 * 
		 * 1. create_delete_callback(C,true)
		 * 2. create_callback(E,3)
		 * 3. delete_callback(B,NULL)
		 * 4. create_delete_callback(C,true)
		 */
		cmd1 = "uname > ";
		cmd1 += dir1;
		system((cmd1 + "/test1.txt").c_str());
		sleep(1);

		cmd1 = "touch ";
		cmd1 += dir1;
		system((cmd1 + "/test1.txt").c_str());
		sleep(1);

		cmd1 = "rm ";
		cmd1 += dir1;
		system((cmd1 + "/test1.txt").c_str());
		sleep(1);

		std::cout << "expecting 4 signals..."
			<< std::endl;

		inotify.poll();

		/*
		 * Change the 1st watch's signal handler
		 *
		 * Note: Currently, wd1 has 3 handlers:
		 *
		 * 1. create_callback (IN_MODIFY)
		 * 2. delete_callback (IN_DELETE)
		 * 3. create_delete_callback (IN_CREATE | IN_DELETE)
		 *
		 * After this, we'll have:
		 *
		 * 1. modify_callback (IN_MODIFY)
		 * 2. delete_callback (IN_DELETE)
		 * 3. create_delete_callback (IN_CREATE | IN_DELETE)
		 */
		// Note: detach() not required here
		tmp = "F";
		// 2nd template arg not required!
		AbortIfNot((inotify.attach<void>(wd1, 
			IN_MODIFY,
			&InotifyTest::modify_callback, tmp)), false);
		
		std::cout << "...\n";

		/*
		 * Create, modify, and delete test1.txt. This will raise the
		 * following:
		 * 
		 * 1. create_delete_callback(C,true)
		 * 2. modify_callback(F)
		 * 3. delete_callback(B,NULL)
		 * 4. create_delete_callback(C,true)
		 */
		cmd1 = "uname > ";
		cmd1 += dir1;
		system((cmd1 + "/test1.txt").c_str());
		sleep(1);

		cmd1 = "touch ";
		cmd1 += dir1;
		system((cmd1 + "/test1.txt").c_str());
		sleep(1);

		cmd1 = "rm ";
		cmd1 += dir1;
		system((cmd1 + "/test1.txt").c_str());
		sleep(1);

		std::cout << "expecting 4 signals..."
			<< std::endl;

		inotify.poll();

		return true;
	}

	void create_callback()
	{
		std::cout << __PRETTY_FUNCTION__
			<< std::endl;
	}

	void delete_callback() const
	{
		std::cout << __PRETTY_FUNCTION__
			<< std::endl;
	}

	static void modify_callback()
	{
		std::cout << __PRETTY_FUNCTION__
			<< std::endl;
	}

	void create_delete_callback()
	{
		std::cout << __PRETTY_FUNCTION__
			<< std::endl;
	}

	InotifyTest& create_callback(const std::string& arg1, int arg2)
	{
		std::cout << __PRETTY_FUNCTION__
			<< ": ARG1 = " << arg1 << ", ARG2 = " << arg2
			<< std::endl;
		return *this;
	}

	bool delete_callback(std::string arg1, void* arg2) const
	{
		std::cout << __PRETTY_FUNCTION__
			<< ": ARG1 = " << arg1 << ", ARG2 = " << arg2
			<< std::endl;
		return true;
	}

	static void modify_callback(std::string& arg1)
	{
		std::cout << __PRETTY_FUNCTION__
			<< ": ARG1 = " << arg1 << std::endl;
	}

	void create_delete_callback(std::string arg1, bool arg2)
	{
		std::cout << __PRETTY_FUNCTION__
			<< ": ARG1 = " << arg1 << ", ARG2 = " << arg2
			<< std::endl;
	}
};

int main()
{
	InotifyTest test;
	std::string dir1 = "/home/jason/Projects/inotify/testDir1/";
	std::string dir2 = "/home/jason/Projects/inotify/testDir2/";

	test.run_api_test(dir1.c_str());

	test.run_callback_test1(dir1.c_str(),
							dir2.c_str());

	test.run_callback_test2(dir1.c_str(),
							dir2.c_str());

	return 0;
}
