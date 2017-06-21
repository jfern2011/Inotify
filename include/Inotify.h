#ifndef __INOTIFY_H__
#define __INOTIFY_H__

#include "WriteEventSink.h"

#include <csignal>
#include <map>
#include <list>
#include <sys/inotify.h>

/**
 **********************************************************************
 *
 * @class Inotify
 *
 * A wrapper for the Linux Inotify API. An Inotify object associates
 * file system events with user-defined handler routines that are
 * dispatched depending on what events occurred. Optionally, Inotify
 * events can be handled manually
 *
 **********************************************************************
 */
class Inotify
{
	/*
	 * Represents an Inotify watch
	 */
	struct Watch
	{
		Watch(int _mask, const std::string& _path, int _wd)
			: mask(_mask), path(_path), wd(_wd)
		{
		}

		uint32_t    mask; // Mask of events to monitor for
		std::string path; // Path to monitor
		int         wd;   // A registered watch descriptor
	};

	typedef std::list<Watch> watch_l;

	/*
	 * Associates file system events with event handlers
	 */
	struct sig_info
	{
		sig_info(uint32_t _mask, Signal::signal_base* _sig,
				 int _wd)
			: mask(_mask), sig(_sig), wd(_wd)
		{
		}

		sig_info(sig_info&& other)
			: mask(other.mask), wd(other.wd)
		{
			sig = other.sig; other.sig = NULL;
		}

		~sig_info()
		{
			if (sig) delete sig;
		}

		uint32_t mask;            // Mask of trigger events
		Signal::signal_base* sig; // Event handler
		int wd;                   // User-selected watch ID
	};

	typedef std::list<sig_info>
		signal_l;

public:

	/**
	 * ----------------------------------------------------------------
	 *
	 * @struct InotifyEvent
	 *
	 *  An enhanced version of the inotify_event struct. See the Linux
	 *  API for details
	 *
	 * @var wd     An Inotify watch descriptor
	 * @var mask   A bitmask indicating what file system events to
	 *             monitor for on \a wd
	 * @var cookie A unique ID for synchronizing events
	 * @var len    The length of the "name" field (not actually needed
	 *             since \a name is a std::string)
	 * @var name   The file path where the event occurred
	 *
	 * ---------------------------------------------------------------- 
	 */
	struct InotifyEvent
	{
		InotifyEvent(const struct inotify_event& _event)
			: wd(_event.wd), mask(_event.mask),
			  cookie(_event.cookie), len(_event.len)
		{
			name = std::string(_event.name);
		}

		InotifyEvent(InotifyEvent&& _event)
		{
			*this = std::move(_event);
		}

		InotifyEvent& operator=( InotifyEvent&& rhs )
		{
			if (this != &rhs)
			{
				name   = std::move(rhs.name);
				wd     = rhs.wd;
				mask   = rhs.mask;
				cookie = rhs.cookie;
				len    = rhs.len;
			}

			return *this;
		}

		/*
		 * Fields from the inotify_event struct:
		 */
		int wd;
		uint32_t mask, cookie, len;
		std::string name;
	};

	/**
	 * Construct a new Inotify instance
	 *
	 * @exception std::runtime_error If the call to inotify_init1()
	 *            fails
	 *
	 * @param [in] flags Flags to pass to inotify_init1(). Defaults
	 *                   to zero
	 */
	Inotify(int flags=0)
		: _event_queue(), _event_sink(), _sigs()
	{
		_fd = inotify_init1(flags);

		if (_fd < 0 || !_event_sink.init(_fd))
			std::raise(SIGABRT);
		else
			_event_sink.attach_reader(*this,
						 &Inotify::update_queue);
	}

	/**
	 * Destructor
	 */
	~Inotify()
	{
		::close(_fd);
	}

