#pragma once

#include <cstdint>
#include <string_view>

namespace nano::log
{
enum class tag : uint8_t
{
	all = 0, // reserved

	generic,
	node,
	daemon,
	network,
	election,
	blockprocessor,
	rpc_callbacks,
	prunning,
	wallet,
	bulk_pull_client,
	bulk_pull_server,
	bulk_pull_account_client,
	bulk_pull_account_server,
	bulk_push_client,
	bulk_push_server,
	active_transactions,
};

enum class detail
{
	all = 0, // reserved

	// blockprocessor
	block_processed,

	//node
	process_confirmed,

	//active_transactions
	active_started,
	active_stopped,

	//election
	generate_vote_normal,
	generate_vote_final,


	// network
	message_received,

	// bulk pull/push
	pulled_block,
	sending_block,
	sending_pending,
};
}

namespace nano
{
std::string_view to_string (nano::log::tag);
std::string_view to_string (nano::log::detail);
}
