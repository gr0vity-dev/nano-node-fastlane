#include <nano/lib/logging.hpp>
#include <nano/lib/utility.hpp>

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void nano::initialize_logging ()
{
	spdlog::cfg::load_env_levels ();
	spdlog::set_automatic_registration (false);

	//	auto logger = spdlog::basic_logger_mt ("default", "nano_log.txt");
	auto logger = spdlog::stdout_color_mt ("default");
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
	spd_logger = std::make_shared<spdlog::logger> (std::string{ nano::to_string (tag) }, sinks.begin (), sinks.end ());
	spdlog::initialize_logger (spd_logger);
	return spd_logger;
}