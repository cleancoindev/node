/**
 * @file	.
 *
 * @brief	Declares the RunAfterEx template class
 * 			The class instance is a special object to call any method after some delay applying desired LaunchScheme (see below)
 *
 * 			Samples:
 * 			1. Declare a class member to call argless method:  
 * 				a) declare and implement a method  
 *	@code		void MethodName();
 * 				b) declare a method caller
 *	@code		RunAfterEx<CustomProc> method_name_caller;
 *				c) init caller in constructor like this  
 *	@code		method_name_caller([this]() { MethodName(); }, "MethodName()")
 *				d) call MethodName() once with delay of 1 second
 *	@code		method_name_caller.Schedule( std::chrono::milliseconds(1000), LaunchScheme::single )
 *				d) call MethodName() periodically with delay of 200 milliseconds
 *	@code		method_name_caller.Schedule( std::chrono::milliseconds(200), LaunchScheme::periodic )
 *				e) stop periodic calling or cancel scheduled call
 *	@code		method_name_caller.Cancel();
 * 			2. Declare a class member to call 2-args method:
 * 				a) declare and implement a method
 *	@code		void MethodName( int arg1, int arg2 );
 * 				b) declare a method caller
 *	@code		std::function<void(int, int)> lambda = [this](int arg1, int arg2) {
					MethodName(arg1, arg2);
				};
				RunAfterEx<decltype(lambda)> method_name_caller;
 *				c) init caller in constructor like this
 *	@code		method_name_caller(std::move(lambda), "MethodName(int arg1, int arg2)")
 *				d) call MethodName() once with delay of 1 second giving int parameters value1 and value2
 *	@code		method_name_caller.Schedule( std::chrono::milliseconds(1000), LaunchScheme::single, value1, value2 )
 *				d) call MethodName() periodically with delay of 200 milliseconds
 *	@code		method_name_caller.Schedule( std::chrono::milliseconds(200), LaunchScheme::periodic_single, value1, value2 )
 *				e) stop periodic calling or cancel scheduled call
 *	@code		method_name_caller.Cancel();
 */

/**
 * Inspired by:
 *	void runAfter(const std::chrono::milliseconds& ms, std::function<void()> cb, const char * msg)
 *	{
 *		const auto tp = std::chrono::system_clock::now() + ms;
 *		std::thread tr([tp, cb, msg]() {
 *			std::this_thread::sleep_until(tp);
 *			LOG_WARN("Inserting callback " << msg);
 *			CallsQueue::instance().insert(cb);
 *		});
 *		tr.detach();
 *	}
*/

#pragma once

// Uncomment to activate detailed log using timer_service (header 'timer_service.h' required>
#define TIMER_SERVICE_LOG

#include <chrono>
#include <thread>
#include <string>
#include <atomic>
#include <lib/system/structures.hpp> // CallsQueue

#if defined(TIMER_SERVICE_LOG)
#include <sstream>
#include "timer_service.h"
#endif

/**
 * @enum	LaunchScheme
 *
 * @brief	Values that represent launch schenes
 */

enum class LaunchScheme {

	///< An enum constant representing the single option: launch once, cancel if previous schedule call still is not done
	single,

	///< An enum constant representing the periodic single option: launch periodically, schedule call cancels if already running one cycle
	periodic
};


/** @brief	The custom procedure */
using CustomProc = std::function<void()>;

/** @brief	The custom procedure int argument */
using CustomProcIntArg = std::function<void(int)>;

/**
 * @class	RunAfterEx
 *
 * @brief	A run after ex.
 *
 * @author	User
 * @date	07.09.2018
 *
 * @tparam	TProc	Type of the procedure.
 * @tparam	TRes 	Timer resolution, default is std::chrono::milliseconds.
 */

template<typename TProc = CustomProc, typename TRes = std::chrono::milliseconds>
class RunAfterEx
{
public:

	RunAfterEx() = delete;
	RunAfterEx(const RunAfterEx&) = delete;
	void operator =(const RunAfterEx&) = delete;

	/**
	 * @fn	RunAfterEx(TProc&& proc, std::string&& comment) : _proc(std::forward<TProc>(proc)) , _comment(std::forward<std::string>(comment))
	 *
	 * @brief	Constructor
	 *
	 * @author	User
	 * @date	07.09.2018
	 *
	 * @param [in,out]	proc   	The procedure.
	 * @param [in,out]	comment	The comment.
	 */

	RunAfterEx(TProc&& proc, std::string&& comment)
		: _proc(std::forward<TProc>(proc))
		, _comment(std::forward<std::string>(comment))
	{
	}

