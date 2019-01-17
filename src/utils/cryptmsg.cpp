// Copyright (c) 2019, The Graft Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <boost/endian/conversion.hpp>
#include "cryptmsg.h"
#include "crypto/chacha.h"

namespace {

static_assert(201103L <= __cplusplus);

#if 0
//can be used to check manually
#define native_to_little boost::endian::native_to_big
#define little_to_native boost::endian::big_to_native
#else
using namespace boost::endian;
#endif

inline size_t getEncryptChachaSize(size_t plainSize)
{
    return plainSize + sizeof(crypto::chacha_iv);
}

void encryptChacha(const uint8_t* plain, size_t plain_size, const crypto::secret_key &skey, uint8_t* cipher)
{
  crypto::chacha_key key;
  crypto::generate_chacha_key(&skey, sizeof(skey), key);
  crypto::chacha_iv& iv = *reinterpret_cast<crypto::chacha_iv*>(cipher);
  iv = crypto::rand<crypto::chacha_iv>();
  crypto::chacha8(plain, plain_size, key, iv, reinterpret_cast<char*>(cipher) + sizeof(iv));
}

void decryptChacha(const uint8_t* cipher, size_t cipher_size, const crypto::secret_key &skey, uint8_t* plain)
{
  const size_t prefix_size = sizeof(crypto::chacha_iv);
  crypto::chacha_key key;
  crypto::generate_chacha_key(&skey, sizeof(skey), key);
  const crypto::chacha_iv &iv = *reinterpret_cast<const crypto::chacha_iv*>(cipher);
  crypto::chacha8(reinterpret_cast<const char*>(cipher) + sizeof(iv), cipher_size - prefix_size, key, iv, reinterpret_cast<char*>(plain));
}

constexpr uint8_t cStart = 0xA5;
constexpr uint8_t cEnd = 0x5A;

#pragma pack(push, 1)

//Note, native order is little-endian

//SessionX in decrypted form contains session key x and constants to check that the decryption was correct
struct SessionX
{
    uint8_t cstart; //decrypted value cStart
    crypto::secret_key x;
    uint8_t cend; //decrypted value cEnd
};

struct XEntry
{
    uint32_t Bhash; //xor of B
    uint8_t cipherX[sizeof(SessionX) + sizeof(crypto::chacha_iv)]; //encrypted SessionX
};

struct CryptoMessageHead
{
    uint32_t plainSize; //size of decrypted data
    crypto::public_key R; //random key used to encrypt session key
    uint16_t count; //recipients count
    XEntry xentries[1];
};

#pragma pack(pop)

uint32_t getBhash(const crypto::public_key& B)
{
    uint32_t res = 0;
    static_assert(sizeof(B) % sizeof(res) == 0);
    const uint32_t* p = reinterpret_cast<const uint32_t*>(&B);
    for(int i = 0, cnt = sizeof(B) / sizeof(res); i < cnt; ++i, ++p)
    {
        res ^= *p;
    }
    return native_to_little(res);
}

/*!
 * \brief encryptMsg - encrypts data for recipients using their B public keys (assumed public view keys).
 *
 * The result has following structure [plainSize:32][R:32][count of XEntrys:16][XEntry]...[XEntry][x encrypted data:+8*8]
 * plainSize - size of original data, x encrypted data takes 8 more bytes
 * [XEntry] - [Bhash:32][rBX] pair for each recipient
 * where Bhash - xor of B (aka fingerprint of B, using which recipient can find his entry)
 * [rBX:+8*8] - encrypted X (aka SessionX)
 * [X] = [cstart:8][x:32*8][cend:8]
 *
 * \param inputSize - input buffer size.
 * \param input - input buffer to encrypt.
 * \param BkeysCount - count of B keys.
 * \param Bkeys - array of B keys for each recipients.
 * \param outputSize - output buffer size.
 * \param output - output buffer.
 * \returns output size required if output==nullptr or outputSize is not enough.
 * returns 0 on error
 */

size_t encryptMsg(size_t inputSize, const uint8_t* input, size_t BkeysCount, const crypto::public_key* Bkeys, size_t outputSize, uint8_t* output)
{
    if(!inputSize || !BkeysCount)
        return 0;

    //prepare
    size_t msgHeadSize = sizeof(CryptoMessageHead) + (BkeysCount - 1) * sizeof(XEntry);
    size_t msgSize = msgHeadSize + getEncryptChachaSize(inputSize);
    if(outputSize < msgSize)
        return msgSize;
    if(!input || !Bkeys || !output)
        return 0;

    //make decorated session key
    SessionX X; X.cstart = cStart; X.cend = cEnd;
    //generate session key X.x
    {
        crypto::public_key tmpX;
        crypto::generate_keys(tmpX,X.x);
    }
    //chacha encrypt input with x
    encryptChacha(input, inputSize, X.x, output + msgHeadSize);

    //fill head
    CryptoMessageHead& head = *reinterpret_cast<CryptoMessageHead*>(output);
    head.plainSize = native_to_little(uint32_t(inputSize));
    crypto::secret_key r;
    crypto::generate_keys(head.R, r);
    head.count = native_to_little(uint16_t(BkeysCount));
    //fill XEntry for each B
    const crypto::public_key* pB = Bkeys;
    XEntry* pxe = head.xentries;
    for(size_t i=0; i<BkeysCount; ++i, ++pxe, ++pB)
    {
        const crypto::public_key& B = *pB;
        XEntry& xe = *pxe;
        xe.Bhash = getBhash(B);
        //get rB key
        crypto::key_derivation rBv;
        crypto::generate_key_derivation(B, r, rBv);
        crypto::secret_key rB;
        crypto::derivation_to_scalar(rBv, 0, rB);
        //encrypt X with rB key
        encryptChacha(reinterpret_cast<const uint8_t*>(&X), sizeof(X), rB, xe.cipherX);
    }
    return msgSize;
}

/*!
 * \brief decryptMsg - (reverse of encryptMsg) decrypts data for one of the recipients using his secret key b.
 *
 * \param inputSize - input buffer size.
 * \param input - input buffer to decrypt.
 * \param bkey - secret key corresponding to one of Bs that were used to encrypt.
 * \param outputSize - output buffer size.
 * \param output - output buffer.
 * \returns output size required if output==nullptr or outputSize is not enough
 * returns 0 on error
 */

size_t decryptMsg(size_t inputSize, const uint8_t* input, const crypto::secret_key& bkey, size_t outputSize, uint8_t* output)
{
    if(!input || inputSize <= sizeof(CryptoMessageHead))
        return 0;
    //prepare
    const CryptoMessageHead& head = *reinterpret_cast<const CryptoMessageHead*>(input);
    size_t head_count = little_to_native(head.count);
    size_t head_plainSize = little_to_native(head.plainSize);
    size_t msgHeadSize = sizeof(CryptoMessageHead) + ((size_t)(head_count - 1)) * sizeof(XEntry);
    size_t msgSize = msgHeadSize + getEncryptChachaSize(head_plainSize);
    if(inputSize < msgSize)
        return 0;
    if(outputSize < head_plainSize)
        return head_plainSize;
    if(!output)
        return 0;

    //get Bhash from b
    const crypto::secret_key& b = bkey;
    uint32_t Bhash;
    {
        crypto::public_key B;
        bool res = crypto::secret_key_to_public_key(bkey, B);
        if(!res) return false; //corrupted key
        Bhash = getBhash(B);
    }
    //find XEntry for B
    const XEntry* pxe = head.xentries;
    for(size_t i=0; i<head_count; ++i, ++pxe)
    {
        const XEntry& xe = *pxe;
        if(xe.Bhash != Bhash) continue;
        //get bR key
        crypto::key_derivation bRv;
        crypto::generate_key_derivation(head.R, b, bRv);
        crypto::secret_key bR;
        crypto::derivation_to_scalar(bRv, 0, bR);
        //decrypt to X
        SessionX X;
        decryptChacha(xe.cipherX, sizeof(xe.cipherX), bR, reinterpret_cast<uint8_t*>(&X));
        if(X.cstart != cStart || X.cend != cEnd) continue;
        //decrypt with session key
        decryptChacha(input + msgHeadSize, getEncryptChachaSize(head_plainSize), X.x, output);
        return head_plainSize;
    }
    return 0;
}

} //namespace

