/**
 *  \file   Fd.h
 *  \author Jason Fernandez
 *  \date   4/1/2018
 *
 *  https://github.com/jfern2011/io_tools
 */

#ifndef __FILE_DESCRIPTOR_H__
#define __FILE_DESCRIPTOR_H__

#ifndef DOXYGEN_SKIP
#define _static_assert_cpp11_(a) static_assert(a, #a);
#endif

/**
 * @class Fd
 *
 * Manages a single file descriptor, employing semantics similar to
 * std::unique_ptr. An Fd cannot be copied, but ownership of the
 * file descriptor it encapsulates can be transferred to another
 * instance. The file descriptor is closed once the owning instance
 * is destroyed, unless the file descriptor is STDIN_FILENO,
 * STDOUT_FILENO, or STDERR_FILENO
 */
class Fd
{

public:

	Fd();

	Fd(int fd);

	Fd(Fd&& other);

	~Fd();

	Fd& operator=(Fd&& rhs);

	explicit operator bool() const;

	bool can_read (int timeout=-1) const;

	bool can_write(int timeout=-1) const;

	int get() const;

	bool is_blocking() const;

	int release();

	bool reset(int fd);

	bool set_blocking(bool val);

	void swap(Fd& fd);

private:

	bool _poll(short ev, int timeout,
		bool& ready) const;

protected:

	/**
	 * The file descriptor currently being managed
	 */
	int _fd;

	/**
	 * If true, \ref _fd is blocking
	 */
	bool _is_blocking;
};

#endif
