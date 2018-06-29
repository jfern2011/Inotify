/**
 *  \file   SharedFd.cpp
 *  \author Jason Fernandez
 *  \date   4/1/2018
 *
 *  https://github.com/jfern2011/io_tools
 */

#include "abort.h"
#include "SharedFd.h"

#include <unistd.h>
#include <utility>

/**
 * The static file descriptor use counter
 */
int SharedFd::_use_count[FD_SETSIZE];

/**
 * Default constructor
 */
SharedFd::SharedFd() : Fd()
{
}

/**
 * Constructor
 *
 * @param[in] fd A raw file descriptor. If invalid (i.e. negative),
 *               this is the same as default construction
 */
SharedFd::SharedFd(int fd)
	: Fd(fd)
{
	if (0 <= fd) _use_count[fd]++;
}

/**
 * Copy constructor
 *
 * @param[in] other The SharedFd instance with which the raw file
 *                  descriptor will be shared
 */
SharedFd::SharedFd(const SharedFd& other)
{
	*this = other;
}

/**
 * Destructor
 */
SharedFd::~SharedFd()
{
	reset();
}

/**
 * Move constructor
 *
 * @param[in] other The instance from which the raw file descriptor
 *                  will be acquired
 */
SharedFd::SharedFd(SharedFd&& other)
{
	*this = std::move(other);
}

/**
 * Copy assignment operator. This will release the currently managed
 * file descriptor and share ownership with the one belonging to
 * \a rhs. If \a rhs does not own a valid file descriptor, then this
 * is the same as \ref reset()
 *
 * @note This also copies the blocking behavior of \a rhs
 *
 * @param[in] rhs The SharedFd to copy
 *
 * @return *this
 */
SharedFd& SharedFd::operator=(const SharedFd& rhs)
{
	if (this != &rhs && _fd != rhs._fd)
	{
		reset(rhs._fd);

		_is_blocking =
			rhs._is_blocking;
	}

	return *this;
}

/**
 * Move assignment operator. This will release the currently managed
 * file descriptor and acquire the one belonging to \a rhs,
 * whether or not it is valid. In either case, \a rhs will hand over
 * its file descriptor
 *
 * @note This also copies the blocking behavior of \a rhs
 *
 * @param[in] rhs The SharedFd to move
 *
 * @return *this
 */
SharedFd& SharedFd::operator=(SharedFd&& rhs)
{
	if (this != &rhs)
	{
		reset();

		this->_fd = rhs._fd;
		rhs._fd   = -1;

		_is_blocking =
			rhs._is_blocking;
	}

	return *this;
}

/**
 * Release ownership of the file descriptor. If this was the
 * last owning instance, the file descriptor is closed
 */
void SharedFd::reset()
{
	if (*this)
	{
		_use_count[_fd]--;

		if ((_fd > 2) && (_use_count[_fd] == 0))
			::close(_fd);

		_fd = -1;
	}
}

/**
 * Release the currently managed file descriptor and acquire
 * ownership of \a fd. If this object is marked blocking
 * (which can be checked with \ref is_blocking()), the newly
 * acquired file descriptor will too be made blocking
 *
 * @param[in] fd The file descriptor to manage
 *
 * @return True on success
 */
bool SharedFd::reset(int fd)
{
	if (fd != _fd)
	{
		reset(); _fd = fd;

		if (*this)
		{
			AbortIfNot(set_blocking(_is_blocking),
				false);
			_use_count[fd]++;
		}
	}

	return true;
}

/**
 * Check whether this instance does not share ownership with
 * any other SharedFds (i.e. \ref use_count() == 1)
 *
 * @note An empty SharedFd is not considered unique
 *
 * @return True if this is unique
 */
bool SharedFd::unique() const
{
	if (*this)
		return _use_count[_fd] == 1;
	return false;
}

/**
 * Get the number of SharedFds (including ourselves) who are
 * also managing our file descriptor
 *
 * @note This function returns zero for an empty SharedFd
 *
 * @return The number of objects sharing our file descriptor
 */
int SharedFd::use_count() const
{
	if (*this)
		return _use_count[_fd];
	return 0;
}
