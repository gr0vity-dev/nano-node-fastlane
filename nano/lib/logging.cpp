#include <nano/lib/logging.hpp>
#include <nano/lib/utility.hpp>
#include <boost/filesystem.hpp>

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

void nano::initialize_logging ()
{
	spdlog::cfg::load_env_levels ();
	spdlog::set_automatic_registration (false);

	// Set log file path
	boost::filesystem::path current_dir = boost::filesystem::current_path();
	boost::filesystem::path file_name = current_dir / "NanoTest" / "spdlog" / "node.log";
	auto const max_size = 1048576000; // max size per file in bytes (1000MB in this example)
	auto const max_files = 5; // maximum number of log files to keep

	// Create a file rotating logger
	auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt> (file_name.string (), max_size, max_files);

	// Create a color console sink
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt> ();

	// Combine both in a multi sink
	std::vector<spdlog::sink_ptr> sinks{ file_sink, console_sink };

	// Create a logger with both console and file logging
	auto logger = std::make_shared<spdlog::logger> ("default", sinks.begin (), sinks.end ());

	spdlog::set_default_logger (logger);
}

/*
 * nlogger
 */

nano::nlogger::nlogger ()
{
}

spdlog::logger & nano::nlogger::get_logger (nano::log::tag tag)
{
	std::shared_lock slock{ mutex };

	if (auto it = spd_loggers.find (tag); it != spd_loggers.end ())
	{
		return *it->second;
	}
	else
	{
		slock.unlock ();
		std::unique_lock lock{ mutex };

		auto [it2, inserted] = spd_loggers.emplace (tag, make_logger (tag));
		debug_assert (inserted);
		return *it2->second;
	}
}

std::shared_ptr<spdlog::logger> nano::nlogger::make_logger (nano::log::tag tag)
{
	auto const & sinks = spdlog::default_logger ()->sinks ();
	auto spd_logger = std::make_shared<spdlog::logger> (std::string{ nano::to_string (tag) }, sinks.begin (), sinks.end ());
	spdlog::initialize_logger (spd_logger);
	return spd_logger;
}

spdlog::level::level_enum nano::nlogger::to_spd_level (nano::log::level level)
{
	switch (level)
	{
		case nano::log::level::off:
			return spdlog::level::off;
		case nano::log::level::critical:
			return spdlog::level::critical;
		case nano::log::level::error:
			return spdlog::level::err;
		case nano::log::level::warn:
			return spdlog::level::warn;
		case nano::log::level::info:
			return spdlog::level::info;
		case nano::log::level::debug:
			return spdlog::level::debug;
		case nano::log::level::trace:
			return spdlog::level::trace;
		default:
			debug_assert (false);
			return spdlog::level::off;
	}
}