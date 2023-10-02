#include <nano/node/lmdb/block_store.hpp>
#include <nano/node/lmdb/lmdb.hpp>
#include <nano/secure/parallel_traversal.hpp>

namespace nano
{
/**
 * Fill in our predecessors
 */
class block_predecessor_mdb_set : public nano::block_visitor
{
public:
	block_predecessor_mdb_set (nano::write_transaction const & transaction_a, nano::lmdb::block_store & block_store_a);
	virtual ~block_predecessor_mdb_set () = default;
	void fill_value (nano::block const & block_a);
	void send_block (nano::send_block const & block_a) override;
	void receive_block (nano::receive_block const & block_a) override;
	void open_block (nano::open_block const & block_a) override;
	void change_block (nano::change_block const & block_a) override;
	void state_block (nano::state_block const & block_a) override;
	nano::write_transaction const & transaction;
	nano::lmdb::block_store & block_store;
};
}

nano::lmdb::block_store::block_store (nano::lmdb::store & store_a) :
	store{ store_a } {};

void nano::lmdb::block_store::put (nano::write_transaction const & transaction, nano::block_hash const & hash, nano::block const & block)
{
	debug_assert (block.sideband ().successor.is_zero () || exists (transaction, block.sideband ().successor));
	std::vector<uint8_t> vector;
	{
		nano::vectorstream stream (vector);
		nano::serialize_block (stream, block);
		block.sideband ().serialize (stream, block.type ());
	}
	raw_put (transaction, vector, hash);
	block_predecessor_mdb_set predecessor (transaction, *this);
	block.visit (predecessor);
	debug_assert (block.previous ().is_zero () || successor (transaction, block.previous ()) == hash);
}

void nano::lmdb::block_store::raw_put (nano::write_transaction const & transaction_a, std::vector<uint8_t> const & data, nano::block_hash const & hash_a)
{
	nano::mdb_val value{ data.size (), (void *)data.data () };
	auto status = store.put (transaction_a, tables::blocks, hash_a, value);
	store.release_assert_success (status);
}

nano::block_hash nano::lmdb::block_store::successor (nano::transaction const & transaction_a, nano::block_hash const & hash_a) const
{
	nano::mdb_val value;
	block_raw_get (transaction_a, hash_a, value);
	nano::block_hash result;
	if (value.size () != 0)
	{
		debug_assert (value.size () >= result.bytes.size ());
		auto type = block_type_from_raw (value.data ());
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()) + block_successor_offset (transaction_a, value.size (), type), result.bytes.size ());
		auto error (nano::try_read (stream, result.bytes));
		(void)error;
		debug_assert (!error);
	}
	else
	{
		result.clear ();
	}
	return result;
}

void nano::lmdb::block_store::successor_clear (nano::write_transaction const & transaction, nano::block_hash const & hash)
{
	nano::mdb_val value;
	block_raw_get (transaction, hash, value);
	debug_assert (value.size () != 0);
	auto type = block_type_from_raw (value.data ());
	std::vector<uint8_t> data (static_cast<uint8_t *> (value.data ()), static_cast<uint8_t *> (value.data ()) + value.size ());
	std::fill_n (data.begin () + block_successor_offset (transaction, value.size (), type), sizeof (nano::block_hash), uint8_t{ 0 });
	raw_put (transaction, data, hash);
}

std::shared_ptr<nano::block> nano::lmdb::block_store::get (nano::transaction const & transaction, nano::block_hash const & hash) const
{
	nano::mdb_val value;
	block_raw_get (transaction, hash, value);
	std::shared_ptr<nano::block> result;
	if (value.size () != 0)
	{
		nano::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
		nano::block_type type;
		auto error (try_read (stream, type));
		release_assert (!error);
		result = nano::deserialize_block (stream, type);
		release_assert (result != nullptr);
		nano::block_sideband sideband;
		error = (sideband.deserialize (stream, type));
		release_assert (!error);
		result->sideband_set (sideband);
	}
	return result;
}

std::shared_ptr<nano::block> nano::lmdb::block_store::random (nano::transaction const & transaction)
{
	nano::block_hash hash;
	nano::random_pool::generate_block (hash.bytes.data (), hash.bytes.size ());
	auto existing = begin (transaction, hash);
	if (existing == end ())
	{
		existing = begin (transaction);
	}
	debug_assert (existing != end ());
	return existing->second.block;
}

