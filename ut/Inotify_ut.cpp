#include <csignal>
#include <map>
#include <sys/inotify.h>

#include "abort/abort.h"
#include "src/Inotify.h"

bool quit = false;

void sig_handler(int n)
{
	quit = true;
	std::printf("caught signal %d.\n", n);
	std::fflush(stdout);
}

bool handle_input(const Inotify::Event& event)
{
	std::map<uint32_t, std::string> int2str;

	int2str[IN_ACCESS]        = "IN_ACCESS";
	int2str[IN_ATTRIB]        = "IN_ATTRIB";
	int2str[IN_CLOSE_WRITE]   = "IN_CLOSE_WRITE";
	int2str[IN_CLOSE_NOWRITE] = "IN_CLOSE_NOWRITE";
	int2str[IN_CREATE]        = "IN_CREATE";
	int2str[IN_DELETE]        = "IN_DELETE";
	int2str[IN_DELETE_SELF]   = "IN_DELETE_SELF";
	int2str[IN_MODIFY]        = "IN_MODIFY";
	int2str[IN_MOVE_SELF]     = "IN_MOVE_SELF";
	int2str[IN_MOVED_FROM]    = "IN_MOVED_FROM";
	int2str[IN_MOVED_TO]      = "IN_MOVED_TO";
	int2str[IN_OPEN]          = "IN_OPEN";

	std::string output;

	for (auto iter = int2str.begin(), end = int2str.end();
		 iter != end; ++iter)
	{
		if (event.mask & iter->first)
			output += ( iter->second + " | " );
	}

	//  Discard the last delimiter:
	if (output != "")
		output = output.substr(0, output.size()-3);

	std::printf("wd:     %d\n", event.wd);
	std::printf("mask:   %s\n", output.c_str());
	std::printf("cookie: %u\n", event.cookie);
	std::printf("name:   '%s'\n", event.name.c_str());
	std::fflush(stdout);

	return true;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::printf("usage: %s <path to test file>\n", argv[0]);
		std::fflush(stdout);
		return 0;
	}

	Inotify inotify;
	AbortIfNot(inotify.init(0), -1);
	int wd = inotify.add_watch( argv[1], IN_MODIFY | IN_ATTRIB);

	AbortIfNot(inotify.data_sig.attach(&handle_input),
		-1);

	AbortIf(wd < 0, -1);

	std::signal(SIGINT, sig_handler);
	while (!quit)
	{
		inotify.poll(100);
	}

	AbortIfNot(inotify.rm_watch(wd),
		-1);

	return 0;
}
