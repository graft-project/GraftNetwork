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
