/**
 *  \file   Inotify.h
 *  \author Jason Fernandez
 *  \date   6/28/2018
 *
 *  https://github.com/jfern2011/Inotify
 */

#ifndef __INOTIFY_H__
#define __INOTIFY_H__

#include <string>

#include "SharedFd/SharedFd.h"
#include "signal/Signal.h"

/**
 * @class Inotify
 *
 * A wrapper for the Linux Inotify API. Unlike the API which requires
 * reading from a raw file descriptor, an Inotify object is polled
 * for events which are handled by user-defined routines. This avoids
 * having to pull variable-sized inotify_event structs as described
 * in the Inotify(7) page
 */
class Inotify
{

public:

	/**
	 * This is essentially an inotify_event struct, but replaces the
	 * name/len fields with a std::string. When an Inotify object
	 * is polled, any events that occured since the last \ref poll()
	 * are emitted via the \ref data_sig
	 */
	struct Event
	{
		/** Watch descriptor */
		int         wd;

		/** The bit mask of returned events */
		uint32_t    mask;

		/** Cookie for synchronizing events */
		uint32_t    cookie;

		/** Optional name */
		std::string name;
	};

	Inotify();

	~Inotify();

	int add_watch(const std::string& path, uint32_t mask);

	bool init(int flags = 0);

	bool poll(int timeout = 0);

	bool rm_watch(int wd);

	/**
	 * The signal raised when events are ready
	 */
	Signal::Signal<bool, const Event&>
		data_sig;

private:

	bool _resize(size_t new_size);

	bool _emit_data(int bytes);

	/**
	 *  The inotify file descriptor
	 */
	SharedFd _fd;

	/**
	 * Stores file descriptor reads
	 */
	char* _raw;

	/**
	 * Size of _raw, in bytes
	 */
	size_t _raw_len;
};

#endif
