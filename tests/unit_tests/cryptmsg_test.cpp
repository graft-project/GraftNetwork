#include <gtest/gtest.h>
#include "utils/cryptmsg.h"

TEST(Utils, cryptoMessage)
{
    using namespace crypto;

    std::vector<public_key> vec_B;
    std::vector<secret_key> vec_b;
    for(int i = 0; i < 10; ++i)
    {
        public_key B; secret_key b;
        generate_keys(B,b);
        vec_B.emplace_back(std::move(B)); vec_b.emplace_back(std::move(b));
    }

    std::string data = "12345qwertasdfgzxcvb";
    std::string message;
    graft::crypto_tools::encryptMessage(data, vec_B, message);

    for(const auto& b : vec_b)
    {
        std::string plain;
        bool res = graft::crypto_tools::decryptMessage(message, b, plain);
        EXPECT_EQ(res, true);
        EXPECT_EQ(plain, data);
    }

    {//unknown key
        public_key B; secret_key b;
        generate_keys(B,b);

        std::string plain;
        bool res = graft::crypto_tools::decryptMessage(message, b, plain);
        EXPECT_EQ(res, false);
    }
    {//corrupted key
        secret_key b = vec_b[0];
        b.data[ sizeof(b.data) - 1] ^= 0xFF;

        std::string plain;
        bool res = graft::crypto_tools::decryptMessage(message, b, plain);
        EXPECT_EQ(res, false);
    }
}
