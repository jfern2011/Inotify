cc_library(
	name = "inotify",
	srcs = ["Inotify.cpp"],
	hdrs = ["Inotify.h"],
	deps = [
		"@io_tools//SharedFd:sharedfd",
		"@io_tools//signal:signal",
	],

	visibility = [
		"//ut:__pkg__"
	],
)