void nano::lmdb::block_store::del (nano::write_transaction const & transaction_a, nano::block_hash const & hash_a)
{
	auto status = store.del (transaction_a, tables::blocks, hash_a);
	store.release_assert_success (status);
}

bool nano::lmdb::block_store::exists (nano::transaction const & transaction, nano::block_hash const & hash)
{
	nano::mdb_val junk;
	block_raw_get (transaction, hash, junk);
	return junk.size () != 0;
}

uint64_t nano::lmdb::block_store::count (nano::transaction const & transaction_a)
{
	return store.count (transaction_a, tables::blocks);
}

nano::store_iterator<nano::block_hash, nano::block_w_sideband> nano::lmdb::block_store::begin (nano::transaction const & transaction) const
{
	return store.make_iterator<nano::block_hash, nano::block_w_sideband> (transaction, tables::blocks);
}

nano::store_iterator<nano::block_hash, nano::block_w_sideband> nano::lmdb::block_store::begin (nano::transaction const & transaction, nano::block_hash const & hash) const
{
	return store.make_iterator<nano::block_hash, nano::block_w_sideband> (transaction, tables::blocks, hash);
}

nano::store_iterator<nano::block_hash, nano::block_w_sideband> nano::lmdb::block_store::end () const
{
	return nano::store_iterator<nano::block_hash, nano::block_w_sideband> (nullptr);
}

void nano::lmdb::block_store::for_each_par (std::function<void (nano::read_transaction const &, nano::store_iterator<nano::block_hash, block_w_sideband>, nano::store_iterator<nano::block_hash, block_w_sideband>)> const & action_a) const
{
	parallel_traversal<nano::uint256_t> (
	[&action_a, this] (nano::uint256_t const & start, nano::uint256_t const & end, bool const is_last) {
		auto transaction (this->store.tx_begin_read ());
		action_a (transaction, this->begin (transaction, start), !is_last ? this->begin (transaction, end) : this->end ());
	});
}

void nano::lmdb::block_store::block_raw_get (nano::transaction const & transaction, nano::block_hash const & hash, nano::mdb_val & value) const
{
	auto status = store.get (transaction, tables::blocks, hash, value);
	release_assert (store.success (status) || store.not_found (status));
}

size_t nano::lmdb::block_store::block_successor_offset (nano::transaction const & transaction_a, size_t entry_size_a, nano::block_type type_a) const
{
	return entry_size_a - nano::block_sideband::size (type_a);
}

nano::block_type nano::lmdb::block_store::block_type_from_raw (void * data_a)
{
	// The block type is the first byte
	return static_cast<nano::block_type> ((reinterpret_cast<uint8_t const *> (data_a))[0]);
}

nano::block_predecessor_mdb_set::block_predecessor_mdb_set (nano::write_transaction const & transaction_a, nano::lmdb::block_store & block_store_a) :
	transaction{ transaction_a },
	block_store{ block_store_a }
{
}
void nano::block_predecessor_mdb_set::fill_value (nano::block const & block_a)
{
	auto hash = block_a.hash ();
	nano::mdb_val value;
	block_store.block_raw_get (transaction, block_a.previous (), value);
	debug_assert (value.size () != 0);
	auto type = block_store.block_type_from_raw (value.data ());
	std::vector<uint8_t> data (static_cast<uint8_t *> (value.data ()), static_cast<uint8_t *> (value.data ()) + value.size ());
	std::copy (hash.bytes.begin (), hash.bytes.end (), data.begin () + block_store.block_successor_offset (transaction, value.size (), type));
	block_store.raw_put (transaction, data, block_a.previous ());
}
void nano::block_predecessor_mdb_set::send_block (nano::send_block const & block_a)
{
	fill_value (block_a);
}
void nano::block_predecessor_mdb_set::receive_block (nano::receive_block const & block_a)
{
	fill_value (block_a);
}
void nano::block_predecessor_mdb_set::open_block (nano::open_block const & block_a)
{
	// Open blocks don't have a predecessor
}
void nano::block_predecessor_mdb_set::change_block (nano::change_block const & block_a)
{
	fill_value (block_a);
}
void nano::block_predecessor_mdb_set::state_block (nano::state_block const & block_a)
{
	if (!block_a.previous ().is_zero ())
	{
		fill_value (block_a);
	}
}
