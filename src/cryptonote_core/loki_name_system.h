#ifndef LOKI_NAME_SYSTEM_H
#define LOKI_NAME_SYSTEM_H

#include "crypto/crypto.h"
#include "cryptonote_config.h"
#include "span.h"
#include "cryptonote_basic/tx_extra.h"

#include <string>

struct sqlite3;
struct sqlite3_stmt;
namespace cryptonote
{
struct checkpoint_t;
struct block;
struct transaction;
struct account_address;
struct tx_extra_loki_name_system;
class Blockchain;
}; // namespace cryptonote

namespace lns
{

constexpr size_t WALLET_NAME_MAX                  = 96;
constexpr size_t LOKINET_DOMAIN_NAME_MAX          = 253;
constexpr size_t LOKINET_ADDRESS_BINARY_LENGTH    = sizeof(crypto::ed25519_public_key);
constexpr size_t SESSION_DISPLAY_NAME_MAX         = 64;
constexpr size_t SESSION_PUBLIC_KEY_BINARY_LENGTH = 1 + sizeof(crypto::ed25519_public_key); // Session keys at prefixed with 0x05 + ed25519 key
constexpr size_t GENERIC_NAME_MAX                 = 255;
constexpr size_t GENERIC_VALUE_MAX                = 255;

struct lns_value
{
  std::array<uint8_t, lns::GENERIC_VALUE_MAX> buffer;
  size_t len;
};

enum struct burn_type
{
  none,
  update_record,
  lokinet_1year,
  session,
  wallet,
  custom,
};

inline std::ostream &operator<<(std::ostream &os, mapping_type type)
{
  switch(type)
  {
    case mapping_type::lokinet: os << "lokinet"; break;
    case mapping_type::session: os << "session"; break;
    case mapping_type::wallet:  os << "wallet"; break;
    default: assert(false);     os << "xx_unhandled_type"; break;
  }
  return os;
}

constexpr bool mapping_type_allowed(uint8_t hf_version, mapping_type type) { return type == mapping_type::session; }
burn_type    mapping_type_to_burn_type(mapping_type in);
uint64_t     burn_requirement_in_atomic_loki(uint8_t hf_version, burn_type type);
sqlite3     *init_loki_name_system(char const *file_path);
uint64_t     lokinet_expiry_blocks(cryptonote::network_type nettype, uint64_t *renew_window = nullptr);
crypto::hash tx_extra_signature_hash(epee::span<const uint8_t> blob, crypto::hash const &prev_txid);
bool         validate_lns_name(mapping_type type, std::string const &name, std::string *reason = nullptr);

// blob: if set, validate_lns_value will convert the value into the binary format suitable for storing into the LNS DB.
bool         validate_lns_value(cryptonote::network_type nettype, mapping_type type, std::string const &value, lns_value *blob = nullptr, std::string *reason = nullptr);
bool         validate_lns_value_binary(mapping_type type, std::string const &value, std::string *reason = nullptr);
bool         validate_mapping_type(std::string const &type, lns::mapping_type *mapping_type, std::string *reason);

struct owner_record
{
  operator bool() const { return loaded; }
  bool loaded;

  int64_t id;
  crypto::ed25519_public_key key;
};

struct settings_record
{
  operator bool() const { return loaded; }
  bool loaded;

  uint64_t     top_height;
  crypto::hash top_hash;
  int          version;
};

struct mapping_record
{
  // NOTE: We keep expired entries in the DB indefinitely because we need to
  // keep all LNS entries indefinitely to support large blockchain detachments.
  // A mapping_record forms a linked list of TXID's which allows us to revert
  // the LNS DB to any arbitrary height at a small additional storage cost.
  // return: if the record is still active and hasn't expired.
  bool active(cryptonote::network_type nettype, uint64_t blockchain_height) const;
  operator bool() const { return loaded; }

  bool                       loaded;
  mapping_type               type; // alias to lns::mapping_type
  std::string                name;
  std::string                value;
  uint64_t                   register_height;
  int64_t                    owner_id;
  crypto::ed25519_public_key owner;
  crypto::hash               txid;
  crypto::hash               prev_txid;
};

struct name_system_db
{
  bool            init        (cryptonote::network_type nettype, sqlite3 *db, uint64_t top_height, crypto::hash const &top_hash);
  bool            add_block   (const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs);
  void            block_detach(cryptonote::Blockchain const &blockchain, uint64_t height);
  uint64_t        height      () { return last_processed_height; }

  bool            save_owner      (crypto::ed25519_public_key const &key, int64_t *row_id);
  bool            save_mapping   (crypto::hash const &tx_hash, cryptonote::tx_extra_loki_name_system const &src, uint64_t height, int64_t owner_id);
  bool            save_settings  (uint64_t top_height, crypto::hash const &top_hash, int version);

  // NOTE: Delete all mappings that are registered on height or newer followed by deleting all owners no longer referenced in the DB
  bool            prune_db(uint64_t height);

  owner_record                get_owner_by_key      (crypto::ed25519_public_key const &key) const;
  owner_record                get_owner_by_id       (int64_t owner_id) const;
  mapping_record              get_mapping           (mapping_type type, std::string const &name) const;
  std::vector<mapping_record> get_mappings          (std::vector<uint16_t> const &types, std::string const &name) const;
  std::vector<mapping_record> get_mappings_by_owner (crypto::ed25519_public_key const &key) const;
  std::vector<mapping_record> get_mappings_by_owners(std::vector<crypto::ed25519_public_key> const &keys) const;
  settings_record             get_settings          () const;

  bool                        validate_lns_tx(uint8_t hf_version, uint64_t blockchain_height, cryptonote::transaction const &tx, cryptonote::tx_extra_loki_name_system *entry = nullptr, std::string *reason = nullptr) const;
  cryptonote::network_type    network_type() const { return nettype; }

  sqlite3                  *db                       = nullptr;
  bool                      transaction_begun        = false;
private:
  cryptonote::network_type  nettype;
  uint64_t last_processed_height                     = 0;
  sqlite3_stmt *save_owner_sql                       = nullptr;
  sqlite3_stmt *save_mapping_sql                     = nullptr;
  sqlite3_stmt *save_settings_sql                    = nullptr;
  sqlite3_stmt *get_owner_by_key_sql                 = nullptr;
  sqlite3_stmt *get_owner_by_id_sql                  = nullptr;
  sqlite3_stmt *get_mapping_sql                      = nullptr;
  sqlite3_stmt *get_settings_sql                     = nullptr;
  sqlite3_stmt *prune_mappings_sql                   = nullptr;
  sqlite3_stmt *prune_owners_sql                     = nullptr;
  sqlite3_stmt *get_mappings_by_owner_sql            = nullptr;
  sqlite3_stmt *get_mappings_on_height_and_newer_sql = nullptr;
};

}; // namespace service_nodes
#endif // LOKI_NAME_SYSTEM_H
