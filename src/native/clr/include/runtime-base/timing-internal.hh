#pragma once

#include <atomic>
#include <concepts>
#include <ctime>
#include <memory>
#include <mutex>
#include <vector>

#include <shared/cpp-util.hh>
#include <runtime-base/logger.hh>
#include "startup-aware-lock.hh"
#include <runtime-base/strings.hh>
#include "util.hh"
#include "../constants.hh"
#include "monodroid-state.hh"

namespace xamarin::android {
	// bionic should use `time_t` in the timespec struct, but it uses `long` instead
	using time_type = long;

	// Events should never change their assigned values and no values should be reused.
	// Values are used by the test runner to determine what measurement was taken.
	//
	// At the same time, the list should be kept sorted alphabetically for easier reading -
	// values therefore might be out of order, but always unique.
	enum class TimingEventKind
	{
		AssemblyDecompression     = 0,
		AssemblyLoad              = 1,
		AssemblyPreload           = 2,
		DebugStart                = 3,
		Init                      = 4,
		JavaToManaged             = 5,
		ManagedToJava             = 6,
		MonoRuntimeInit           = 7,
		NativeToManagedTransition = 8,
		RuntimeConfigBlob         = 9,
		RuntimeRegister           = 10,
		TotalRuntimeInit          = 11,
		Unspecified               = 12,
	};

	struct TimingEventPoint
	{
		time_t   sec;
		uint64_t ns;
	};

	struct TimingInterval
	{
		time_t   sec;
		uint32_t ms;
		uint32_t ns;
	};

	struct TimingEvent
	{
		bool             before_managed;
		TimingEventPoint start;
		TimingEventPoint end;
		TimingEventKind  kind;
		std::unique_ptr<std::string> more_info;
	};

	template<typename T>
	concept TimingPointType = requires (T a) {
		{ a.sec } -> std::same_as<time_t&>;
		{ a.ns } -> std::same_as<uint64_t&>;
	};

	template<typename T>
	concept TimingIntervalType = requires (T a) {
		{ a.sec } -> std::same_as<time_t&>;
		{ a.ms } -> std::same_as<uint32_t&>;
		{ a.ns } -> std::same_as<uint32_t&>;
	};

	class FastTiming final
	{
		// Number of TimingEvent entries in the event vector allocated at the
		// time of class instantiation.  It's an arbitrary value, but it should
		// be large enough to not require any dynamic reallocation of memory at
		// the run time.
		static constexpr size_t INITIAL_EVENT_VECTOR_SIZE = 4096uz;
		static constexpr uint32_t ns_in_millisecond = 1000000u;
		static constexpr uint32_t ms_in_second = 1000u;
		static constexpr uint32_t ns_in_second = ms_in_second * ns_in_millisecond;

	protected:
		void configure_for_use () noexcept
		{
			events.reserve (INITIAL_EVENT_VECTOR_SIZE);
		}

	public:
		constexpr FastTiming () noexcept
		{}

		[[gnu::always_inline]]
		static auto enabled () noexcept -> bool
		{
			return is_enabled;
		}

		[[gnu::always_inline]]
		static auto is_bare_mode () noexcept -> bool
		{
			return
				(Logger::log_timing_categories() & LogTimingCategories::Bare) == LogTimingCategories::Bare ||
				(Logger::log_timing_categories() & LogTimingCategories::FastBare) == LogTimingCategories::FastBare;
		}

		[[gnu::always_inline]]
		static void initialize (bool log_immediately) noexcept
		{
			if (!Util::should_log (LOG_TIMING)) [[likely]] {
				return;
			}

			mark (init_time.start);
			really_initialize (log_immediately);
			mark (init_time.end);

			init_time.before_managed = true;
			init_time.kind = TimingEventKind::Init;

			if (!immediate_logging) {
				return;
			}

			log (init_time, false /* skip_log_if_more_info_missing */);
		}

		// std::vector<T> isn't used in a conventional manner here. We treat it as if it was a standard array and we
		// don't take advantage of any emplacement functionality, merely using vector's ability to resize itself when
		// needed.  The reason for this is speed - we can atomically increase index into the array and relatively
		// quickly check whether it's within the boundaries.  We can then safely use thus indexed element without
		// worrying about concurrency.  Emplacing a new element in the vector would require holding the mutex, something
		// that's fairly costly and has unpredictable effect on time spent acquiring and holding the lock (the OS can
		// preempt us at this point)
		[[gnu::always_inline]]
		auto start_event (TimingEventKind kind = TimingEventKind::Unspecified) noexcept -> size_t
		{
			size_t index = next_event_index.fetch_add (1);

			if (index >= events.capacity ()) [[unlikely]] {
				StartupAwareLock lock (event_vector_realloc_mutex);
				if (index >= events.size ()) { // don't increase unnecessarily, if another thread has already done that
					// Double the vector size. We should, in theory, check for integer overflow here, but it's more
					// likely we'll run out of memory way, way, way before that happens
					size_t old_size = events.capacity ();
					events.reserve (old_size << 1);
					log_warn (LOG_TIMING, "Reallocated timing event buffer from {} to {}", old_size, events.size ());
				}
			}

			TimingEvent &ev = events[index];
			mark (ev.start);
			ev.kind = kind;
			ev.before_managed = MonodroidState::is_startup_in_progress ();
			ev.more_info = nullptr;

			return index;
		}