	/**
	 * Add a new watch or modify an existing one. This is a wrapper
	 * for inotify_add_watch()
	 *
	 * @param[in] path The file or directory to monitor
	 * @param[in] mask A bitmask specifying what file system events
	 *                 to monitor for
	 *
	 * @return A new watch descriptor, or -1 on error
	 */
	int add_watch(const std::string& path, uint32_t mask)
	{
		int wd =
			inotify_add_watch( _fd, path.c_str(), mask );

		if (exists(wd))
			return update_watch(wd,mask);

		if (wd != -1)
				_watches.push_back(Watch(mask, path, wd));

		return wd;
	}

	/**
	 * Attach an event handler to be invoked whenever a file system
	 * event occurs on the given watch descriptor, according to the
	 * given event mask
	 *
	 * @tparam R  The return type of the event handler
	 * @tparam T1 The type of the first input argument to the event
	 *            handler
	 * @tparam T2 The types of all other inputs needed by the event
	 *            handler
	 *
	 * @param[in] wd   A watch descriptor returned by add_watch()
	 * @param[in] mask The event mask that will trigger the event
	 *                 handler
	 * @param[in] func A function pointer to the handler
	 * @param[in] arg1 The first input argument to forward to the
	 *                 event handler
	 * @param[in] args Additional arguments required by the event
	 *                 handler
	 *
	 * @return True on success, or false if \a wd does not exist or
	 *         \a func is invalid
	 */
	template <typename R, typename T1, typename... T2>
	bool attach(int wd, uint32_t mask,
				R(*func)(T1,T2...), T1&& arg1, T2&&... args)
	{
		AbortIfNot(exists(wd), false);

		if (sig_exists(wd, mask))
			detach(wd, mask);

		auto sig = new Signal::Signal<R,T1,T2...>;
		if (!sig->attach(func))
		{
			delete sig; return false;
		}
		else
			sig->bind(std::forward<T1>(arg1),
						std::forward<T2>(args)...);

		_sigs.push_back(sig_info(mask,
			sig, wd));

		return true;
	}

	/**
	 * Attach an event handler to be invoked whenever a file system
	 * event occurs on the given watch descriptor, according to the
	 * given event mask
	 *
	 * This is a specialization where the event handler requires no
	 * input arguments
	 *
	 * @tparam R The return type of the event handler
	 *
	 * @param[in] wd   A watch descriptor returned by add_watch()
	 * @param[in] mask The event mask that will trigger the event
	 *                 handler
	 * @param[in] func A function pointer to the handler
	 *
	 * @return True on success, or false if \a wd does not exist or
	 *         \a func is invalid
	 */
	template <typename R>
	bool attach(int wd, uint32_t mask, R(*func)())
	{
		AbortIfNot(exists(wd), false);

		if (sig_exists(wd, mask))
			detach(wd, mask);

		auto sig = new Signal::Signal<R,void>;
		if (!sig->attach(func))
		{
			delete sig; return false;
		}

		_sigs.push_back(sig_info(mask,
			sig, wd));

		return true;
	}

	/**
	 * Attach an event handler to be invoked whenever a file system
	 * event occurs on the given watch descriptor, according to the
	 * given event mask
	 *
	 * @tparam R The return type of the event handler
	 * @tparam C Class defining the handler
	 * @tparam T1 The type of the first input argument to the event
	 *            handler
	 * @tparam T2 The types of all other inputs needed by the event
	 *            handler
	 *
	 * @param[in] wd   A watch descriptor returned by add_watch()
	 * @param[in] mask The event mask that will trigger the event
	 *                 handler
	 * @param[in] obj  Instance of the class defining the handler
	 * @param[in] func A pointer to the class method that will
	 *                 serve as the event handler
	 * @param[in] arg1 The first input argument to forward to the
	 *                 event handler
	 * @param[in] args Additional arguments required by the event
	 *                 handler
	 *
	 * @return True on success, or false if \a wd does not exist or
	 *         \a func is invalid
	 */
	template <typename R, typename C, typename T1, typename... T2>
	bool attach(int wd, uint32_t mask, C& obj,
				R(C::*func)(T1,T2...), T1&& arg1, T2&&... args)
	{
		AbortIfNot(exists(wd), false);

		if (sig_exists(wd, mask))
			detach(wd, mask);

		auto sig = new Signal::Signal<R,T1,T2...>;
		if (!sig->attach(obj,func))
		{
			delete sig; return false;
		}
		else
			sig->bind(std::forward<T1>(arg1),
						std::forward<T2>(args)...);

		_sigs.push_back(sig_info(mask,
			sig, wd));

		return true;
	}

