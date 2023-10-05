#pragma once

#include <nano/lib/locks.hpp>
#include <nano/lib/numbers.hpp>
#include <nano/secure/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <chrono>
#include <condition_variable>
#include <thread>
#include <unordered_map>

namespace mi = boost::multi_index;
namespace nano
{
class node;
class node_config;
class active_transactions;
class vote_cache;
class online_reps;
}
namespace nano::scheduler
{

class hinted_config final
{
public:
	explicit hinted_config (nano::network_constants const &);

	nano::error deserialize (nano::tomlconfig & toml);
	nano::error serialize (nano::tomlconfig & toml) const;

public:
	std::chrono::milliseconds check_interval{ 5000 };
	std::chrono::milliseconds block_cooldown{ 10000 };
	uint hinting_threshold_percent{ 10 };
};

/*
 * Monitors inactive vote cache and schedules elections with the highest observed vote tally.
 */
class hinted final
{
public: // Config
	struct config final
	{
		explicit config (node_config const & config);
		// Interval of wakeup to check inactive vote cache when idle
		uint64_t vote_cache_check_interval_ms;
	};

public:
	hinted (config const &, nano::node &, nano::vote_cache &, nano::active_transactions &, nano::online_reps &, nano::stats &);
	~hinted ();

	void start ();
	void stop ();

	/*
	 * Notify about changes in AEC vacancy
	 */
	void notify ();

private:
	bool predicate (nano::uint128_t const & minimum_tally) const;
	void run ();
	bool run_one (nano::uint128_t const & minimum_tally);

	void run_iterative ();
	// bool activate (nano::transaction const &, nano::block_hash const & hash, bool check_dependents);
	void activate (nano::transaction const &, nano::block_hash const & hash);
	// void activate_dependents (nano::transaction const &, nano::block const & block);
	void activate_with_dependents (nano::transaction const &, nano::block_hash const & hash);

	nano::uint128_t tally_threshold () const;
	nano::uint128_t final_tally_threshold () const;

private: // Dependencies
	nano::node & node;
	nano::vote_cache & inactive_vote_cache;
	nano::active_transactions & active;
	nano::online_reps & online_reps;
	nano::stats & stats;

private:
	config const config_m;

	bool stopped{ false };
	nano::condition_variable condition;
	mutable nano::mutex mutex;
	std::thread thread;

private:
	bool cooldown (nano::block_hash const & hash);

	struct cooldown_entry
	{
		nano::block_hash hash;
		std::chrono::steady_clock::time_point timeout;
	};

	// clang-format off
	class tag_hash {};
	class tag_timeout {};
	// clang-format on

	// clang-format off
	using ordered_cooldowns = boost::multi_index_container<cooldown_entry,
	mi::indexed_by<
		mi::hashed_unique<mi::tag<tag_hash>,
			mi::member<cooldown_entry, nano::block_hash, &cooldown_entry::hash>>,
		mi::ordered_non_unique<mi::tag<tag_timeout>,
			mi::member<cooldown_entry, std::chrono::steady_clock::time_point, &cooldown_entry::timeout>>
	>>;
	// clang-format on

	ordered_cooldowns cooldowns_m;
};
}
