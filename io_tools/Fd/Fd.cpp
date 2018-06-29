/**
 *  \file   Fd.cpp
 *  \author Jason Fernandez
 *  \date   4/1/2018
 *
 *  https://github.com/jfern2011/io_tools
 */

#include <fcntl.h>
#include <poll.h>
#include <utility>

#include <unistd.h>

#include "abort.h"
#include "Fd.h"

#ifndef DOXYGEN_SKIP
_static_assert_cpp11_( STDIN_FILENO == 0);
_static_assert_cpp11_(STDOUT_FILENO == 1);
_static_assert_cpp11_(STDERR_FILENO == 2);
#endif

/**
 * Default constructor
 */
Fd::Fd() : _fd(-1), _is_blocking(false)
{
}

/**
 * Constructor
 *
 * @param [in] fd A raw file descriptor
 */
Fd::Fd(int fd)
	: _fd(fd), _is_blocking(false)
{
}

/**
 * Move constructor
 *
 * @param[in] other The file descriptor to take ownership of
 */
Fd::Fd(Fd&& other)
{
	*this  = std::move(other);
}

/**
 * Destructor
 */
Fd::~Fd()
{
	if (2 < _fd) ::close(_fd);
}

/**
 * Move assignment operator
 *
 * @note This will also copy the blocking behavior of \a rhs
 *
 * @param [in] rhs  The file descriptor to take ownership of
 *
 * @return *this
 */
Fd& Fd::operator=(Fd&& rhs)
{
	if (this != &rhs)
	{
		this->_fd = rhs._fd;
		rhs._fd   = -1;

		_is_blocking =
			rhs._is_blocking;
	}

	return *this;
}

/**
 * C++ boolean type conversion. Returns true if the managed
 * file descriptor is valid
 *
 * @return  True if we are handling a valid file descriptor
 */
Fd::operator bool() const
{
	return 0 <= _fd;
}

/**
 * Check if data is available for reading on the file descriptor
 *
 * @param[in]  timeout Wait at most this many milliseconds
 *                     for the file to become readable.
 *                     Specifying -1 may block indefinitely
 *
 * @return True if reading would not block
 */
bool Fd::can_read(int timeout) const
{
	bool ready = false;
	AbortIfNot(_poll(POLLIN, timeout, ready), false);
	return ready;
}

/**
 * Check if space is available for writing on the file descriptor
 *
 * @param[in] timeout Wait at most this many milliseconds
 *                    for the file to become writable.
 *                    Specifying -1 may block indefinitely
 *
 * @return True if writing would not block
 */
bool Fd::can_write(int timeout) const
{
	bool ready =  false;
	AbortIfNot(_poll(POLLOUT, timeout, ready), false);
	return ready;
}

/**
 * Get the internal file descriptor
 *
 * @return  The file descriptor currently being managed, or
 *          -1 if we are not managing one
 */
int Fd::get() const
{
	return _fd;
}

/**
 * Get the file descriptor blocking mode
 *
 * @return True if blocking is enabled
 */
bool Fd::is_blocking() const
{
	return _is_blocking;
}

/**
 * Release ownership of the file descriptor currently being
 * handled. This will not modify its blocking behavior
 *
 * @return The file descriptor we were managing
 */
int Fd::release()
{
	int fd = _fd;
	_fd = -1;

	return fd;
}

/**
 * Assume ownership of a new file descriptor. If this Fd is
 * marked blocking (i.e. according to \ref is_blocking()),
 * then the newly acquired file descriptor \a fd shall also
 * be made blocking
 *
 * @param[in] fd The new file descriptor
 *
 * @return True on success
 */
bool Fd::reset(int fd)
{
	if (2 < _fd) ::close(_fd);
	_fd = fd;

	AbortIfNot(set_blocking(_is_blocking),
		false);

	return true;
}

/**
 * Set the file descriptor blocking mode. This may even be done
 * when the currently handled file descriptor is invalid; in
 * this case the next file descriptor acquired (e.g. via a call
 * to \ref reset()) will automatically have its blocking mode
 * set to \a val
 *
 * @param[in] val If true, enable blocking behavior
 *
 * @return True on success
 */
bool Fd::set_blocking(bool val)
{
	if (*this)
	{
		int flags = ::fcntl(_fd, F_GETFL, 0);
		if (flags < 0) return false;

		if (!val)
		{
			if (::fcntl(_fd, F_SETFL, flags |   O_NONBLOCK ) < 0)
				return false;
		}
		else
		{
			if (::fcntl(_fd, F_SETFL, flags & (~O_NONBLOCK)) < 0)
				return false;
		}
	}

	_is_blocking = val;
	return true;
}

/**
 * Swap internal file descriptors with another \ref Fd
 *
 * @param[in,out] fd The \ref Fd to swap with
 */
void Fd::swap(Fd& fd)
{
	int temp = _fd;

	_fd    = fd._fd;
	fd._fd = temp;
}

/**
 * Polls the file descriptor for the specified event
 *
 * @param[in]  ev      The event to poll for
 * @param[in]  timeout The number of milliseconds to wait for
 *                     an I/O notification
 * @param[out] ready   True if the event occured
 *
 * @return True on success
 */
bool Fd::_poll(short ev, int timeout, bool& ready) const
{
	ready = false;

	if (*this)
	{
		struct pollfd pfd; pfd.events = ev;
		pfd.fd = _fd;

		const int res = ::poll(&pfd, 1, timeout);
		AbortIf(res < 0, false);
		if (res > 0)
		{
			AbortIfNot(pfd.revents == ev, false);
			ready = true;
		}
	}

	return true;
}
