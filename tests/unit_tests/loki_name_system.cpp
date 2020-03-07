#include "gtest/gtest.h"

#include "common/loki.h"
#include "cryptonote_core/loki_name_system.h"

TEST(loki_name_system, lokinet_domain_names)
{

  char domain_edkeys[lns::LOKINET_ADDRESS_BINARY_LENGTH * 2] = {};
  memset(domain_edkeys, 'a', sizeof(domain_edkeys));
  lns::mapping_type const lokinet = lns::mapping_type::lokinet_1year;

  // Should work
  {
    std::string const name = "mydomain.loki";
    ASSERT_TRUE(lns::validate_lns_name(lokinet, name));
  }

  {
    std::string const name = "a.loki";
    ASSERT_TRUE(lns::validate_lns_name(lokinet, name));
  }

  {
    std::string const name = "xn--bcher-kva.loki";
    ASSERT_TRUE(lns::validate_lns_name(lokinet, name));
  }

  // Should fail
  {
    std::string const name = "";
    ASSERT_FALSE(lns::validate_lns_name(lokinet, name));
  }

  {
    std::string const name = "mydomain.loki.example";
    ASSERT_FALSE(lns::validate_lns_name(lokinet, name));
  }

  {
    std::string const name = "mydomain.loki.com";
    ASSERT_FALSE(lns::validate_lns_name(lokinet, name));
  }

  {
    std::string const name = "mydomain.com";
    ASSERT_FALSE(lns::validate_lns_name(lokinet, name));
  }

  {
    std::string const name = "mydomain";
    ASSERT_FALSE(lns::validate_lns_name(lokinet, name));
  }

  {
    std::string const name = "xn--bcher-kva.lok";
    ASSERT_FALSE(lns::validate_lns_name(lokinet, name));
  }

  {
    std::string const name = "a_b_c.loki";
    ASSERT_FALSE(lns::validate_lns_name(lokinet, name));
  }
}

TEST(loki_name_system, value_encrypt_and_decrypt)
{
  std::string name         = "my lns name";
  lns::mapping_value value = {};
  value.len                = 32;
  memset(&value.buffer[0], 'a', value.len);

  // Encryption and Decryption success
  {
    lns::mapping_value encrypted_value = {};
    lns::mapping_value decrypted_value = {};
    ASSERT_TRUE(lns::encrypt_mapping_value(name, value, encrypted_value));
    ASSERT_TRUE(lns::decrypt_mapping_value(name, encrypted_value, decrypted_value));
    ASSERT_TRUE(value == decrypted_value);
  }

  // Decryption Fail: Encrypted value was modified
  {
    lns::mapping_value encrypted_value = {};
    ASSERT_TRUE(lns::encrypt_mapping_value(name, value, encrypted_value));

    encrypted_value.buffer[0] = 'Z';
    lns::mapping_value decrypted_value;
    ASSERT_FALSE(lns::decrypt_mapping_value(name, encrypted_value, decrypted_value));
  }

  // Decryption Fail: Name was modified
  {
    std::string name_copy = name;
    lns::mapping_value encrypted_value = {};
    ASSERT_TRUE(lns::encrypt_mapping_value(name_copy, value, encrypted_value));

    name_copy[0] = 'Z';
    lns::mapping_value decrypted_value;
    ASSERT_FALSE(lns::decrypt_mapping_value(name_copy, encrypted_value, decrypted_value));
  }
}
