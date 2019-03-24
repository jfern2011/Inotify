/**
 *  \file   Inotify.cpp
 *  \author Jason Fernandez
 *  \date   6/28/2018
 *
 *  https://github.com/jfern2011/Inotify
 */

#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "Inotify.h"

/**
 * Constructor
 */
Inotify::Inotify()
	: _fd(), _raw(nullptr), _raw_len(0)
{
}

/**
 * Destructor
 */
Inotify::~Inotify()
{
	if (_raw) delete[] _raw;
}

/**
 * Wrapper to the inotify_add_watch() API call
 *
 * @param[in] path See inotify_add_watch(2)
 * @param[in] mask See inotify_add_watch(2)
 *
 * @return A unique watch descriptor
 */
int Inotify::add_watch(const std::string& path, uint32_t mask)
{
	return ::inotify_add_watch(_fd.get(), path.c_str(), mask);
}

/**
 * Wrapper to the inotify_init1() API call
 *
 * @param[in] flags See inotify_init(2)
 *
 * @return True on success
 */
bool Inotify::init(int flags)
{
	_fd.reset( ::inotify_init1(flags) );
	return (bool)_fd;
}

/**
 * Get the Inotify file descriptor (e.g. to
 * add to a fd_set)
 *
 * @return The file descriptor
 */
SharedFd Inotify::get_fd() const
{
    return _fd;
}

/**
 *  Poll for events, forwarding them to consumers in the form of
 *  \ref Event structs
 *
 * @param[in] timeout Wait at most this many milliseconds for
 *                    events before returning. If negative, this
 *                    may block indefinitely; if zero, this
 *                    returns immediately, even if no events are
 *                    available (returning true in that case)
 *
 * @return True on success
 */
bool Inotify::poll(int timeout)
{
	if (_fd.can_read(timeout))
	{
		int fsize = 0;
		if (::ioctl(_fd.get(), FIONREAD, &fsize) < 0)
			return false;

		if (!_resize(fsize)) return false;

		return _emit_data(fsize);
	}

	return true;
}

/**
 * Wrapper to the inotify_rm_watch() API call
 *
 * @param[in] wd See inotify_rm_watch(2)
 *
 * @return True on success
 */
bool Inotify::rm_watch(int wd)
{
	return ::inotify_rm_watch(_fd.get(), wd)
				== 0;
}

/**
 * Emit event notifications on \ref data_sig
 *
 * @note The number of \a bytes to read should resolve to an
 *       integral number of events; in other words, there should
 *       never be any leftover bytes after reading any number of
 *       events. If this were to occur, those leftover bytes
 *       would be discarded
 *
 * @param[in] bytes The number of bytes of data to read
 *
 * @return True on success
 */
bool Inotify::_emit_data(int bytes)
{
	if (data_sig.is_connected())
	{
		int nbytes = ::read( _fd.get(), _raw, bytes);
		if (nbytes < 0) return false;

		Event event;

		for (char* ptr = _raw; ptr < _raw + bytes;)
		{
			auto evt_ptr= (struct inotify_event*)ptr;

			event.cookie = evt_ptr->cookie;
			event.mask   = evt_ptr->mask;
			event.name   = std::string(evt_ptr->name,
									   evt_ptr->len);
			event.wd     = evt_ptr->wd;

			if ( !data_sig.raise(event) )
				return false;

			ptr += sizeof(struct inotify_event)
					+ evt_ptr->len;
		}
	}

	return true;
}

/**
 * Size the internal buffer as needed to accommodate input
 * from the inotify file descriptor. If the buffer is already
 * sufficiently large, nothing is done. Otherwise, if dynamic
 * allocation fails, the buffer retains its original state
 *
 * @param[in] new_size The new size, in bytes
 *
 * @return True on success
 */
bool Inotify::_resize(size_t new_size)
{
	if (new_size > _raw_len)
	{
		char* temp = _raw;

		if (!(_raw = new char[ new_size ]))
		{
			_raw = temp; return false;
		}

		if (temp) delete[] temp;
		
		_raw_len = new_size;
	}

	return true;
}
