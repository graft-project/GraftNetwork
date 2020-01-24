#include "gtest/gtest.h"

#include "common/loki.h"
#include "cryptonote_core/loki_name_system.h"

TEST(loki_name_system, lokinet_domain_names)
{

  char domain_edkeys[lns::LOKINET_ADDRESS_BINARY_LENGTH * 2] = {};
  memset(domain_edkeys, 'a', sizeof(domain_edkeys));
  uint16_t const lokinet = static_cast<uint16_t>(lns::mapping_type::lokinet);

  // Should work
  {
    char const name[] = "mydomain.loki";
    ASSERT_TRUE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  {
    char const name[] = "a.loki";
    ASSERT_TRUE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  {
    char const name[] = "xn--bcher-kva.loki";
    ASSERT_TRUE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  // Should fail
  {
    char const name[] = "";
    ASSERT_FALSE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  {
    char const name[] = "mydomain.loki.example";
    ASSERT_FALSE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  {
    char const name[] = "mydomain.loki.com";
    ASSERT_FALSE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  {
    char const name[] = "mydomain.com";
    ASSERT_FALSE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  {
    char const name[] = "mydomain";
    ASSERT_FALSE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  {
    char const name[] = "xn--bcher-kva.lok";
    ASSERT_FALSE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }

  {
    char const name[] = "a_b_c.loki";
    ASSERT_FALSE(lns::validate_lns_name_and_value(cryptonote::MAINNET, lokinet, name, loki::char_count(name), domain_edkeys, loki::array_count(domain_edkeys)));
  }
}
