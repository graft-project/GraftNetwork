#include "loki_name_system.h"

#include "checkpoints/checkpoints.h"
#include "common/loki.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_basic/tx_extra.h"

#include "sqlite/sqlite3.h"

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
  expire_mapping,

  get_sentinel_start,
  get_user,
  get_setting,
  get_mapping,
  get_mappings_by_user,
  get_sentinel_end,
};

enum lns_db_setting_row
{
  lns_db_setting_row_id,
  lns_db_setting_row_top_height,
  lns_db_setting_row_top_hash,
  lns_db_setting_row_version,
};

enum user_record_row
{
  user_record_row_id,
  user_record_row_public_key,
};

enum mapping_record_row
{
  mapping_record_row_id,
  mapping_record_row_type,
  mapping_record_row_name,
  mapping_record_row_value,
  mapping_record_row_register_height,
  mapping_record_row_user_id,
};

static void sql_copy_blob(sqlite3_stmt *statement, int row, void *dest, int dest_size)
{
  void const *blob = sqlite3_column_blob(statement, row);
  int blob_len     = sqlite3_column_bytes(statement, row);
  assert(blob_len == dest_size);
  memcpy(dest, blob, std::min(dest_size, blob_len));
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
            entry->id   = sqlite3_column_int(statement, user_record_row_id);
            sql_copy_blob(statement, user_record_row_public_key, entry->key.data, sizeof(entry->key.data));
            data_loaded = true;
          }
          break;

          case lns_sql_type::get_setting:
          {
            auto *entry       = reinterpret_cast<settings_record *>(context);
            entry->top_height = static_cast<uint64_t>(sqlite3_column_int64(statement, lns_db_setting_row_top_height));
            sql_copy_blob(statement, lns_db_setting_row_top_hash, entry->top_hash.data, sizeof(entry->top_hash.data));
            entry->version = sqlite3_column_int(statement, lns_db_setting_row_version);
            data_loaded = true;
          }
          break;

          case lns_sql_type::get_mappings_by_user: /* FALLTHRU */
          case lns_sql_type::get_mapping:
          {
            mapping_record tmp_entry = {};
            tmp_entry.type = static_cast<uint16_t>(sqlite3_column_int(statement, mapping_record_row_type));
            tmp_entry.register_height = static_cast<uint16_t>(sqlite3_column_int(statement, mapping_record_row_register_height));
            tmp_entry.user_id = sqlite3_column_int(statement, mapping_record_row_user_id);

            int name_len  = sqlite3_column_bytes(statement, mapping_record_row_name);
            auto *name    = reinterpret_cast<char const *>(sqlite3_column_text(statement, mapping_record_row_name));

            size_t value_len = static_cast<size_t>(sqlite3_column_bytes(statement, mapping_record_row_value));
            auto *value      = reinterpret_cast<char const *>(sqlite3_column_text(statement, mapping_record_row_value));

            tmp_entry.name  = std::string(name, name_len);
            tmp_entry.value = std::string(value, value_len);
            data_loaded = true;

            if (type == lns_sql_type::get_mappings_by_user)
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

bool validate_lns_tx(uint8_t hf_version, cryptonote::network_type nettype, cryptonote::transaction const &tx, cryptonote::tx_extra_loki_name_system *entry, std::string *reason)
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

enum struct db_version : int { v1, };
auto constexpr DB_VERSION = db_version::v1;
bool name_system_db::init(cryptonote::network_type nettype, sqlite3 *db, uint64_t top_height, crypto::hash const &top_hash)
{
  if (!db) return false;
  this->db      = db;
  this->nettype = nettype;

  char constexpr EXPIRE_MAPPINGS_SQL[]      = R"(DELETE FROM "mappings" WHERE "type" = ? AND "register_height" < ?)";
  char constexpr GET_MAPPING_SQL[]          = R"(SELECT * FROM "mappings" WHERE "type" = ? AND "name" = ?)";
  char constexpr GET_SETTINGS_SQL[]         = R"(SELECT * FROM settings WHERE "id" = 0)";
  char constexpr GET_USER_BY_KEY_SQL[]      = R"(SELECT * FROM user WHERE "public_key" = ?)";
  char constexpr GET_USER_BY_ID_SQL[]       = R"(SELECT * FROM user WHERE "id" = ?)";
  char constexpr GET_MAPPINGS_BY_USER_SQL[] = R"(SELECT * FROM mappings WHERE "user_id" = ?)";
  char constexpr SAVE_SETTINGS_SQL[]        = R"(INSERT OR REPLACE INTO "settings" ("rowid", "top_height", "top_hash", "version") VALUES (1,?,?,?))";
  char constexpr SAVE_MAPPING_SQL[]         = R"(INSERT OR REPLACE INTO "mappings" ("type", "name", "value", "register_height", "user_id") VALUES (?,?,?,?,?))";
  char constexpr SAVE_USER_SQL[]            = R"(INSERT INTO "user" ("public_key") VALUES (?);)";

  if (!build_default_tables(db))
    return false;

  if (!sql_compile_statement(db, SAVE_USER_SQL,            loki::array_count(SAVE_USER_SQL),            &save_user_sql)    ||
      !sql_compile_statement(db, SAVE_MAPPING_SQL,         loki::array_count(SAVE_MAPPING_SQL),         &save_mapping_sql) ||
      !sql_compile_statement(db, SAVE_SETTINGS_SQL,        loki::array_count(SAVE_SETTINGS_SQL),        &save_settings_sql) ||
      !sql_compile_statement(db, GET_USER_BY_KEY_SQL,      loki::array_count(GET_USER_BY_KEY_SQL),      &get_user_by_key_sql) ||
      !sql_compile_statement(db, GET_USER_BY_ID_SQL,       loki::array_count(GET_USER_BY_ID_SQL),       &get_user_by_id_sql) ||
      !sql_compile_statement(db, GET_MAPPING_SQL,          loki::array_count(GET_MAPPING_SQL),          &get_mapping_sql) ||
      !sql_compile_statement(db, GET_SETTINGS_SQL,         loki::array_count(GET_SETTINGS_SQL),         &get_settings_sql) ||
      !sql_compile_statement(db, EXPIRE_MAPPINGS_SQL,      loki::array_count(EXPIRE_MAPPINGS_SQL),      &expire_mapping_sql) ||
      !sql_compile_statement(db, GET_MAPPINGS_BY_USER_SQL, loki::array_count(GET_MAPPINGS_BY_USER_SQL), &get_mappings_by_user_sql)
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

enum struct sql_transaction_type
{
    begin,
    commit,
    rollback,
    count,
};

static bool sql_transaction(sqlite3 *db, sql_transaction_type type)
{
  char *sql_err            = nullptr;
  char const *const CMDS[] = {
      "BEGIN;",
      "END;",
      "ROLLBACK;",
  };

  static_assert(loki::array_count(CMDS) == static_cast<int>(sql_transaction_type::count), "Unexpected enum to string mismatch");
  if (sqlite3_exec(db, CMDS[static_cast<int>(type)], NULL, NULL, &sql_err) != SQLITE_OK)
  {
    MERROR("Can not execute transactional step: " << CMDS[static_cast<int>(type)] << ", reason: " << (sql_err ? sql_err : "??"));
    sqlite3_free(sql_err);
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
    if (!sql_transaction(db, sql_transaction_type::begin))
    {
      MERROR("Failed to initiate transaction for LNS DB");
      return false;
    }

    for (cryptonote::transaction const &tx : txs)
    {
      if (tx.type != cryptonote::txtype::loki_name_system)
        continue;

      cryptonote::tx_extra_loki_name_system entry = {};
      std::string fail_reason;
      if (!validate_lns_tx(block.major_version, nettype, tx, &entry, &fail_reason))
      {
        LOG_PRINT_L1("LNS TX: Failed to validate for tx=" << get_transaction_hash(tx) << ". This should have failed validation earlier reason=" << fail_reason);
        assert("Failed to validate acquire name service. Should already have failed validation prior" == nullptr);
        continue;
      }

      int64_t user_id = 0;
      if (user_record user = get_user_by_key(entry.owner)) user_id = user.id;
      if (user_id == 0)
      {
        if (!save_user(entry.owner, &user_id))
        {
          LOG_PRINT_L1("Failed to save LNS user to DB tx: " << cryptonote::get_transaction_hash(tx) << ", type: " << (uint16_t)entry.type << ", name: " << entry.name << ", user: " << entry.owner);
          if (!sql_transaction(db, sql_transaction_type::rollback))
          {
            MERROR("Failed to rollback transaction in LNS DB");
            return false;
          }
        }
      }
      assert(user_id != 0);

      if (!save_mapping(static_cast<uint16_t>(entry.type), entry.name, entry.value, height, user_id))
      {
        LOG_PRINT_L1("Failed to save LNS entry to DB tx: " << cryptonote::get_transaction_hash(tx)
                                                           << ", type: " << (uint16_t)entry.type
                                                           << ", name: " << entry.name << ", user: " << entry.owner);
        if (!sql_transaction(db, sql_transaction_type::rollback))
        {
          MERROR("Failed to rollback transaction in LNS DB");
          return false;
        }
      }
    }

    if (!sql_transaction(db, sql_transaction_type::commit))
    {
      MERROR("Failed to commit transaction(s) to LNS DB from block" << cryptonote::get_block_hash(block));
      return false;
    }

    if (!expire_mappings(height))
    {
      MERROR("Failed to expire mappings in LNS DB");
      return false;
    }
  }

  last_processed_height = height;
  save_settings(height, cryptonote::get_block_hash(block), static_cast<int>(DB_VERSION));
  return true;
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

bool name_system_db::save_mapping(uint16_t type, std::string const &name, std::string const &value, uint64_t height, int64_t user_id)
{
  sqlite3_stmt *statement = save_mapping_sql;
  sqlite3_bind_int  (statement, mapping_record_row_type, type);
  sqlite3_bind_text (statement, mapping_record_row_name, name.data(), name.size(), nullptr /*destructor*/);
  sqlite3_bind_blob (statement, mapping_record_row_value, value.data(), value.size(), nullptr /*destructor*/);
  sqlite3_bind_int64(statement, mapping_record_row_register_height, static_cast<int64_t>(height));
  sqlite3_bind_int64(statement, mapping_record_row_user_id, user_id);
  bool result = sql_run_statement(nettype, lns_sql_type::save_mapping, statement, nullptr);
  return result;
}

bool name_system_db::save_settings(uint64_t top_height, crypto::hash const &top_hash, int version)
{
  sqlite3_stmt *statement = save_settings_sql;
  sqlite3_bind_blob (statement, lns_db_setting_row_top_hash,   top_hash.data, sizeof(top_hash), nullptr /*destructor*/);
  sqlite3_bind_int64(statement, lns_db_setting_row_top_height, top_height);
  sqlite3_bind_int  (statement, lns_db_setting_row_version,    version);
  bool result = sql_run_statement(nettype, lns_sql_type::save_setting, statement, nullptr);
  return result;
}

bool name_system_db::expire_mappings(uint64_t height)
{
  uint64_t lifetime = lokinet_expiry_blocks(nettype, nullptr);
  if (height < lifetime)
      return true;

  uint64_t expire_height  = height - lifetime;
  sqlite3_stmt *statement = expire_mapping_sql;
  sqlite3_clear_bindings(statement);
  sqlite3_bind_int      (statement, 1 /*sql param index*/, static_cast<int>(mapping_type::lokinet));
  sqlite3_bind_int64    (statement, 2 /*sql param index*/, expire_height);

  bool result = sql_run_statement(nettype, lns_sql_type::expire_mapping, statement, nullptr);
  return result;
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
