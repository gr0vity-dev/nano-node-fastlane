#include <nano/lib/stats.hpp>
#include <nano/node/node.hpp>
#include <nano/node/scheduler/hinted.hpp>

nano::scheduler::hinted::config::config (nano::node_config const & config) :
	vote_cache_check_interval_ms{ config.network_params.network.is_dev_network () ? 100u : 1000u }
{
}

nano::scheduler::hinted::hinted (config const & config_a, nano::node & node_a, nano::vote_cache & inactive_vote_cache_a, nano::active_transactions & active_a, nano::online_reps & online_reps_a, nano::stats & stats_a) :
	config_m{ config_a },
	node{ node_a },
	inactive_vote_cache{ inactive_vote_cache_a },
	active{ active_a },
	online_reps{ online_reps_a },
	stats{ stats_a }
{
}

nano::scheduler::hinted::~hinted ()
{
	// Thread must be stopped before destruction
	debug_assert (!thread.joinable ());
}

void nano::scheduler::hinted::start ()
{
	debug_assert (!thread.joinable ());

	thread = std::thread{ [this] () {
		nano::thread_role::set (nano::thread_role::name::election_hinting);
		run ();
	} };
}

void nano::scheduler::hinted::stop ()
{
	{
		nano::lock_guard<nano::mutex> lock{ mutex };
		stopped = true;
	}
	notify ();
	nano::join_or_pass (thread);
}

void nano::scheduler::hinted::notify ()
{
	//	condition.notify_all ();
}

bool nano::scheduler::hinted::predicate (nano::uint128_t const & minimum_tally) const
{
	// Check if there is space inside AEC for a new hinted election
	if (active.vacancy (nano::election_behavior::hinted) > 0)
	{
		//		// Check if there is any vote cache entry surpassing our minimum vote tally threshold
		//		if (inactive_vote_cache.peek (minimum_tally))
		//		{
		//			return true;
		//		}

		return true;
	}
	return false;
}

bool nano::scheduler::hinted::run_one (nano::uint128_t const & minimum_tally)
{
	if (auto top = inactive_vote_cache.pop (minimum_tally); top)
	{
		const auto hash = top->hash (); // Hash of block we want to hint

		// Check if block exists
		auto block = node.block (hash);
		if (block != nullptr)
		{
			// Ensure block is not already confirmed
			if (!node.block_confirmed_or_being_confirmed (hash))
			{
				// Try to insert it into AEC as hinted election
				// We check for AEC vacancy inside our predicate
				auto result = node.active.insert (block, nano::election_behavior::hinted);

				stats.inc (nano::stat::type::hinting, result.inserted ? nano::stat::detail::insert : nano::stat::detail::insert_failed);

				return result.inserted; // Return whether block was inserted
			}
		}
		else
		{
			// Missing block in ledger to start an election
			node.bootstrap_block (hash);

			stats.inc (nano::stat::type::hinting, nano::stat::detail::missing_block);
		}
	}
	return false;
}


void nano::scheduler::hinted::activate (const nano::transaction & transaction, const nano::block_hash & hash)
{
	// Check if block exists
	if (auto block = node.store.block.get (transaction, hash); block)
	{
		// Ensure block is not already confirmed
		if (node.block_confirmed_or_being_confirmed (transaction, hash))
		{
			stats.inc (nano::stat::type::hinting, nano::stat::detail::already_confirmed);
			return;
		}

		// Try to insert it into AEC as hinted election
		// We check for AEC vacancy inside the predicate
		auto result = node.active.insert (block, nano::election_behavior::hinted);
		stats.inc (nano::stat::type::hinting, result.inserted ? nano::stat::detail::insert : nano::stat::detail::insert_failed);
	}
	else
	{
		// Missing block in ledger to start an election
		stats.inc (nano::stat::type::hinting, nano::stat::detail::missing_block);
		node.bootstrap_block (hash);
	}
}

// bool nano::scheduler::hinted::activate (const nano::transaction & transaction, const nano::block_hash & hash, bool check_dependents)
// {
// 	// Check if block exists
// 	if (auto block = node.store.block.get (transaction, hash); block != nullptr)
// 	{
// 		// Ensure block is not already confirmed
// 		if (node.block_confirmed_or_being_confirmed (transaction, hash))
// 		{
// 			stats.inc (nano::stat::type::hinting, nano::stat::detail::already_confirmed);
// 			return false;
// 		}
// 		if (check_dependents && !node.ledger.dependents_confirmed (transaction, *block))
// 		{
// 			stats.inc (nano::stat::type::hinting, nano::stat::detail::dependent_unconfirmed);
// 			activate_dependents (transaction, *block);
// 			return false;
// 		}

// 		// Try to insert it into AEC as hinted election
// 		// We check for AEC vacancy inside our predicate
// 		auto result = node.active.insert (block, nano::election_behavior::hinted);
// 		stats.inc (nano::stat::type::hinting, result.inserted ? nano::stat::detail::insert : nano::stat::detail::insert_failed);
// 		return true;
// 	}
// 	else
// 	{
// 		// Missing block in ledger to start an election
// 		stats.inc (nano::stat::type::hinting, nano::stat::detail::missing_block);
// 		node.bootstrap_block (hash);
// 	}

// 	return false;
// }



