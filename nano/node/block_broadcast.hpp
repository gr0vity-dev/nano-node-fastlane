#pragma once

#include <nano/lib/blocks.hpp>
#include <nano/lib/locks.hpp>
#include <nano/lib/processing_queue.hpp>
#include <nano/node/blockprocessor.hpp>
#include <nano/secure/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

#include <memory>
#include <thread>
#include <unordered_set>

namespace mi = boost::multi_index;

namespace nano
{
class network;
}

namespace nano
{
/**
 * Broadcasts blocks to the network
 * Tracks local blocks for more aggressive propagation
 */
class block_broadcast
{
	enum class broadcast_strategy
	{
		normal,
		aggressive,
	};

	using entry = std::pair<std::shared_ptr<nano::block>, broadcast_strategy>;
	using queue_t = nano::processing_queue<entry>;

public:
	block_broadcast (nano::block_processor &, nano::network &, nano::stats &, bool enabled = false);
	~block_broadcast ();

	void start ();
	void stop ();

	// Mark a block as originating locally
	void track_local (nano::block_hash const &);

private: // Dependencies
	nano::block_processor & block_processor;
	nano::network & network;
	nano::stats & stats;

private:
	// Block_processor observer
	void observe (std::shared_ptr<nano::block> const & block, nano::block_processor::context const & context);
	void process_batch (queue_t::batch_t & batch);

private:
	class hash_tracker
	{
	public:
		void add (nano::block_hash const &);
		void erase (nano::block_hash const &);
		bool contains (nano::block_hash const &) const;

	private:
		mutable nano::mutex mutex;

		// clang-format off
		class tag_sequenced {};
		class tag_hash {};

		using ordered_hashes = boost::multi_index_container<nano::block_hash,
		mi::indexed_by<
			mi::sequenced<mi::tag<tag_sequenced>>,
			mi::hashed_unique<mi::tag<tag_hash>,
				mi::identity<nano::block_hash>>
		>>;
		// clang-format on

		// Blocks originated on this node
		ordered_hashes hashes;

		static std::size_t constexpr max_size = 1024 * 128;
	};
	hash_tracker local;

private:
	bool enabled{ false };
	queue_t queue;

	bool stopped{ false };
	nano::condition_variable condition;
	mutable nano::mutex mutex;
	std::thread thread;

	static std::size_t constexpr max_size = 1024 * 32;
};
}
