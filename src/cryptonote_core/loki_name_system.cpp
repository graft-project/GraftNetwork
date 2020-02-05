#include "loki_name_system.h"

#include "checkpoints/checkpoints.h"
#include "common/loki.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_core/blockchain.h"

#include <sqlite3.h>

extern "C"
{
#include "sodium.h"
}

#undef LOKI_DEFAULT_LOG_CATEGORY
#define LOKI_DEFAULT_LOG_CATEGORY "lns"

namespace lns
{
enum struct lns_sql_type
{
  save_user,
  save_setting,
  save_mapping,
  pruning,

  get_sentinel_start,
  get_user,
  get_setting,
  get_mapping,
  get_mappings_by_user,
  get_mappings_on_height_and_newer,
  get_sentinel_end,
};

enum struct lns_db_setting_row
{
  id,
  top_height,
  top_hash,
  version,
};

enum struct user_record_row
{
  id,
  public_key,
};

enum struct mapping_record_row
{
  id,
  type,
  name,
  value,
  txid,
  prev_txid,
  register_height,
  user_id,
};

static bool sql_copy_blob(sqlite3_stmt *statement, int row, void *dest, int dest_size)
{
  void const *blob = sqlite3_column_blob(statement, row);
  int blob_len     = sqlite3_column_bytes(statement, row);
  assert(blob_len == dest_size);
  if (blob_len != dest_size)
  {
    LOG_PRINT_L0("Unexpected blob size=" << blob_len << ", in LNS DB does not match expected size=" << dest_size);
    return false;
  }

  memcpy(dest, blob, blob_len);
  return true;
}

static bool sql_run_statement(cryptonote::network_type nettype, lns_sql_type type, sqlite3_stmt *statement, void *context)
{
  bool data_loaded = false;
  bool result      = false;

  for (bool infinite_loop = true; infinite_loop;)
  {
    int step_result = sqlite3_step(statement);
    switch (step_result)
    {
      case SQLITE_ROW:
      {
        switch (type)
        {
          default:
          {
            MERROR("Unhandled lns type enum with value: " << (int)type << ", in: " << __func__);
          }
          break;

          case lns_sql_type::get_user:
          {
            auto *entry = reinterpret_cast<user_record *>(context);
            entry->id   = sqlite3_column_int(statement, static_cast<int>(user_record_row::id));
            if (!sql_copy_blob(statement, static_cast<int>(user_record_row::public_key), entry->key.data, sizeof(entry->key.data)))
              return false;
            data_loaded = true;
          }
          break;

          case lns_sql_type::get_setting:
          {
            auto *entry       = reinterpret_cast<settings_record *>(context);
            entry->top_height = static_cast<uint64_t>(sqlite3_column_int64(statement, static_cast<int>(lns_db_setting_row::top_height)));
            if (!sql_copy_blob(statement, static_cast<int>(lns_db_setting_row::top_hash), entry->top_hash.data, sizeof(entry->top_hash.data)))
              return false;
            entry->version = sqlite3_column_int(statement, static_cast<int>(lns_db_setting_row::version));
            data_loaded = true;
          }
          break;

          case lns_sql_type::get_mappings_on_height_and_newer: /* FALLTHRU */
          case lns_sql_type::get_mappings_by_user: /* FALLTHRU */
          case lns_sql_type::get_mapping:
          {
            mapping_record tmp_entry = {};
            tmp_entry.type = static_cast<uint16_t>(sqlite3_column_int(statement, static_cast<int>(mapping_record_row::type)));
            tmp_entry.register_height = static_cast<uint16_t>(sqlite3_column_int(statement, static_cast<int>(mapping_record_row::register_height)));
            tmp_entry.user_id = sqlite3_column_int(statement, static_cast<int>(mapping_record_row::user_id));

            int name_len  = sqlite3_column_bytes(statement, static_cast<int>(mapping_record_row::name));
            auto *name    = reinterpret_cast<char const *>(sqlite3_column_text(statement, static_cast<int>(mapping_record_row::name)));

            size_t value_len = static_cast<size_t>(sqlite3_column_bytes(statement, static_cast<int>(mapping_record_row::value)));
            auto *value      = reinterpret_cast<char const *>(sqlite3_column_text(statement, static_cast<int>(mapping_record_row::value)));

            tmp_entry.name  = std::string(name, name_len);
            tmp_entry.value = std::string(value, value_len);

            if (!sql_copy_blob(statement, static_cast<int>(mapping_record_row::txid), tmp_entry.txid.data, sizeof(tmp_entry.txid)))
              return false;

            if (!sql_copy_blob(statement, static_cast<int>(mapping_record_row::prev_txid), tmp_entry.prev_txid.data, sizeof(tmp_entry.prev_txid)))
              return false;

            data_loaded = true;
            if (type == lns_sql_type::get_mappings_by_user || type == lns_sql_type::get_mappings_on_height_and_newer)
            {
              auto *records = reinterpret_cast<std::vector<mapping_record> *>(context);
              records->emplace_back(std::move(tmp_entry));
            }
            else
            {
              mapping_record *entry = reinterpret_cast<mapping_record *>(context);
              *entry                = std::move(tmp_entry);
            }
          }
          break;
        }
      }
      break;

      case SQLITE_BUSY: break;
      case SQLITE_DONE:
      {
        infinite_loop = false;
        result        = (type > lns_sql_type::get_sentinel_start && type < lns_sql_type::get_sentinel_end) ? data_loaded : true;
        break;
      }

      default:
      {
        LOG_PRINT_L1("Failed to execute statement: " << sqlite3_sql(statement) <<", reason: " << sqlite3_errstr(step_result));
        infinite_loop = false;
        break;
      }
    }
  }

  sqlite3_reset(statement);
  sqlite3_clear_bindings(statement);
  return result;
}

bool mapping_record::active(cryptonote::network_type nettype, uint64_t blockchain_height) const
{
    if (!loaded) return false;
    if (type != static_cast<uint16_t>(mapping_type::lokinet)) return true;
    uint64_t expiry_blocks            = lns::lokinet_expiry_blocks(nettype);
    uint64_t const last_active_height = register_height + expiry_blocks;
    return last_active_height >= (blockchain_height - 1);
}

static bool sql_compile_statement(sqlite3 *db, char const *query, int query_len, sqlite3_stmt **statement)
{
  int prepare_result = sqlite3_prepare_v2(db, query, query_len, statement, nullptr);
  bool result        = prepare_result == SQLITE_OK;
  if (!result) MERROR("Can not compile SQL statement: " << query << ", reason: " << sqlite3_errstr(prepare_result));
  return result;
}

uint64_t burn_requirement_in_atomic_loki(uint8_t hf_version)
{
  (void)hf_version;
  return 30 * COIN;
}

sqlite3 *init_loki_name_system(char const *file_path)
{
  sqlite3 *result = nullptr;
  int sql_init    = sqlite3_initialize();
  if (sql_init != SQLITE_OK)
  {
    MERROR("Failed to initialize sqlite3: " << sqlite3_errstr(sql_init));
    return nullptr;
  }

  int sql_open = sqlite3_open(file_path, &result);
  if (sql_open != SQLITE_OK)
  {
    MERROR("Failed to open LNS db at: " << file_path << ", reason: " << sqlite3_errstr(sql_init));
    return nullptr;
  }

  return result;
}

uint64_t lokinet_expiry_blocks(cryptonote::network_type nettype, uint64_t *renew_window)
{
  uint64_t renew_window_ = BLOCKS_EXPECTED_IN_DAYS(31);
  uint64_t result        = BLOCKS_EXPECTED_IN_YEARS(1) + renew_window_;
  if (nettype == cryptonote::FAKECHAIN)
  {
    renew_window_ = 10;
    result        = 10 + renew_window_;
  }
  else if (nettype == cryptonote::TESTNET)
  {
    renew_window_ = BLOCKS_EXPECTED_IN_DAYS(1);
    result        = BLOCKS_EXPECTED_IN_DAYS(1) + renew_window_;
  }

  if (renew_window) *renew_window = renew_window_;
  return result;
}

static bool char_is_num(char c)
{
  bool result = c >= '0' && c <= '9';
  return result;
}

static bool char_is_alpha(char c)
{
  bool result = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  return result;
}

static bool char_is_alphanum(char c)
{
  bool result = char_is_num(c) || char_is_alpha(c);
  return result;
}

bool validate_lns_name(uint16_t type, char const *name, int name_len, std::string *reason)
{
  std::stringstream err_stream;
  LOKI_DEFER { if (reason) *reason = err_stream.str(); };

  int max_name_len = lns::GENERIC_NAME_MAX;
  if (type == static_cast<uint16_t>(mapping_type::messenger))       max_name_len = lns::MESSENGER_DISPLAY_NAME_MAX;
  else if (type == static_cast<uint16_t>(mapping_type::blockchain)) max_name_len = lns::BLOCKCHAIN_NAME_MAX;
  else if (type == static_cast<uint16_t>(mapping_type::lokinet))    max_name_len = lns::LOKINET_DOMAIN_NAME_MAX;

  // NOTE: Validate name length
  if (name_len > max_name_len || name_len == 0)
  {
    if (reason)
    {
      err_stream << "LNS type=" << type << ", specifies mapping from name -> value where the name's length=" << name_len << " is 0 or exceeds the maximum length=" << max_name_len << ", given name=";
      err_stream.write(name, name_len);
    }
    return false;
  }

  // NOTE: Validate domain specific requirements
  if (type == static_cast<uint16_t>(mapping_type::messenger))
  {
  }
  else if (type == static_cast<uint16_t>(mapping_type::lokinet))
  {
    // Domain has to start with a letter or digit, and can have letters, digits, or hyphens in between and must end with a .loki
    // ^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.loki$
    {
      char const SHORTEST_DOMAIN[] = "a.loki";
      if (name_len < static_cast<int>(loki::char_count(SHORTEST_DOMAIN)))
      {
        if (reason)
        {
          err_stream << "LNS type=lokinet, specifies mapping from name -> value where the name is shorter than the shortest possible name=" << SHORTEST_DOMAIN << ", given name=";
          err_stream.write(name, name_len);
        }
        return false;
      }
    }

    if (!char_is_alphanum(name[0])) // Must start with alphanumeric
    {
      if (reason)
      {
        err_stream << "LNS type=lokinet, specifies mapping from name -> value where the name does not start with an alphanumeric character, name=";
        err_stream.write(name, name_len);
      }
      return false;
    }

    char const SUFFIX[]     = ".loki";
    char const *name_suffix = name + (name_len - loki::char_count(SUFFIX));
    if (memcmp(name_suffix, SUFFIX, loki::char_count(SUFFIX)) != 0) // Must end with .loki
    {
      if (reason)
      {
        err_stream << "LNS type=lokinet, specifies mapping from name -> value where the name does not end with the domain .loki, name=";
        err_stream.write(name, name_len);
      }
      return false;
    }

    char const *char_preceeding_suffix = name_suffix - 1;
    if (!char_is_alphanum(char_preceeding_suffix[0])) // Characted preceeding suffix must be alphanumeric
    {
      if (reason)
      {
        err_stream << "LNS type=lokinet, specifies mapping from name -> value where the character preceeding the <char>.loki is not alphanumeric, char=" << char_preceeding_suffix[0] << ", name=";
        err_stream.write(name, name_len);
      }
      return false;
    }

    char const *begin = name + 1;
    char const *end   = char_preceeding_suffix;
    for (char const *it = begin; it < end; it++) // Inbetween start and preceeding suffix, alphanumeric and hyphen characters permitted
    {
      char c = it[0];
      if (!(char_is_alphanum(c) || c == '-'))
      {
        if (reason)
        {
          err_stream << "LNS type=lokinet, specifies mapping from name -> value where the domain name contains more than the permitted alphanumeric or hyphen characters name=";
          err_stream.write(name, name_len);
        }
        return false;
      }
    }
  }

  return true;
}

static bool check_lengths(uint16_t type, char const *value, int len, int max, bool require_exact_len, std::string *reason)
{
  bool result = true;
  if (require_exact_len)
  {
    if (len != max)
      result = false;
  }
  else
  {
    if (len > max || len == 0)
    {
      result = false;
    }
  }

  if (!result)
  {
    if (reason)
    {
      std::stringstream err_stream;
      err_stream << "LNS type=" << type << ", specifies mapping from name -> value where the value's length=" << len;
      if (require_exact_len) err_stream << ", does not equal the required length=";
      else                   err_stream <<" is 0 or exceeds the maximum length=";
      err_stream << max << ", given value=";
      err_stream.write(value, len);
      *reason = err_stream.str();
    }
  }

  return result;
}

bool validate_lns_value(cryptonote::network_type nettype, uint16_t type, char const *value, int value_len, lns_value *blob, std::string *reason)
{
  if (blob) *blob = {};
  std::stringstream err_stream;

  cryptonote::address_parse_info addr_info = {};

  static_assert(GENERIC_VALUE_MAX >= MESSENGER_PUBLIC_KEY_BINARY_LENGTH, "lns_value assumes the largest blob size required, all other values should be able to fit into this buffer");
  static_assert(GENERIC_VALUE_MAX >= LOKINET_ADDRESS_BINARY_LENGTH,      "lns_value assumes the largest blob size required, all other values should be able to fit into this buffer");
  static_assert(GENERIC_VALUE_MAX >= sizeof(addr_info.address),          "lns_value assumes the largest blob size required, all other values should be able to fit into this buffer");
  if (type == static_cast<uint16_t>(mapping_type::blockchain))
  {
    if (value_len == 0 || !get_account_address_from_str(addr_info, nettype, std::string(value, value_len)))
    {
      if (reason)
      {
        if (value_len == 0)
        {
          err_stream << "The value=";
          err_stream.write(value, value_len);
          err_stream << ", mapping into the wallet address, specifies a wallet address of 0 length";
        }
        else
        {
          err_stream << "Could not convert the wallet address string, check it is correct, value=";
          err_stream.write(value, value_len);
        }
        *reason = err_stream.str();
      }
      return false;
    }
  }
  else
  {
    int max_value_len            = lns::GENERIC_VALUE_MAX;
    bool value_require_exact_len = true;
    if (type == static_cast<uint16_t>(mapping_type::lokinet))        max_value_len = (LOKINET_ADDRESS_BINARY_LENGTH * 2);
    else if (type == static_cast<uint16_t>(mapping_type::messenger)) max_value_len = (MESSENGER_PUBLIC_KEY_BINARY_LENGTH * 2);
    else value_require_exact_len = false;

    if (!check_lengths(type, value, value_len, max_value_len, value_require_exact_len, reason))
      return false;
  }

  if (type == static_cast<uint16_t>(mapping_type::blockchain))
  {
    if (blob)
    {
      blob->len = sizeof(addr_info.address);
      memcpy(blob->buffer.data(), &addr_info.address, blob->len);
    }
  }
  else
  {
    // NOTE: Check value is hex
    if ((value_len % 2) != 0)
    {
      if (reason)
      {
        err_stream << "The value=";
        err_stream.write(value, value_len);
        err_stream << ", should be a hex string that has an even length to be convertible back into binary, length=" << value_len;
        *reason = err_stream.str();
      }
      return false;
    }

    for (int val_index = 0; val_index < value_len; val_index += 2)
    {
      char a = value[val_index];
      char b = value[val_index + 1];
      if (loki::char_is_hex(a) && loki::char_is_hex(b))
      {
        if (blob) // NOTE: Given blob, write the binary output
          blob->buffer.data()[blob->len++] = loki::from_hex_pair(a, b);
      }
      else
      {
        if (reason)
        {
          err_stream << "LNS type=" << type <<", specifies name -> value mapping where the value is not a hex string given value=";
          err_stream.write(value, value_len);
          *reason = err_stream.str();
        }
        return false;
      }
    }

    if (type == static_cast<uint16_t>(mapping_type::messenger))
    {
      if (!(value[0] == '0' && value[1] == '5')) // NOTE: Messenger public keys are 33 bytes, with the first byte being 0x05 and the remaining 32 being the public key.
      {
        if (reason)
        {
          err_stream << "LNS type=messenger, specifies mapping from name -> ed25519 key where the key is not prefixed with 53 (0x05), prefix=";
          err_stream << std::to_string(value[0]) << " (" << value[0] << "), given ed25519=";
          err_stream.write(value, value_len);
          *reason = err_stream.str();
        }
        return false;
      }
    }
  }
  return true;
}

bool validate_lns_value_binary(uint16_t type, char const *value, int value_len, std::string *reason)
{
  int max_value_len            = lns::GENERIC_VALUE_MAX;
  bool value_require_exact_len = true;
  if (type == static_cast<uint16_t>(mapping_type::lokinet))         max_value_len = LOKINET_ADDRESS_BINARY_LENGTH;
  else if (type == static_cast<uint16_t>(mapping_type::messenger))  max_value_len = MESSENGER_PUBLIC_KEY_BINARY_LENGTH;
  else if (type == static_cast<uint16_t>(mapping_type::blockchain)) max_value_len = sizeof(cryptonote::account_public_address);
  else value_require_exact_len = false;

  if (!check_lengths(type, value, value_len, max_value_len, value_require_exact_len, reason))
    return false;

  if (type == static_cast<uint16_t>(lns::mapping_type::blockchain))
  {
    // TODO(doyle): Better address validation? Is it a valid address, is it a valid nettype address?
    cryptonote::account_public_address address;
    memcpy(&address, value, sizeof(address));
    if (!(crypto::check_key(address.m_spend_public_key) && crypto::check_key(address.m_view_public_key)))
    {
      if (reason)
      {
        std::stringstream err_stream;
        err_stream << "LNS type=" << type << ", specifies mapping from name -> wallet address where the wallet address's blob does not generate valid public spend/view keys";
        err_stream.write(value, value_len);
        *reason = err_stream.str();
      }
      return false;
    }
  }
  return true;
}

static std::stringstream &error_stream_append_tx_msg(std::stringstream &err_stream, cryptonote::transaction const &tx, cryptonote::tx_extra_loki_name_system const &data)
{
  err_stream << "TX type=" << tx.type << ", tx=" << get_transaction_hash(tx) << ", owner=" << data.owner << ", type=" << (int)data.type << ", name=" << data.name << ", ";
  return err_stream;
}

static bool validate_against_previous_mapping(lns::name_system_db const &lns_db, uint64_t blockchain_height, cryptonote::transaction const &tx, cryptonote::tx_extra_loki_name_system const &data, std::string *reason = nullptr)
{
  std::stringstream err_stream;
  crypto::hash expected_prev_txid = crypto::null_hash;
  if (lns::mapping_record mapping = lns_db.get_mapping(data.type, data.name.data(), data.name.size()))
  {
    expected_prev_txid = mapping.txid;
    if (data.type != static_cast<uint16_t>(lns::mapping_type::lokinet))
    {
      if (reason)
      {
        lns::user_record user = lns_db.get_user_by_id(mapping.user_id);
        error_stream_append_tx_msg(err_stream, tx, data) << "non-lokinet entries can NOT be renewed, mapping already exists with name=" << mapping.name << ", owner=" << user.key << ", type=" << mapping.type;
        *reason = err_stream.str();
      }
      return false;
    }

    if (!mapping.active(lns_db.network_type(), blockchain_height))
      return true;

    uint64_t renew_window              = 0;
    uint64_t expiry_blocks             = lns::lokinet_expiry_blocks(lns_db.network_type(), &renew_window);
    uint64_t const renew_window_offset = expiry_blocks - renew_window;
    uint64_t const min_renew_height    = mapping.register_height + renew_window_offset;

    if (min_renew_height >= blockchain_height)
    {
      error_stream_append_tx_msg(err_stream, tx, data) << "trying to renew too early, the earliest renew height=" << min_renew_height << ", urrent height=" << blockchain_height;
      *reason = err_stream.str();
      return false; // Trying to renew too early
    }

    // LNS entry can be renewed, check that the request to renew originates from the owner of this mapping
    lns::user_record requester = lns_db.get_user_by_key(data.owner);
    if (!requester)
    {
      if (reason)
      {
        error_stream_append_tx_msg(err_stream, tx, data) << "trying to renew existing mapping but owner specified in LNS extra does not exist, rejected";
        *reason = err_stream.str();
      }
      return false;
    }

    lns::user_record owner = lns_db.get_user_by_id(mapping.user_id);
    if (!owner)
    {
      if (reason)
      {
        error_stream_append_tx_msg(err_stream, tx, data) << "unexpected user_id=" << mapping.user_id << " does not exist";
        *reason = err_stream.str();
      }
      return false;
    }

    if (requester.id != owner.id)
    {
      if (reason)
      {
        error_stream_append_tx_msg(err_stream, tx, data) << " actual owner=" << owner.key << ", with user_id=" << mapping.user_id << ", does not match requester=" << requester.key << ", with id=" << requester.id;
        *reason = err_stream.str();
      }
      return false;
    }
  }

  if (data.prev_txid != expected_prev_txid)
  {
    if (reason)
    {
      error_stream_append_tx_msg(err_stream, tx, data) << " specified prior owner txid=" << data.prev_txid << ", but LNS DB reports=" << expected_prev_txid << ", possible competing TX was submitted and accepted before this TX was processed";
      *reason = err_stream.str();
    }
    return false;
  }

  return true;
}

bool name_system_db::validate_lns_tx(uint8_t hf_version, uint64_t blockchain_height, cryptonote::transaction const &tx, cryptonote::tx_extra_loki_name_system *entry, std::string *reason) const
{
  cryptonote::tx_extra_loki_name_system entry_;
  if (!entry) entry = &entry_;
  std::stringstream err_stream;

  if (!cryptonote::get_loki_name_system_from_tx_extra(tx.extra, *entry))
  {
    if (reason)
    {
      err_stream << "TX: " << tx.type << " " << get_transaction_hash(tx) << ", didn't have loki name service in the tx_extra";
      *reason = err_stream.str();
    }
    return false;
  }

  if (entry->type >= static_cast<uint16_t>(lns::mapping_type::start_reserved) && entry->type <= static_cast<uint16_t>(mapping_type::end_reserved))
  {
    if (reason)
    {
      err_stream << "TX: " << tx.type << " " << get_transaction_hash(tx) << " specifying type = " << static_cast<uint16_t>(entry->type) << " that is unused, but reserved by the protocol";
      *reason = err_stream.str();
    }
    return false;
  }

  uint64_t burn = cryptonote::get_burned_amount_from_tx_extra(tx.extra);
  if (!validate_lns_name(entry->type, entry->name.data(), static_cast<int>(entry->name.size()), reason))
    return false;

  if (!validate_lns_value_binary(entry->type, entry->value.data(), static_cast<int>(entry->value.size()), reason))
    return false;

  if (!validate_against_previous_mapping(*this, blockchain_height, tx, *entry, reason))
    return false;

  uint64_t const burn_required = burn_requirement_in_atomic_loki(hf_version);
  if (burn != burn_required)
  {
    if (reason)
    {
      err_stream << "LNS TX=" << cryptonote::get_transaction_hash(tx) << ", burned insufficient amounts of loki=" << burn << ", require=" << burn_required;
      *reason = err_stream.str();
    }
    return false;
  }

  return true;
}

bool validate_mapping_type(std::string const &type, uint16_t *mapping_type, std::string *reason)
{
  std::string type_lowered = type;
  for (char &ch : type_lowered)
  {
    if (ch >= 'A' && ch <= 'Z')
      ch = ch + ('a' - 'A');
  }

  uint16_t mapping_type_ = 0;
  if      (type_lowered == "blockchain") mapping_type_ = static_cast<uint16_t>(lns::mapping_type::blockchain);
  else if (type_lowered == "lokinet")    mapping_type_ = static_cast<uint16_t>(lns::mapping_type::lokinet);
  else if (type_lowered == "messenger")  mapping_type_ = static_cast<uint16_t>(lns::mapping_type::messenger);
  else
  {
    try
    {
      size_t value = std::stoul(type_lowered);
      if (value > std::numeric_limits<uint16_t>::max())
      {
        if (reason) *reason = "LNS custom type specifies value too large, must be from 0-65536: " + std::to_string(value);
        return false;
      }
      mapping_type_ = static_cast<uint16_t>(value);
    }
    catch (std::exception const &)
    {
      if (reason) *reason = "Failed to convert custom lns mapping argument (was not proper integer, or not one of the recognised arguments blockchain, lokinet, messenger), string was: " + type;
      return false;
    }
  }

  if (mapping_type) *mapping_type = mapping_type_;
  return true;
}

static bool build_default_tables(sqlite3 *db)
{
  constexpr char BUILD_TABLE_SQL[] = R"(
CREATE TABLE IF NOT EXISTS "user"(
    "id" INTEGER PRIMARY KEY AUTOINCREMENT,
    "public_key" BLOB NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS "settings" (
    "top_height" INTEGER NOT NULL,
    "top_hash" VARCHAR NOT NULL,
    "version" INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS "mappings" (
    "id" INTEGER PRIMARY KEY NOT NULL,
    "type" INTEGER NOT NULL,
    "name" VARCHAR NOT NULL,
    "value" BLOB NOT NULL,
    "txid" BLOB NOT NULL,
    "prev_txid" BLOB NOT NULL,
    "register_height" INTEGER NOT NULL,
    "user_id" INTEGER NOT NULL REFERENCES "user" ("id")
);
CREATE UNIQUE INDEX IF NOT EXISTS "name_type_id" ON mappings("name", "type");
)";

  char *table_err_msg = nullptr;
  int table_created   = sqlite3_exec(db, BUILD_TABLE_SQL, nullptr /*callback*/, nullptr /*callback context*/, &table_err_msg);
  if (table_created != SQLITE_OK)
  {
    MERROR("Can not generate SQL table for LNS: " << (table_err_msg ? table_err_msg : "??"));
    sqlite3_free(table_err_msg);
    return false;
  }

  return true;
}

enum struct db_version { v1, };
auto constexpr DB_VERSION = db_version::v1;
bool name_system_db::init(cryptonote::network_type nettype, sqlite3 *db, uint64_t top_height, crypto::hash const &top_hash)
{
  if (!db) return false;
  this->db      = db;
  this->nettype = nettype;

  char constexpr PRUNE_MAPPINGS_SQL[]                   = R"(DELETE FROM "mappings" WHERE "register_height" >= ?)";
  char constexpr PRUNE_USERS_SQL[]                      = R"(DELETE FROM "user" WHERE "id" NOT IN (SELECT "user_id" FROM "mappings"))";
  char constexpr GET_MAPPING_SQL[]                      = R"(SELECT * FROM "mappings" WHERE "type" = ? AND "name" = ?)";
  char constexpr GET_SETTINGS_SQL[]                     = R"(SELECT * FROM "settings" WHERE "id" = 0)";
  char constexpr GET_USER_BY_KEY_SQL[]                  = R"(SELECT * FROM "user" WHERE "public_key" = ?)";
  char constexpr GET_USER_BY_ID_SQL[]                   = R"(SELECT * FROM "user" WHERE "id" = ?)";
  char constexpr GET_MAPPINGS_BY_USER_SQL[]             = R"(SELECT * FROM "mappings" WHERE "user_id" = ?)";
  char constexpr GET_MAPPINGS_ON_HEIGHT_AND_NEWER_SQL[] = R"(SELECT * FROM mappings WHERE "register_height" >= ?)";
  char constexpr SAVE_SETTINGS_SQL[]                    = R"(INSERT OR REPLACE INTO "settings" ("rowid", "top_height", "top_hash", "version") VALUES (1,?,?,?))";
  char constexpr SAVE_MAPPING_SQL[]                     = R"(INSERT OR REPLACE INTO "mappings" ("type", "name", "value", "txid", "prev_txid", "register_height", "user_id") VALUES (?,?,?,?,?,?,?))";
  char constexpr SAVE_USER_SQL[]                        = R"(INSERT INTO "user" ("public_key") VALUES (?);)";

  if (!build_default_tables(db))
    return false;

  if (!sql_compile_statement(db, SAVE_USER_SQL,                        loki::array_count(SAVE_USER_SQL),                        &save_user_sql)    ||
      !sql_compile_statement(db, SAVE_MAPPING_SQL,                     loki::array_count(SAVE_MAPPING_SQL),                     &save_mapping_sql) ||
      !sql_compile_statement(db, SAVE_SETTINGS_SQL,                    loki::array_count(SAVE_SETTINGS_SQL),                    &save_settings_sql) ||
      !sql_compile_statement(db, GET_USER_BY_KEY_SQL,                  loki::array_count(GET_USER_BY_KEY_SQL),                  &get_user_by_key_sql) ||
      !sql_compile_statement(db, GET_USER_BY_ID_SQL,                   loki::array_count(GET_USER_BY_ID_SQL),                   &get_user_by_id_sql) ||
      !sql_compile_statement(db, GET_MAPPING_SQL,                      loki::array_count(GET_MAPPING_SQL),                      &get_mapping_sql) ||
      !sql_compile_statement(db, GET_SETTINGS_SQL,                     loki::array_count(GET_SETTINGS_SQL),                     &get_settings_sql) ||
      !sql_compile_statement(db, PRUNE_MAPPINGS_SQL,                   loki::array_count(PRUNE_MAPPINGS_SQL),                   &prune_mappings_sql) ||
      !sql_compile_statement(db, PRUNE_USERS_SQL,                      loki::array_count(PRUNE_USERS_SQL),                      &prune_users_sql) ||
      !sql_compile_statement(db, GET_MAPPINGS_BY_USER_SQL,             loki::array_count(GET_MAPPINGS_BY_USER_SQL),             &get_mappings_by_user_sql) ||
      !sql_compile_statement(db, GET_MAPPINGS_ON_HEIGHT_AND_NEWER_SQL, loki::array_count(GET_MAPPINGS_ON_HEIGHT_AND_NEWER_SQL), &get_mappings_on_height_and_newer_sql)
      )
  {
    return false;
  }

  if (settings_record settings = get_settings())
  {
    if (settings.top_height == top_height && settings.top_hash == top_hash)
    {
      this->last_processed_height = settings.top_height;
      assert(settings.version == static_cast<int>(DB_VERSION));
    }
    else
    {
      char constexpr DROP_TABLE_SQL[] = R"(DROP TABLE IF EXISTS "user"; DROP TABLE IF EXISTS "settings"; DROP TABLE IF EXISTS "mappings")";
      sqlite3_exec(db, DROP_TABLE_SQL, nullptr /*callback*/, nullptr /*callback context*/, nullptr);
      if (!build_default_tables(db)) return false;
    }
  }

  return true;
}

struct scoped_db_transaction
{
  scoped_db_transaction(name_system_db &lns_db);
  ~scoped_db_transaction();
  operator bool() const { return initialised; }
  name_system_db &lns_db;
  bool commit      = false; // If true, on destruction- END the transaction otherwise ROLLBACK all SQLite events prior for the lns_db
  bool initialised = false;
};

scoped_db_transaction::scoped_db_transaction(name_system_db &lns_db)
: lns_db(lns_db)
{
  if (lns_db.transaction_begun)
  {
    MERROR("Failed to begin transaction, transaction exists previously that was not closed properly");
    return;
  }

  char *sql_err = nullptr;
  if (sqlite3_exec(lns_db.db, "BEGIN;", nullptr, nullptr, &sql_err) != SQLITE_OK)
  {
    MERROR("Failed to begin transaction " << ", reason=" << (sql_err ? sql_err : "??"));
    sqlite3_free(sql_err);
    return;
  }

  initialised              = true;
  lns_db.transaction_begun = true;
}

scoped_db_transaction::~scoped_db_transaction()
{
  if (!initialised) return;
  if (!lns_db.transaction_begun)
  {
    MERROR("Trying to apply non-existent transaction (no prior history of a db transaction beginning) to the LNS DB");
    return;
  }

  char *sql_err = nullptr;
  if (sqlite3_exec(lns_db.db, commit ? "END;" : "ROLLBACK;", NULL, NULL, &sql_err) != SQLITE_OK)
  {
    MERROR("Failed to " << (commit ? "end " : "rollback ") << " transaction to LNS DB, reason=" << (sql_err ? sql_err : "??"));
    sqlite3_free(sql_err);
    return;
  }

  lns_db.transaction_begun = false;
}

static bool add_lns_entry(lns::name_system_db &lns_db, uint64_t height, cryptonote::tx_extra_loki_name_system const &entry, crypto::hash const &tx_hash)
{
  int64_t user_id = 0;
  if (user_record user = lns_db.get_user_by_key(entry.owner)) user_id = user.id;
  if (user_id == 0)
  {
    if (!lns_db.save_user(entry.owner, &user_id))
    {
      LOG_PRINT_L1("Failed to save LNS user to DB tx: " << tx_hash << ", type: " << (uint16_t)entry.type << ", name: " << entry.name << ", user: " << entry.owner);
      return false;
    }
  }
  assert(user_id != 0);

  if (!lns_db.save_mapping(tx_hash, entry, height, user_id))
  {
    LOG_PRINT_L1("Failed to save LNS entry to DB tx: " << tx_hash << ", type: " << (uint16_t)entry.type << ", name: " << entry.name << ", user: " << entry.owner);
    return false;
  }

  return true;
}

bool name_system_db::add_block(const cryptonote::block &block, const std::vector<cryptonote::transaction> &txs)
{
  uint64_t height = cryptonote::get_block_height(block);
  if (last_processed_height >= height)
      return true;

  if (block.major_version >= cryptonote::network_version_15_lns)
  {
    scoped_db_transaction db_transaction(*this);
    if (!db_transaction)
     return false;

    for (cryptonote::transaction const &tx : txs)
    {
      if (tx.type != cryptonote::txtype::loki_name_system)
        continue;

      cryptonote::tx_extra_loki_name_system entry = {};
      std::string fail_reason;
      if (!validate_lns_tx(block.major_version, height, tx, &entry, &fail_reason))
      {
        LOG_PRINT_L0("LNS TX: Failed to validate for tx=" << get_transaction_hash(tx) << ". This should have failed validation earlier reason=" << fail_reason);
        assert("Failed to validate acquire name service. Should already have failed validation prior" == nullptr);
        return false;
      }

      crypto::hash const &tx_hash = cryptonote::get_transaction_hash(tx);
      add_lns_entry(*this, height, entry, tx_hash);
    }

    db_transaction.commit = true;
  }

  last_processed_height = height;
  save_settings(height, cryptonote::get_block_hash(block), static_cast<int>(DB_VERSION));
  return true;
}

static bool get_txid_lns_entry(cryptonote::Blockchain const &blockchain, crypto::hash txid, cryptonote::tx_extra_loki_name_system &extra)
{
  std::vector<cryptonote::transaction> txs;
  std::vector<crypto::hash> missed_txs;
  if (!blockchain.get_transactions({txid}, txs, missed_txs) || txs.empty())
    return false;

  return cryptonote::get_loki_name_system_from_tx_extra(txs[0].extra, extra);
}

static bool find_closest_valid_lns_tx_extra_in_blockchain(cryptonote::Blockchain const &blockchain,
                                                          lns::mapping_record const &mapping,
                                                          uint64_t blockchain_height,
                                                          cryptonote::tx_extra_loki_name_system &extra,
                                                          crypto::hash &tx_hash,
                                                          uint64_t &extra_height)
{
  uint64_t prev_height                             = static_cast<uint64_t>(-1);
  crypto::hash prev_txid                           = mapping.prev_txid;
  cryptonote::tx_extra_loki_name_system prev_entry = {};
  if (!get_txid_lns_entry(blockchain, prev_txid, prev_entry)) return false;

  for (;;)
  {
    std::vector<uint64_t> prev_heights = blockchain.get_transactions_heights({prev_txid});
    if (prev_heights.empty())
    {
        MERROR("Unexpected error querying TXID=" << prev_txid << ", height from DB for LNS");
      return false;
    }

    prev_height = prev_heights[0];
    if (prev_height >= blockchain_height)
    {
      // Previous owner of mapping is after the detach height, iterate back to
      // next prev entry by getting the relevant transaction, extract the LNS
      // extra and continue searching.
      if (!get_txid_lns_entry(blockchain, prev_txid, prev_entry))
          return false;
      prev_txid = prev_entry.prev_txid;
    }
    else
    {
      break;
    }
  }

  bool result = prev_height < blockchain_height && prev_txid != crypto::null_hash;
  if (result)
  {
    tx_hash      = prev_txid;
    extra        = std::move(prev_entry);
    extra_height = prev_height;
  }
  return result;
}

void name_system_db::block_detach(cryptonote::Blockchain const &blockchain, uint64_t height)
{
  std::vector<mapping_record> new_mappings = {};
  {
    sqlite3_stmt *statement = get_mappings_on_height_and_newer_sql;
    sqlite3_clear_bindings(statement);
    sqlite3_bind_int(statement, 1 /*sql param index*/, height);
    sql_run_statement(nettype, lns_sql_type::get_mappings_on_height_and_newer, statement, &new_mappings);
  }

  struct lns_parts
  {
    uint64_t                              height;
    crypto::hash                          tx_hash;
    cryptonote::tx_extra_loki_name_system entry;
  };

  std::vector<lns_parts> entries;
  for (auto const &mapping : new_mappings)
  {
    cryptonote::tx_extra_loki_name_system entry = {};
    uint64_t entry_height                       = 0;
    crypto::hash tx_hash                        = {};
    if (!find_closest_valid_lns_tx_extra_in_blockchain(blockchain, mapping, height, entry, tx_hash, entry_height)) continue;
    entries.push_back({entry_height, tx_hash, entry});
  }

  prune_db(height);
  for (auto const &lns : entries)
  {
    if (!add_lns_entry(*this, lns.height, lns.entry, lns.tx_hash))
      MERROR("Unexpected failure to add historical LNS into the DB on reorganization from tx=" << lns.tx_hash);
  }

}

bool name_system_db::save_user(crypto::ed25519_public_key const &key, int64_t *row_id)
{
  sqlite3_stmt *statement = save_user_sql;
  sqlite3_clear_bindings(statement);
  sqlite3_bind_blob(statement, 1 /*sql param index*/, &key, sizeof(key), nullptr /*destructor*/);
  bool result = sql_run_statement(nettype, lns_sql_type::save_user, statement, nullptr);
  if (row_id) *row_id = sqlite3_last_insert_rowid(db);
  return result;
}

bool name_system_db::save_mapping(crypto::hash const &tx_hash, cryptonote::tx_extra_loki_name_system const &src, uint64_t height, int64_t user_id)
{
  sqlite3_stmt *statement = save_mapping_sql;
  sqlite3_bind_int  (statement, static_cast<int>(mapping_record_row::type), static_cast<uint16_t>(src.type));
  sqlite3_bind_text (statement, static_cast<int>(mapping_record_row::name), src.name.data(), src.name.size(), nullptr /*destructor*/);
  sqlite3_bind_blob (statement, static_cast<int>(mapping_record_row::value), src.value.data(), src.value.size(), nullptr /*destructor*/);
  sqlite3_bind_blob (statement, static_cast<int>(mapping_record_row::txid), tx_hash.data, sizeof(tx_hash), nullptr /*destructor*/);
  sqlite3_bind_blob (statement, static_cast<int>(mapping_record_row::prev_txid), src.prev_txid.data, sizeof(src.prev_txid), nullptr /*destructor*/);
  sqlite3_bind_int64(statement, static_cast<int>(mapping_record_row::register_height), static_cast<int64_t>(height));
  sqlite3_bind_int64(statement, static_cast<int>(mapping_record_row::user_id), user_id);
  bool result = sql_run_statement(nettype, lns_sql_type::save_mapping, statement, nullptr);
  return result;
}

bool name_system_db::save_settings(uint64_t top_height, crypto::hash const &top_hash, int version)
{
  sqlite3_stmt *statement = save_settings_sql;
  sqlite3_bind_blob (statement, static_cast<int>(lns_db_setting_row::top_hash),   top_hash.data, sizeof(top_hash), nullptr /*destructor*/);
  sqlite3_bind_int64(statement, static_cast<int>(lns_db_setting_row::top_height), top_height);
  sqlite3_bind_int  (statement, static_cast<int>(lns_db_setting_row::version),    version);
  bool result = sql_run_statement(nettype, lns_sql_type::save_setting, statement, nullptr);
  return result;
}

bool name_system_db::prune_db(uint64_t height)
{
  {
    sqlite3_stmt *statement = prune_mappings_sql;
    sqlite3_bind_int64(statement, 1 /*sql param index*/, height);
    if (!sql_run_statement(nettype, lns_sql_type::pruning, statement, nullptr)) return false;
  }

  {
    sqlite3_stmt *statement = prune_users_sql;
    if (!sql_run_statement(nettype, lns_sql_type::pruning, statement, nullptr)) return false;
  }

  return true;
}

user_record name_system_db::get_user_by_key(crypto::ed25519_public_key const &key) const
{
  sqlite3_stmt *statement = get_user_by_key_sql;
  sqlite3_clear_bindings(statement);
  sqlite3_bind_blob(statement, 1 /*sql param index*/, &key, sizeof(key), nullptr /*destructor*/);

  user_record result = {};
  result.loaded      = sql_run_statement(nettype, lns_sql_type::get_user, statement, &result);
  return result;
}

user_record name_system_db::get_user_by_id(int64_t user_id) const
{
  sqlite3_stmt *statement = get_user_by_id_sql;
  sqlite3_clear_bindings(statement);
  sqlite3_bind_int(statement, 1 /*sql param index*/, user_id);

  user_record result = {};
  result.loaded      = sql_run_statement(nettype, lns_sql_type::get_user, statement, &result);
  return result;
}

mapping_record name_system_db::get_mapping(uint16_t type, char const *name, size_t name_len) const
{
  sqlite3_stmt *statement = get_mapping_sql;
  sqlite3_clear_bindings(statement);
  sqlite3_bind_int(statement, 1 /*sql param index*/, type);
  sqlite3_bind_text(statement, 2 /*sql param index*/, name, name_len, nullptr /*destructor*/);

  mapping_record result = {};
  result.loaded         = sql_run_statement(nettype, lns_sql_type::get_mapping, statement, &result);
  return result;
}

mapping_record name_system_db::get_mapping(uint16_t type, std::string const &name) const
{
  mapping_record result = get_mapping(type, name.data(), name.size());
  return result;
}

std::vector<mapping_record> name_system_db::get_mappings_by_user(crypto::ed25519_public_key const &key) const
{
  std::vector<mapping_record> result = {};
  if (lns::user_record user = get_user_by_key(key))
  {
    sqlite3_stmt *statement = get_mappings_by_user_sql;
    sqlite3_clear_bindings(statement);
    sqlite3_bind_int(statement, 1 /*sql param index*/, user.id);
    sql_run_statement(nettype, lns_sql_type::get_mappings_by_user, statement, &result);
  }
  return result;
}

settings_record name_system_db::get_settings() const
{
  sqlite3_stmt *statement = get_user_by_id_sql;
  settings_record result  = {};
  result.loaded           = sql_run_statement(nettype, lns_sql_type::get_setting, statement, &result);
  return result;
}
}; // namespace service_nodes