namespace graft::crypto_tools {

void encryptMessage(const std::string& input, const std::vector<crypto::public_key>& Bkeys, std::string& output)
{
    assert(!input.empty());
    //get output size
    size_t size = encryptMsg( input.size(), nullptr, Bkeys.size(), nullptr, 0, nullptr);
    assert(0<size);
    output.resize(size);
    //encrypt
    size_t res = encryptMsg( input.size(), reinterpret_cast<const uint8_t*>(input.data()),
                             Bkeys.size(), &Bkeys[0],
            output.size(), reinterpret_cast<uint8_t*>(&output[0]));
    assert(res == size);
}

void encryptMessage(const std::string& input, const crypto::public_key& Bkey, std::string& output)
{
    std::vector<crypto::public_key> v(1, Bkey);
    encryptMessage(input, v, output);
}

bool decryptMessage(const std::string& input, const crypto::secret_key& bkey, std::string& output)
{
    assert(!input.empty());
    //get output size
    size_t size = decryptMsg( input.size(), reinterpret_cast<const uint8_t*>(input.data()), bkey, 0, nullptr);
    if(!size)
    {
        output.clear();
        return false;
    }
    output.resize(size);
    //encrypt
    size_t res = decryptMsg( input.size(), reinterpret_cast<const uint8_t*>(input.data()),
                             bkey, output.size(), reinterpret_cast<uint8_t*>(&output[0]));
    if(!res)
    {
        output.clear();
        return false;
    }
    assert(res == size);
    return true;
}

} //namespace graft::crypto_tools