	/**
	 * Attach an event handler to be invoked whenever a file system
	 * event occurs on the given watch descriptor, according to the
	 * given event mask
	 *
	 * This is a specialization where the event handler requires no
	 * input arguments
	 *
	 * @tparam R The return type of the event handler
	 * @tparam C The class defining the event handler
	 *
	 * @param[in] wd   A watch descriptor returned by add_watch()
	 * @param[in] mask The event mask that will trigger the event
	 *                 handler
	 * @param[in] obj  Instance of the class defining the handler
	 * @param[in] func A pointer to the class method that will
	 *                 serve as the event handler
	 *
	 * @return True on success, or false if \a wd does not exist or
	 *         \a func is invalid
	 */
	template <typename R, typename C>
	bool attach(int wd, uint32_t mask, C& obj, R(C::*func)())
	{
		AbortIfNot(exists(wd), false);

		if (sig_exists(wd, mask))
			detach(wd, mask);

		auto sig = new Signal::Signal<R,void>;
		if (!sig->attach(obj,func))
		{
			delete sig; return false;
		}

		_sigs.push_back(sig_info(mask,
			sig, wd));

		return true;
	}

	/**
	 * Attach an event handler to be invoked whenever a file system
	 * event occurs on the given watch descriptor, according to the
	 * given event mask
	 *
	 * @tparam R The return type of the event handler
	 * @tparam C Class defining the handler
	 * @tparam T1 The type of the first input argument to the event
	 *            handler
	 * @tparam T2 The types of all other inputs needed by the event
	 *            handler
	 *
	 * @param[in] wd   A watch descriptor returned by add_watch()
	 * @param[in] mask The event mask that will trigger the event
	 *                 handler
	 * @param[in] obj  Instance of the class defining the handler
	 * @param[in] func A pointer to the class method that will
	 *                 serve as the event handler
	 * @param[in] arg1 The first input argument to forward to the
	 *                 event handler
	 * @param[in] args Additional arguments required by the event
	 *                 handler
	 *
	 * @return True on success, or false if \a wd does not exist or
	 *         \a func is invalid
	 */
	template <typename R, typename C, typename T1, typename... T2>
	bool attach(int wd, uint32_t mask, C& obj,
			R(C::*func)(T1,T2...) const, T1&& arg1, T2&&... args)
	{
		AbortIfNot(exists(wd), false);

		if (sig_exists(wd, mask))
			detach(wd, mask);

		auto sig = new Signal::Signal<R,T1,T2...>;
		if (!sig->attach(obj,func))
		{
			delete sig; return false;
		}
		else
			sig->bind(std::forward<T1>(arg1),
						std::forward<T2>(args)...);

		_sigs.push_back(sig_info(mask,
			sig, wd));

		return true;
	}

	/**
	 * Attach an event handler to be invoked whenever a file system
	 * event occurs on the given watch descriptor, according to the
	 * given event mask
	 *
	 * This is a specialization where the event handler requires no
	 * input arguments
	 *
	 * @tparam R The return type of the event handler
	 * @tparam C The class defining the event handler
	 *
	 * @param[in] wd   A watch descriptor returned by add_watch()
	 * @param[in] mask The event mask that will trigger the event
	 *                 handler
	 * @param[in] obj  Instance of the class defining the handler
	 * @param[in] func A pointer to the class method that will
	 *                 serve as the event handler
	 *
	 * @return True on success, or false if \a wd does not exist or
	 *         \a func is invalid
	 */
	template <typename R, typename C>
	bool attach(int wd, uint32_t mask, C& obj, R(C::*func)() const)
	{
		AbortIfNot(exists(wd), false);

		if (sig_exists(wd, mask))
			detach(wd, mask);

		auto sig = new Signal::Signal<R,void>;
		if (!sig->attach(obj,func))
		{
			delete sig; return false;
		}

		_sigs.push_back(sig_info(mask,
			sig, wd));

		return true;
	}