		[[gnu::always_inline]]
		void end_event (size_t event_index, bool uses_more_info = false) noexcept
		{
			if (!is_valid_event_index (event_index, __PRETTY_FUNCTION__)) [[unlikely]] {
				return;
			}

			mark (events[event_index].end);
			log (events[event_index], uses_more_info /* skip_log_if_more_info_missing */);
		}

		template<size_t MaxStackSize, typename TStorage, typename TChar = char>
		[[gnu::always_inline]]
		void add_more_info (size_t event_index, string_base<MaxStackSize, TStorage, TChar> const& str) noexcept
		{
			if (!is_valid_event_index (event_index, __PRETTY_FUNCTION__)) [[unlikely]] {
				return;
			}

			events[event_index].more_info = std::make_unique<std::string> (str.get (), str.length ());
			log (events[event_index], false /* skip_log_if_more_info_missing */);
		}

		[[gnu::always_inline]]
		void add_more_info (size_t event_index, const char* str) noexcept
		{
			if (!is_valid_event_index (event_index, __PRETTY_FUNCTION__)) [[unlikely]] {
				return;
			}

			events[event_index].more_info = std::make_unique<std::string> (str);
			log (events[event_index], false /* skip_log_if_more_info_missing */);
		}

		[[gnu::always_inline]]
		static void get_time (time_t &seconds_out, uint64_t& ns_out) noexcept
		{
			int ret;
			timespec tv_ctm;

			ret = clock_gettime (CLOCK_MONOTONIC, &tv_ctm);
			ns_out = ret == 0 ? static_cast<uint64_t>(tv_ctm.tv_nsec) : 0;
			seconds_out = ret == 0 ? tv_ctm.tv_sec : 0;
		}

		template<TimingPointType P, TimingIntervalType I> [[gnu::always_inline]]
		static void calculate_interval (P const& start, P const& end, I &result) noexcept
		{
			uint64_t nsec;
			if (end.ns < start.ns) {
				result.sec = end.sec - start.sec - 1;
				if (result.sec < 0) {
					result.sec = 0;
				}
				nsec = 1000000000ULL + end.ns - start.ns;
			} else {
				result.sec = end.sec - start.sec;
				nsec = end.ns - start.ns;
			}

			result.ms = static_cast<uint32_t>(nsec / ns_in_millisecond);
			if (result.ms >= ms_in_second) {
				result.sec += result.ms / ms_in_second;
				result.ms = result.ms % ms_in_second;
			}

			result.ns = static_cast<uint32_t>(nsec  % ns_in_millisecond);
		}

		template<TimingPointType P, TimingIntervalType I> [[gnu::always_inline]]
		static void calculate_interval (P const& start, P const& end, I &result, uint64_t& total_ns) noexcept
		{
			calculate_interval (start, end, result);
			total_ns =
				(static_cast<uint64_t>(result.sec) * static_cast<uint64_t>(ns_in_second)) +
				(static_cast<uint64_t>(result.ms) * static_cast<uint64_t>(ns_in_millisecond)) +
				static_cast<uint64_t>(result.ns);
		}

		void dump () noexcept;

	private:
		static void really_initialize (bool log_immediately) noexcept;
		static void* timing_signal_thread (void *arg) noexcept;

		[[gnu::always_inline]]
		static void mark (TimingEventPoint &point) noexcept
		{
			get_time (point.sec, point.ns);
		}

		[[gnu::always_inline]]
		auto is_valid_event_index (size_t index, const char *method_name) noexcept -> bool
		{
			if (index >= events.capacity ()) [[unlikely]] {
				log_warn (LOG_TIMING, "Invalid event index passed to method '{}'", method_name);
				return false;
			}

			return true;
		}

		template<size_t BufferSize> [[gnu::always_inline]]
		static void append_event_kind_description (TimingEventKind kind, dynamic_local_string<BufferSize, char>& message) noexcept
		{
			switch (kind) {
				case TimingEventKind::AssemblyDecompression: {
					constexpr char desc[] = "LZ4 decompression time for ";
					message.append (desc);
					return;
				}

				case TimingEventKind::AssemblyLoad: {
					constexpr char desc[] = "Assembly load";
					message.append (desc);
					return;
				}

				case TimingEventKind::AssemblyPreload: {
					constexpr char desc[] = "Finished preloading, number of loaded assemblies: ";
					message.append (desc);
					return;
				}

				case TimingEventKind::DebugStart: {
					constexpr char desc[] = "Debug::start_debugging_and_profiling: end";
					message.append (desc);
					return;
				}

				case TimingEventKind::Init: {
					constexpr char desc[] = "XATiming: init time";
					message.append (desc);
					return;
				}

				case TimingEventKind::JavaToManaged: {
					constexpr char desc[] = "Typemap.java_to_managed: end, total time";
					message.append (desc);
					return;
				}

				case TimingEventKind::ManagedToJava: {
					constexpr char desc[] = "Typemap.managed_to_java: end, total time";
					message.append (desc);
					return;
				}

				case TimingEventKind::MonoRuntimeInit: {
					constexpr char desc[] = "Runtime.init: Mono runtime init";
					message.append (desc);
					return;
				}

				case TimingEventKind::NativeToManagedTransition: {
					constexpr char desc[] = "Runtime.init: end native-to-managed transition";
					message.append (desc);
					return;
				}

				case TimingEventKind::RuntimeConfigBlob: {
					constexpr char desc[] = "Register runtimeconfig binary blob";
					message.append (desc);
					return;
				}

				case TimingEventKind::RuntimeRegister: {
					constexpr char desc[] = "Runtime.register: end time. Registered type: ";
					message.append (desc);
					return;
				}

				case TimingEventKind::TotalRuntimeInit: {
					constexpr char desc[] = "Runtime.init: end, total time";
					message.append (desc);
					return;
				}

				default: {
					constexpr char desc[] = "Unknown timing event";
					message.append (desc);
					return;
				}
			}
		}

