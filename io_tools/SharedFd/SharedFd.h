/**
 *  \file   SharedFd.h
 *  \author Jason Fernandez
 *  \date   4/1/2018
 *
 *  https://github.com/jfern2011/io_tools
 */

#ifndef __SHARED_FD_H__
#define __SHARED_FD_H__

#include <sys/types.h> // FD_SETSIZE

#include "Fd.h"

/**
 * @class SharedFd
 *
 * Manages a single file descriptor, employing semantics similar to
 * std::shared_ptr. A key difference is if two shared pointers
 * are initialized with the same raw pointer, then each will have a
 * use count of 1. On the other hand, two SharedFds initialized
 * with the same file descriptor will set its use count to 2
 *
 * @note
 * Because ShareFds globally share file descriptors, it is possible
 * that a SharedFd marks its shared file descriptor as blocking
 * without any of its co-owners knowing about it. A file descriptor
 * may be checked for blocking behavior via \ref is_blocking()
 */
class SharedFd : public Fd
{
	static int _use_count[FD_SETSIZE];

	friend class SharedFd_ut;

public:

	SharedFd();

	SharedFd(int fd);

	SharedFd(const SharedFd& other);

	SharedFd(SharedFd&& other);

	~SharedFd();

	SharedFd& operator=(const SharedFd& rhs);

	SharedFd& operator=(SharedFd&& rhs);

	void reset();

	bool reset( int fd );

	bool unique() const;

	int use_count() const;

private:

	using Fd::release;
};

#endif