	/*
	 * Detach the event handler matching a given watch descriptor
	 * and event mask
	 *
	 * @param[in] wd   A watch descriptor returned by add_watch()
	 * @param[in] mask The event mask used to trigger the handler
	 *
	 * @return True on success
	 */
	bool detach(int wd, uint32_t mask)
	{
		for (auto iter = _sigs.begin(),
			  end = _sigs.end(); iter != end; ++iter)
		{
			if (iter->wd == wd && iter->mask == mask)
			{
				_sigs.erase(iter);
						return true;
			}
		}

		return false;
	}

	/*
	 * Detach each event handler matching a given watch descriptor
	 *
	 * @param[in] wd A watch descriptor returned by add_watch()
	 *
	 * @return True on success
	 */
	bool detach(int wd)
	{
		bool success = false;

		for (auto iter = _sigs.begin(),
			 end = _sigs.end(); iter != end; ++iter)
		{
			if (iter->wd == wd)
			{
				_sigs.erase(iter);
						success = true;
			}
		}

		return success;
	}

	/**
	 * Check if a watch descriptor exists
	 *
	 * @param [in] wd The inotify watch descriptor to search for
	 *
	 * @return True if it exists
	 */
	bool exists(int wd)
	{
		for (auto iter = _watches.begin(), end = _watches.end();
			 iter != end; ++iter)
		{
			if (iter->wd == wd) return true;
		}

		return false;
	}

	/**
	 * Translates an event mask into human-readable form
	 *
	 * @param[in] mask The event bit mask
	 * 
	 * @return A string representation of the event mask
	 */
	static std::string mask2name( uint32_t mask )
	{
		std::map<uint32_t, std::string> int2str;
		std::string output = "";

		if (mask == 0) return "";

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

		for (auto iter = int2str.begin(), end = int2str.end();
			 iter != end; ++iter)
		{
			if (mask & iter->first)
				output += ( iter->second + " | " );
		}

		//  Discard the last delimiter:
		if (output != "")
			return output.substr(0, output.size()-3);
		else
			return output;
	}

	/**
	 * Poll all registered watch descriptors. This will dispatch
	 * event handlers for all monitored events that occurred
	 *
	 * @return True on success or false if a read error occurred
	 *         on the inotify file descriptor
	 */
	bool poll()
	{
		bool success = true;

		for (auto iter = _watches.begin(), end = _watches.end();
			 iter != end; ++iter)
				success = success && poll( iter->wd );

		return success;
	}

	/**
	 * Poll a specific watch descriptor, dispatching event handlers
	 * based on that watch's event mask
	 *
	 * @param[in] wd A watch descriptor returned by add_watch()
	 *
	 * @return True on success or false if a read error occurred on
	 *         the inotify file descriptor
	 */
	bool poll(int wd)
	{
		AbortIfNot(pollFd(),false);

		for (auto iter= _event_queue.begin(), end= _event_queue.end();
			 iter != end; )
		{
			if (iter->wd == wd)
			{
				for (auto iter2 = _sigs.begin(),
					 end2 = _sigs.end(); iter2 != end2; ++iter2)
				{
					if (iter2->wd == wd && (iter2->mask & iter->mask))
					{
						iter2->sig->v_raise();
					}
				}

				iter =
					_event_queue.erase(iter);
			}
			else
				++iter;
		}

		return true;
	}