		//
		// Message format is as follows: <OPTIONAL_INDENT>[STAGE/EVENT] <MESSAGE>; elapsed s:ms::ns
		//
		//  STAGE is one of:
		//    0 - native init (before managed code runs)
		//    1 - managed code enabled
		//    2 - events summary (see the `dump()` function)
		//
		//  EVENT is one of:
		//    for stages 0 and 1, it's the value of the TimingEventKind member
		//    for stage 2 see the `dump()` function
		//
		// The [STAGE/EVENT] format is meant to help the test runner application, so that it can parse logcat without
		// having to be kept in sync with the actual wording used for the event message.
		//
		template<size_t BufferSize> [[gnu::always_inline]]
		static void format_and_log (TimingEvent const& event, TimingInterval const& interval, dynamic_local_string<BufferSize, char>& message, bool indent = false) noexcept
		{
			constexpr char INDENT[] = "  ";
			constexpr char NATIVE_INIT_TAG[] = "[0/";
			constexpr char MANAGED_TAG[] = "[1/";

			message.clear ();
			if (indent) {
				message.append (INDENT);
			}

			if (event.before_managed) {
				message.append (NATIVE_INIT_TAG);
			} else {
				message.append (MANAGED_TAG);
			}

			message.append (static_cast<uint32_t>(event.kind));
			message.append ("] ");

			append_event_kind_description (event.kind, message);
			if (event.more_info && !event.more_info->empty ()) {
				message.append (event.more_info->c_str (), event.more_info->length ());
			}

			constexpr char COLON[] = ":";
			constexpr char TWO_COLONS[] = "::";

			message.append ("; elapsed: ");
			message.append (static_cast<uint32_t>(interval.sec));
			message.append (COLON);
			message.append (interval.ms);
			message.append (TWO_COLONS);
			message.append (interval.ns);

			log_write (LOG_TIMING, LogLevel::Info, message.get ());
		}

		template<size_t BufferSize> [[gnu::always_inline]]
		static void format_and_log (TimingEvent const& event, dynamic_local_string<BufferSize, char>& message, uint64_t& total_ns, bool indent = false) noexcept
		{
			TimingInterval interval;
			calculate_interval (event.start, event.end, interval, total_ns);
			format_and_log (event, interval, message, indent);
		}

		[[gnu::always_inline]]
		static void format_and_log (TimingEvent const& event) noexcept
		{
			TimingInterval interval;
			calculate_interval (event.start, event.end, interval);

			// `message` isn't used here, it is passed to `format_and_log` so that the `dump()` function can
			// be slightly more efficient when dumping the event buffer.
			dynamic_local_string<Constants::MAX_LOGCAT_MESSAGE_LENGTH, char> message;
			format_and_log (event, interval, message);
		}

		[[gnu::always_inline]]
		static void log (TimingEvent const& event, bool skip_log_if_more_info_missing) noexcept
		{
			if (!immediate_logging) {
				return;
			}

			if (skip_log_if_more_info_missing && (!event.more_info || event.more_info->empty ())) {
				return;
			}

			format_and_log (event);
		}

		[[gnu::always_inline]]
		static void ns_to_time (uint64_t total_ns, uint32_t &sec, uint32_t &ms, uint32_t &ns) noexcept
		{
			sec = static_cast<uint32_t>(total_ns / ns_in_second);
			if (sec > 0) {
				total_ns = total_ns % 1000000000ULL;
			}

			ms = static_cast<uint32_t>(total_ns / ns_in_millisecond);
			if (ms >= 1000) {
				sec += ms / 1000;
				ms = ms % 1000;
			}

			ns = static_cast<uint32_t>(total_ns % ns_in_millisecond);
		}

	private:
		std::atomic_size_t next_event_index = 0uz;
		std::mutex event_vector_realloc_mutex;
		std::vector<TimingEvent> events;

		static inline TimingEvent init_time{};
		static inline bool is_enabled = false;
		static inline bool immediate_logging = false;
	};

	extern FastTiming internal_timing;
}