	/**
	 * @fn	template<typename... Args> void Schedule(TRes&& wait_for, LaunchScheme scheme, Args... args)
	 *
	 * @brief	Schedules
	 *
	 * @author	User
	 * @date	07.09.2018
	 *
	 * @tparam	Args	Type of the arguments.
	 * @param [in,out]	wait_for	The wait for.
	 * @param 		  	scheme  	The scheme.
	 * @param 		  	args		Variable arguments providing the arguments.
	 */

	template<typename... Args>
	void Schedule(TRes&& wait_for, LaunchScheme scheme, Args... args)
	{

#if defined(TIMER_SERVICE_LOG)
		std::ostringstream os;
		os << "RunAfterEx: schedule (" << wait_for.count() << ") " << _comment;
		timer_service.Mark(os.str(), -1);
#endif

		// test has not already launched in single cases
		if (_launched)
		{
#if defined(TIMER_SERVICE_LOG)
				timer_service.Mark(os.str() + " canceled (already running)", -1);
#endif
			return;
		}

		_launched = true;

		// obtain launch count depending on scheme (-1 - forever)
		{
			Lock l (_mtx);
			switch (scheme) {
			case LaunchScheme::single:
				_remains = 1;
				break;
			case LaunchScheme::periodic:
				_remains = -1;
				break;
			default:
				_remains = 0;
				break;
			}
		}

		std::thread([this, wait_for, scheme, args...]() {

			std::this_thread::sleep_for(wait_for);
			
			// launch until _remains != 0
			do {
				// test stop condition
				if (HandleStopCondition ()) {
					break;
				}
				ExecuteProc (std::forward<TProc> (_proc), args...);
				if (HandleStopCondition ()) {
					break;
				}
				std::this_thread::sleep_for (wait_for);
			} while (true);

			_launched = false;
#if defined(TIMER_SERVICE_LOG)
			std::ostringstream os2;
			os2 << "RunAfterEx: " << _comment << " is finished";
			timer_service.Mark (os2.str (), -1);
#endif
		}).detach();
	}

	/**
	 * @fn	void CancelAfter()
	 *
	 * @brief	Cancels this object
	 *
	 * @author	User
	 * @date	07.09.2018
	 */

	void CancelAfter( int count_calls )
	{
		Lock l (_mtx);
		if (count_calls < 0 || ( _remains > 0 && _remains <= count_calls ) ) {
			// should not schedule periodic calls using CancelAfter()
			// should not re-schedule more calls using CancelAfter()
			return;
		}
		_remains = count_calls;
	}

	/**
	 * @fn	void Cancel()
	 *
	 * @brief	Cancels this object
	 *
	 * @author	User
	 * @date	07.09.2018
	 */

	void Cancel()
	{
		Lock l (_mtx);
		_remains = 0;
	}

	bool IsScheduled () const
	{
		return _launched;
	}

private:

	TProc _proc;
	std::string _comment;
	std::atomic_bool _launched{ false };
	int _remains = 0;
	std::mutex _mtx;
	using Lock = std::lock_guard<std::mutex>;

	// Special 1-arg variant writes more detailed log (with argument value)
#if defined(TIMER_SERVICE_LOG)
	void ExecuteProc(CustomProcIntArg&& proc, int arg)
	{
		std::ostringstream os;
		os << "RunAfterEx: launching " << _comment;
		timer_service.Mark(os.str(), arg);
		//auto _p = std::forward<TProc>(proc);
		CallsQueue::instance().insert([proc, arg]() { proc(arg); });
	}
#endif

	/**
	 * @fn	template<typename... Args> void ExecuteProc(std::function<void(Args...)>&& proc, Args... args)
	 *
	 * @brief	Executes the procedure operation
	 *
	 * @author	User
	 * @date	07.09.2018
	 *
	 * @tparam	Args	Type of the arguments.
	 * @param [in,out]	proc	The procedure.
	 * @param 		  	args	Variable arguments providing the arguments.
	 */

	template<typename... Args>
	void ExecuteProc(std::function<void(Args...)>&& proc, Args... args)
	{
#if defined(TIMER_SERVICE_LOG)
		std::ostringstream os;
		os << "RunAfterEx: launching " << _comment;
		timer_service.Mark(os.str());
#endif
		CallsQueue::instance().insert([proc, args...]() { proc(args...); });
	}

	bool HandleStopCondition ()
	{
		Lock l (_mtx);
		if (_remains > 0) {
			_remains--;
		}
		if (_remains == 0) {
			return true;
		}
		return false;
	}
};