	/*
	 * Poll for Inotify events. This produces an event "queue" of all
	 * events currently queued up but not yet processed, plus any
	 * waiting events that have not been queued. All of these events
	 * are moved to \a _queue, meaning that calling this again will
	 * produce an empty list (unless new events occurred on the file
	 * descriptor)
	 *
	 * The purpose of this is to provide the option to handle events
	 * manually without having to attach handlers
	 *
	 * @param[out] _queue The current set of Inotify events
	 *
	 * @return  True on success, or false if a read error occurred on
	 *          the inotify file descriptor
	 */
	bool poll(std::list<InotifyEvent>& _queue)
	{
		AbortIfNot(pollFd(),false);

		if (!_event_queue.empty())
		{
			_queue = std::move( _event_queue );
			_event_queue.clear();
		}

		return true;
	}

	/**
	 * Print an InotifyEvent
	 */
	static void print(const InotifyEvent& event)
	{
		std::cout << "----------------------\n";
		std::cout << "watch descriptor: "
			<< event.wd  << "\n";
		std::cout << "mask:             "
			<< mask2name(event.mask)
			<< "\n";
		std::cout << "cookie:           "
			<< event.cookie << "\n";
		std::cout << "name length:      "
			<< event.len << "\n";
		std::cout << "name:             "
						<< event.name << std::endl;
	}

	/**
	 *  Remove a watch. This is a wrapper for inotify_rm_watch() 
	 *
	 * @param[in] wd An inotify watch descriptor
	 *
	 * @return True on success, or false if the watch descriptor
	 *         was not found
	 */
	bool rm_watch(int wd)
	{
		bool success = false;

		for (auto iter = _watches.begin(), end = _watches.end();
			 iter != end; ++iter)
		{
			if (iter->wd == wd)
			{
				success= (inotify_rm_watch(_fd, iter->wd) == 0);
				_watches.erase(iter);
				break;
			}
		}

		detach(wd);
			return success;
	}

	/**
	 *  Remove a watch. This is a wrapper for inotify_rm_watch() 
	 *
	 * @param[in] path The path being monitored
	 *
	 * @return  True on success, or false if no watch descriptor
	 *          exists for the given path
	 */
	int rm_watch(const std::string& path)
	{
		bool success = false;

		int wd = -1;
		for (auto iter = _watches.begin(), end = _watches.end();
			 iter != end; ++iter)
		{
			if (iter->path == path)
			{
				success= (inotify_rm_watch(_fd, iter->wd) == 0);
				wd = iter->wd;
				_watches.erase(iter);
				break;
			}
		}

		detach(wd);
			return success;
	}

private:

	/*
	 * Send a read request to the event sink, which will invoke a
	 * callback to update the event queue
	 */
	bool pollFd()
	{
		WriteEventSink::err_code code = _event_sink.read();

		AbortIf(code != WriteEventSink::SUCCESS &&
			    code != WriteEventSink::NO_DATA, false);

		return true;
	}

	/*
	 * Determine if a handler exists based on its associated watch
	 * descriptor and event mask
	 */
	bool sig_exists(int wd, uint32_t mask)
	{
		for (auto iter = _sigs.begin(),
			  end = _sigs.end(); iter != end; ++iter)
		{
			if (iter->wd == wd && iter->mask == mask)
			{
				return true;
			}
		}

		return false;
	}

	/*
	 * Update stored events with newly read data from the inotify
	 * file descriptor
	 */
	bool update_queue(const char* data, size_t size)
	{
		for (const char* p = data; p < data + size;)
		{
			struct inotify_event* event
				= (inotify_event*)p;

			AbortIf(data + size
					<= p + sizeof (struct inotify_event),
					false,
					"incomplete write");

			_event_queue.push_back(InotifyEvent(*event));

			p += sizeof(struct inotify_event)
					+ event->len;
		}

		return true;
	}

	/*
	 * Update a watch's event mask. This has nothing to do with
	 * the API's internal watches
	 */
	int update_watch(int wd, uint32_t mask)
	{
		for (auto iter = _watches.begin(), end = _watches.end();
			iter != end; ++iter)
		{
			if (iter->wd == wd)
			{
				iter->mask = mask;
					return iter->wd;
			}
		}

		return -1;
	}

	std::list<InotifyEvent> _event_queue;
	WriteEventSink _event_sink;
	int _fd;
	signal_l _sigs;
		watch_l _watches;
};

#endif