// void nano::scheduler::hinted::activate_dependents (const nano::transaction & transaction, const nano::block & block)
// {
// 	auto dependents = node.ledger.dependent_blocks (transaction, block);
// 	for (auto const & hash : dependents)
// 	{
// 		if (!hash.is_zero ())
// 		{
// 			bool activated = activate (transaction, hash, /* check dependents */ true);
// 			if (activated)
// 			{
// 				stats.inc (nano::stat::type::hinting, nano::stat::detail::dependent_activated);
// 			}
// 		}
// 	}
// }

void nano::scheduler::hinted::activate_with_dependents (const nano::transaction & transaction, const nano::block_hash & hash)
{
	std::stack<nano::block_hash> stack;
	stack.push (hash);

	while (!stack.empty ())
	{
		const nano::block_hash current_hash = stack.top ();
		stack.pop ();

		// Check if block exists
		if (auto block = node.store.block.get (transaction, current_hash); block)
		{
			// Ensure block is not already confirmed
			if (node.block_confirmed_or_being_confirmed (transaction, current_hash))
			{
				stats.inc (nano::stat::type::hinting, nano::stat::detail::already_confirmed);
				continue; // Move on to the next item in the stack
			}

			if (!node.ledger.dependents_confirmed (transaction, *block))
			{
				stats.inc (nano::stat::type::hinting, nano::stat::detail::dependent_unconfirmed);
				auto dependents = node.ledger.dependent_blocks (transaction, *block);
				for (const auto & dependent_hash : dependents)
				{
					if (!dependent_hash.is_zero ())
					{
						stack.push (dependent_hash); // Add dependent block to the stack
					}
				}
				continue; // Move on to the next item in the stack
			}

			// Try to insert it into AEC as hinted election
			auto result = node.active.insert (block, nano::election_behavior::hinted);
			stats.inc (nano::stat::type::hinting, result.inserted ? nano::stat::detail::insert : nano::stat::detail::insert_failed);
		}
		else
		{
			stats.inc (nano::stat::type::hinting, nano::stat::detail::missing_block);
			node.bootstrap_block (current_hash);
		}
	}
}

void nano::scheduler::hinted::run_iterative ()
{
	const auto minimum_tally = tally_threshold ();
	const auto minimum_final_tally = final_tally_threshold ();

	auto transaction = node.store.tx_begin_read ();

	// Blocks with a vote tally higher than quorum
	// Can be activated and confirmed immediately
	for (auto const & entry : inactive_vote_cache.top_final (minimum_final_tally))
	{
		if (!predicate (0))
		{
			break;
		}

		if (cooldown (entry.hash))
		{
			continue;
		}

		stats.inc (nano::stat::type::hinting, nano::stat::detail::activate_final);
		activate (transaction, entry.hash);
	}

	// Block with highest observed tally, might not be final
	// Ensure all dependent blocks are already confirmed before activating
	for (auto const & entry : inactive_vote_cache.top (minimum_tally))
	{
		if (!predicate (0))
		{
			break;
		}

		if (cooldown (entry.hash))
		{
			continue;
		}

		stats.inc (nano::stat::type::hinting, nano::stat::detail::activate_normal);
		activate_with_dependents (transaction, entry.hash);
	}
}

void nano::scheduler::hinted::run ()
{
	nano::unique_lock<nano::mutex> lock{ mutex };
	while (!stopped)
	{
		stats.inc (nano::stat::type::hinting, nano::stat::detail::loop);

		// It is possible that if we are waiting long enough this tally becomes outdated due to changes in trended online weight
		// However this is only used here for hinting, election does independent tally calculation, so there is no need to ensure it's always up-to-date
		const auto minimum_tally = tally_threshold ();

		// Periodically wakeup for condition checking
		// We are not notified every time new vote arrives in inactive vote cache as that happens too often
		condition.wait_for (lock, std::chrono::milliseconds (config_m.vote_cache_check_interval_ms), [this, minimum_tally] () {
			return stopped || predicate (minimum_tally);
		});

		debug_assert ((std::this_thread::yield (), true)); // Introduce some random delay in debug builds

		if (!stopped)
		{
			// We don't need the lock when running main loop
			lock.unlock ();

			if (predicate (minimum_tally))
			{
				//				run_one (minimum_tally);
				run_iterative ();
			}

			lock.lock ();
		}
	}
}

bool nano::scheduler::hinted::cooldown (const nano::block_hash & hash)
{
	auto const now = std::chrono::steady_clock::now ();
	std::chrono::milliseconds check_interval{ 5000 };

	// Check if the hash is still in the cooldown period using the hashed index
	auto const & hashed_index = cooldowns_m.get<tag_hash> ();
	if (auto it = hashed_index.find (hash); it != hashed_index.end ())
	{
		if (it->timeout > now)
		{
			return true; // Needs cooldown
		}
		cooldowns_m.erase (it); // Entry is outdated, so remove it
	}

	// Insert the new entry
	cooldowns_m.insert ({ hash, now + check_interval });

	// Trim old entries
	auto & seq_index = cooldowns_m.get<tag_timeout> ();
	while (!seq_index.empty () && seq_index.begin ()->timeout <= now)
	{
		seq_index.erase (seq_index.begin ());
	}

	return false; // No need to cooldown
}

nano::uint128_t nano::scheduler::hinted::tally_threshold () const
{
	//	auto min_tally = (online_reps.trended () / 100) * node.config.election_hint_weight_percent;
	//	return min_tally;

	return 0;
}

nano::uint128_t nano::scheduler::hinted::final_tally_threshold () const
{
	auto quorum = online_reps.delta ();
	return quorum;
}