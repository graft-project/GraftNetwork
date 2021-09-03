// Copyright (c) 2017-2018, The Graft Project
// Copyright (c) 2014-2018, The Monero Project
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
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>
#include <boost/archive/portable_binary_iarchive.hpp>
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "ringct/rctSigs.h"
#include "serialization/binary_archive.h"
#include "serialization/json_archive.h"
#include "serialization/debug_archive.h"
#include "serialization/variant.h"
#include "serialization/vector.h"
#include "serialization/binary_utils.h"
#include "wallet/wallet2.h"
#include "gtest/gtest.h"
#include "unit_tests_utils.h"
#include "device/device.hpp"
using namespace std;
using namespace crypto;

namespace {
    static const string ADDRESS1 = "F8ER6NJ6zka6keUKJjX8ry44mVaXuQeVg5dPsuW3gyWRDzxCXpwuHVkMCNmrXZEMVHMFo5zEkoNTeb95hkqWgzMDSWFvana";
    static const string ADDRESS2 = "F85wjfH5DH6Hyo27TbCG98aBDw5J7xEYpJL2QC2cj5TTdinKu3XE6z1Uojry6aa9py94H9RCnQu4gdM6ywERTvCHVfDcMoE";
}

struct Struct
{
  int32_t a;
  int32_t b;
  char blob[8];
};

template <class Archive>
struct serializer<Archive, Struct>
{
  static bool serialize(Archive &ar, Struct &s) {
    ar.begin_object();
    ar.tag("a");
    ar.serialize_int(s.a);
    ar.tag("b");
    ar.serialize_int(s.b);
    ar.tag("blob");
    ar.serialize_blob(s.blob, sizeof(s.blob));
    ar.end_object();
    return true;
  }
};

struct Struct1
{
  vector<boost::variant<Struct, int32_t>> si;
  vector<int16_t> vi;

  BEGIN_SERIALIZE_OBJECT()
    FIELD(si)
    FIELD(vi)
  END_SERIALIZE()
  /*template <bool W, template <bool> class Archive>
  bool do_serialize(Archive<W> &ar)
  {
    ar.begin_object();
    ar.tag("si");
    ::do_serialize(ar, si);
    ar.tag("vi");
    ::do_serialize(ar, vi);
    ar.end_object();
  }*/
};

struct Blob
{
  uint64_t a;
  uint32_t b;

  bool operator==(const Blob& rhs) const
  {
    return a == rhs.a;
  }
};

VARIANT_TAG(binary_archive, Struct, 0xe0);
VARIANT_TAG(binary_archive, int, 0xe1);
VARIANT_TAG(json_archive, Struct, "struct");
VARIANT_TAG(json_archive, int, "int");
VARIANT_TAG(debug_archive, Struct1, "struct1");
VARIANT_TAG(debug_archive, Struct, "struct");
VARIANT_TAG(debug_archive, int, "int");

BLOB_SERIALIZER(Blob);

bool try_parse(const string &blob)
{
  Struct1 s1;
  return serialization::parse_binary(blob, s1);
}

TEST(Serialization, BinaryArchiveInts) {
  uint64_t x = 0xff00000000, x1;

  ostringstream oss;
  binary_archive<true> oar(oss);
  oar.serialize_int(x);
  ASSERT_TRUE(oss.good());
  ASSERT_EQ(8, oss.str().size());
  ASSERT_EQ(string("\0\0\0\0\xff\0\0\0", 8), oss.str());

  istringstream iss(oss.str());
  binary_archive<false> iar(iss);
  iar.serialize_int(x1);
  ASSERT_EQ(8, iss.tellg());
  ASSERT_TRUE(iss.good());

  ASSERT_EQ(x, x1);
}

TEST(Serialization, BinaryArchiveVarInts) {
  uint64_t x = 0xff00000000, x1;

  ostringstream oss;
  binary_archive<true> oar(oss);
  oar.serialize_varint(x);
  ASSERT_TRUE(oss.good());
  ASSERT_EQ(6, oss.str().size());
  ASSERT_EQ(string("\x80\x80\x80\x80\xF0\x1F", 6), oss.str());

  istringstream iss(oss.str());
  binary_archive<false> iar(iss);
  iar.serialize_varint(x1);
  ASSERT_TRUE(iss.good());
  ASSERT_EQ(x, x1);
}

TEST(Serialization, Test1) {
  ostringstream str;
  binary_archive<true> ar(str);

  Struct1 s1;
  s1.si.push_back(0);
  {
    Struct s;
    s.a = 5;
    s.b = 65539;
    std::memcpy(s.blob, "12345678", 8);
    s1.si.push_back(s);
  }
  s1.si.push_back(1);
  s1.vi.push_back(10);
  s1.vi.push_back(22);

  string blob;
  ASSERT_TRUE(serialization::dump_binary(s1, blob));
  ASSERT_TRUE(try_parse(blob));

  ASSERT_EQ('\xE0', blob[6]);
  blob[6] = '\xE1';
  ASSERT_FALSE(try_parse(blob));
  blob[6] = '\xE2';
  ASSERT_FALSE(try_parse(blob));
}

TEST(Serialization, Overflow) {
  Blob x = { 0xff00000000 };
  Blob x1;

  string blob;
  ASSERT_TRUE(serialization::dump_binary(x, blob));
  ASSERT_EQ(sizeof(Blob), blob.size());

  ASSERT_TRUE(serialization::parse_binary(blob, x1));
  ASSERT_EQ(x, x1);

  vector<Blob> bigvector;
  ASSERT_FALSE(serialization::parse_binary(blob, bigvector));
  ASSERT_EQ(0, bigvector.size());
}

TEST(Serialization, serializes_vector_uint64_as_varint)
{
  std::vector<uint64_t> v;
  string blob;

  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(1, blob.size());

  // +1 byte
  v.push_back(0);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(2, blob.size());

  // +1 byte
  v.push_back(1);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(3, blob.size());

  // +2 bytes
  v.push_back(0x80);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(5, blob.size());

  // +2 bytes
  v.push_back(0xFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(7, blob.size());

  // +2 bytes
  v.push_back(0x3FFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(9, blob.size());

  // +3 bytes
  v.push_back(0x40FF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(12, blob.size());

  // +10 bytes
  v.push_back(0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(22, blob.size());
}

TEST(Serialization, serializes_vector_int64_as_fixed_int)
{
  std::vector<int64_t> v;
  string blob;

  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(1, blob.size());

  // +8 bytes
  v.push_back(0);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(9, blob.size());

  // +8 bytes
  v.push_back(1);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(17, blob.size());

  // +8 bytes
  v.push_back(0x80);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(25, blob.size());

  // +8 bytes
  v.push_back(0xFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(33, blob.size());

  // +8 bytes
  v.push_back(0x3FFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(41, blob.size());

  // +8 bytes
  v.push_back(0x40FF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(49, blob.size());

  // +8 bytes
  v.push_back(0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(serialization::dump_binary(v, blob));
  ASSERT_EQ(57, blob.size());
}

namespace
{
  template<typename T>
  std::vector<T> linearize_vector2(const std::vector< std::vector<T> >& vec_vec)
  {
    std::vector<T> res;
    for (const auto& vec : vec_vec)
    {
      res.insert(res.end(), vec.begin(), vec.end());
    }
    return res;
  }
}

TEST(Serialization, serializes_transacion_signatures_correctly)
{
  using namespace cryptonote;

  transaction tx;
  transaction tx1;
  string blob;

  // Empty tx
  tx.set_null();
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(5, blob.size()); // 5 bytes + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Miner tx without signatures
  txin_gen txin_gen1;
  txin_gen1.height = 0;
  tx.set_null();
  tx.vin.push_back(txin_gen1);
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(7, blob.size()); // 5 bytes + 2 bytes vin[0] + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Miner tx with empty signatures 2nd vector
  tx.signatures.resize(1);
  tx.invalidate_hashes();
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(7, blob.size()); // 5 bytes + 2 bytes vin[0] + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Miner tx with one signature
  tx.signatures[0].resize(1);
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Miner tx with 2 empty vectors
  tx.signatures.resize(2);
  tx.signatures[0].resize(0);
  tx.signatures[1].resize(0);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Miner tx with 2 signatures
  tx.signatures[0].resize(1);
  tx.signatures[1].resize(1);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Two txin_gen, no signatures
  tx.vin.push_back(txin_gen1);
  tx.signatures.resize(0);
  tx.invalidate_hashes();
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(9, blob.size()); // 5 bytes + 2 * 2 bytes vins + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Two txin_gen, signatures vector contains only one empty element
  tx.signatures.resize(1);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Two txin_gen, signatures vector contains two empty elements
  tx.signatures.resize(2);
  tx.invalidate_hashes();
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_EQ(9, blob.size()); // 5 bytes + 2 * 2 bytes vins + 0 bytes extra + 0 bytes signatures
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Two txin_gen, signatures vector contains three empty elements
  tx.signatures.resize(3);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Two txin_gen, signatures vector contains two non empty elements
  tx.signatures.resize(2);
  tx.signatures[0].resize(1);
  tx.signatures[1].resize(1);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // A few bytes instead of signature
  tx.vin.clear();
  tx.vin.push_back(txin_gen1);
  tx.signatures.clear();
  tx.invalidate_hashes();
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  blob.append(std::string(sizeof(crypto::signature) / 2, 'x'));
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));

  // blob contains one signature
  blob.append(std::string(sizeof(crypto::signature) / 2, 'y'));
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));

  // Not enough signature vectors for all inputs
  txin_to_key txin_to_key1;
  txin_to_key1.amount = 1;
  memset(&txin_to_key1.k_image, 0x42, sizeof(crypto::key_image));
  txin_to_key1.key_offsets.push_back(12);
  txin_to_key1.key_offsets.push_back(3453);
  tx.vin.clear();
  tx.vin.push_back(txin_to_key1);
  tx.vin.push_back(txin_to_key1);
  tx.signatures.resize(1);
  tx.signatures[0].resize(2);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // Too much signatures for two inputs
  tx.signatures.resize(3);
  tx.signatures[0].resize(2);
  tx.signatures[1].resize(2);
  tx.signatures[2].resize(2);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // First signatures vector contains too little elements
  tx.signatures.resize(2);
  tx.signatures[0].resize(1);
  tx.signatures[1].resize(2);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // First signatures vector contains too much elements
  tx.signatures.resize(2);
  tx.signatures[0].resize(3);
  tx.signatures[1].resize(2);
  tx.invalidate_hashes();
  ASSERT_FALSE(serialization::dump_binary(tx, blob));

  // There are signatures for each input
  tx.signatures.resize(2);
  tx.signatures[0].resize(2);
  tx.signatures[1].resize(2);
  tx.invalidate_hashes();
  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_EQ(linearize_vector2(tx.signatures), linearize_vector2(tx1.signatures));

  // Blob doesn't contain enough data
  blob.resize(blob.size() - sizeof(crypto::signature) / 2);
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));

  // Blob contains too much data
  blob.resize(blob.size() + sizeof(crypto::signature));
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));

  // Blob contains one excess signature
  blob.resize(blob.size() + sizeof(crypto::signature) / 2);
  ASSERT_FALSE(serialization::parse_binary(blob, tx1));
}

TEST(Serialization, serializes_ringct_types)
{
  string blob;
  rct::key key0, key1;
  rct::keyV keyv0, keyv1;
  rct::keyM keym0, keym1;
  rct::ctkey ctkey0, ctkey1;
  rct::ctkeyV ctkeyv0, ctkeyv1;
  rct::ctkeyM ctkeym0, ctkeym1;
  rct::ecdhTuple ecdh0, ecdh1;
  rct::boroSig boro0, boro1;
  rct::mgSig mg0, mg1;
  rct::Bulletproof bp0, bp1;
  rct::rctSig s0, s1;
  cryptonote::transaction tx0, tx1;

  key0 = rct::skGen();
  ASSERT_TRUE(serialization::dump_binary(key0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, key1));
  ASSERT_TRUE(key0 == key1);

  keyv0 = rct::skvGen(30);
  for (size_t n = 0; n < keyv0.size(); ++n)
    keyv0[n] = rct::skGen();
  ASSERT_TRUE(serialization::dump_binary(keyv0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, keyv1));
  ASSERT_TRUE(keyv0.size() == keyv1.size());
  for (size_t n = 0; n < keyv0.size(); ++n)
  {
    ASSERT_TRUE(keyv0[n] == keyv1[n]);
  }

  keym0 = rct::keyMInit(9, 12);
  for (size_t n = 0; n < keym0.size(); ++n)
    for (size_t i = 0; i < keym0[n].size(); ++i)
      keym0[n][i] = rct::skGen();
  ASSERT_TRUE(serialization::dump_binary(keym0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, keym1));
  ASSERT_TRUE(keym0.size() == keym1.size());
  for (size_t n = 0; n < keym0.size(); ++n)
  {
    ASSERT_TRUE(keym0[n].size() == keym1[n].size());
    for (size_t i = 0; i < keym0[n].size(); ++i)
    {
      ASSERT_TRUE(keym0[n][i] == keym1[n][i]);
    }
  }

  rct::skpkGen(ctkey0.dest, ctkey0.mask);
  ASSERT_TRUE(serialization::dump_binary(ctkey0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, ctkey1));
  ASSERT_TRUE(!memcmp(&ctkey0, &ctkey1, sizeof(ctkey0)));

  ctkeyv0 = std::vector<rct::ctkey>(14);
  for (size_t n = 0; n < ctkeyv0.size(); ++n)
    rct::skpkGen(ctkeyv0[n].dest, ctkeyv0[n].mask);
  ASSERT_TRUE(serialization::dump_binary(ctkeyv0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, ctkeyv1));
  ASSERT_TRUE(ctkeyv0.size() == ctkeyv1.size());
  for (size_t n = 0; n < ctkeyv0.size(); ++n)
  {
    ASSERT_TRUE(!memcmp(&ctkeyv0[n], &ctkeyv1[n], sizeof(ctkeyv0[n])));
  }

  ctkeym0 = std::vector<rct::ctkeyV>(9);
  for (size_t n = 0; n < ctkeym0.size(); ++n)
  {
    ctkeym0[n] = std::vector<rct::ctkey>(11);
    for (size_t i = 0; i < ctkeym0[n].size(); ++i)
      rct::skpkGen(ctkeym0[n][i].dest, ctkeym0[n][i].mask);
  }
  ASSERT_TRUE(serialization::dump_binary(ctkeym0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, ctkeym1));
  ASSERT_TRUE(ctkeym0.size() == ctkeym1.size());
  for (size_t n = 0; n < ctkeym0.size(); ++n)
  {
    ASSERT_TRUE(ctkeym0[n].size() == ctkeym1[n].size());
    for (size_t i = 0; i < ctkeym0.size(); ++i)
    {
      ASSERT_TRUE(!memcmp(&ctkeym0[n][i], &ctkeym1[n][i], sizeof(ctkeym0[n][i])));
    }
  }

  ecdh0.mask = rct::skGen();
  ecdh0.amount = rct::skGen();
  ASSERT_TRUE(serialization::dump_binary(ecdh0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, ecdh1));
  ASSERT_TRUE(!memcmp(&ecdh0.mask, &ecdh1.mask, sizeof(ecdh0.mask)));
  ASSERT_TRUE(!memcmp(&ecdh0.amount, &ecdh1.amount, sizeof(ecdh0.amount)));

  for (size_t n = 0; n < 64; ++n)
  {
    boro0.s0[n] = rct::skGen();
    boro0.s1[n] = rct::skGen();
  }
  boro0.ee = rct::skGen();
  ASSERT_TRUE(serialization::dump_binary(boro0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, boro1));
  ASSERT_TRUE(!memcmp(&boro0, &boro1, sizeof(boro0)));

  // create a full rct signature to use its innards
  vector<uint64_t> inamounts;
  rct::ctkeyV sc, pc;
  rct::ctkey sctmp, pctmp;
  inamounts.push_back(6000);
  tie(sctmp, pctmp) = rct::ctskpkGen(inamounts.back());
  sc.push_back(sctmp);
  pc.push_back(pctmp);
  inamounts.push_back(7000);
  tie(sctmp, pctmp) = rct::ctskpkGen(inamounts.back());
  sc.push_back(sctmp);
  pc.push_back(pctmp);
  vector<uint64_t> amounts;
  rct::keyV amount_keys;
  //add output 500
  amounts.push_back(500);
  amount_keys.push_back(rct::hash_to_scalar(rct::zero()));
  rct::keyV destinations;
  rct::key Sk, Pk;
  rct::skpkGen(Sk, Pk);
  destinations.push_back(Pk);
  //add output for 12500
  amounts.push_back(12500);
  amount_keys.push_back(rct::hash_to_scalar(rct::zero()));
  rct::skpkGen(Sk, Pk);
  destinations.push_back(Pk);
  //compute rct data with mixin 3; TODO: Graft: it was 500
  const rct::RCTConfig rct_config{ rct::RangeProofPaddedBulletproof, 0 };
  s0 = rct::genRctSimple(rct::zero(), sc, pc, destinations, inamounts, amounts, amount_keys, NULL, NULL, 0, 3, rct_config, hw::get_device("default"));

  mg0 = s0.p.MGs[0];
  ASSERT_TRUE(serialization::dump_binary(mg0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, mg1));
  ASSERT_TRUE(mg0.ss.size() == mg1.ss.size());
  for (size_t n = 0; n < mg0.ss.size(); ++n)
  {
    ASSERT_TRUE(mg0.ss[n] == mg1.ss[n]);
  }
  ASSERT_TRUE(mg0.cc == mg1.cc);

  // mixRing and II are not serialized, they are meant to be reconstructed
  ASSERT_TRUE(mg1.II.empty());

  ASSERT_FALSE(s0.p.bulletproofs.empty());
  bp0 = s0.p.bulletproofs.front();
  ASSERT_TRUE(serialization::dump_binary(bp0, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, bp1));
  bp1.V = bp0.V; // this is not saved, as it is reconstructed from other tx data
  ASSERT_EQ(bp0, bp1);
}

// TODO: Graft: remove
TEST(Serialization, serializes_rta_transaction_correctly)
{
  string blob;

  // Empty tx
  cryptonote::transaction tx;
  cryptonote::transaction tx1;
  tx.version = 3;
  tx.type = cryptonote::transaction::tx_type_rta;
  cryptonote::rta_header rta_hdr_in, rta_hdr_out;
  std::vector<cryptonote::account_base> accounts;

  for (size_t i = 0; i < 10; ++i) {
      cryptonote::account_base acc;
      acc.generate();
      rta_hdr_in.keys.push_back(acc.get_keys().m_account_address.m_view_public_key);
      accounts.push_back(acc);
  }

  cryptonote::add_graft_rta_header_to_extra(tx.extra, rta_hdr_in);

  crypto::hash tx_hash;
  ASSERT_TRUE(cryptonote::get_transaction_hash(tx, tx_hash));


  std::vector<cryptonote::rta_signature> signatures1, signatures2;

  for (size_t i = 0; i < 10; ++i) {
      crypto::signature sign;
      crypto::generate_signature(tx_hash, accounts[i].get_keys().m_account_address.m_view_public_key, accounts[i].get_keys().m_view_secret_key, sign);
      signatures1.push_back({i, sign});
  }

  ASSERT_TRUE(cryptonote::add_graft_rta_signatures_to_extra2(tx.extra2, signatures1));


  ASSERT_TRUE(serialization::dump_binary(tx, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, tx1));
  ASSERT_EQ(tx, tx1);
  ASSERT_TRUE(cryptonote::get_graft_rta_signatures_from_extra2(tx, signatures2));
  ASSERT_EQ(signatures1, signatures2);

  crypto::hash tx_hash1;
  ASSERT_TRUE(cryptonote::get_transaction_hash(tx1, tx_hash1));
  ASSERT_EQ(tx_hash, tx_hash1);
  ASSERT_TRUE(cryptonote::get_graft_rta_header_from_extra(tx1, rta_hdr_out));
  ASSERT_EQ(rta_hdr_in, rta_hdr_out);
}

TEST(Serialization, empty_rta_signatures)
{
  string blob;

  // Empty tx
  cryptonote::transaction tx_in;
  cryptonote::transaction tx_out;
  tx_in.version = 3;
  tx_in.type = cryptonote::transaction::tx_type_rta;
  cryptonote::rta_header rta_hdr_in, rta_hdr_out;
  std::vector<cryptonote::account_base> accounts;

  for (size_t i = 0; i < 10; ++i) {
      cryptonote::account_base acc;
      acc.generate();
      rta_hdr_in.keys.push_back(acc.get_keys().m_account_address.m_view_public_key);
      accounts.push_back(acc);
  }

  cryptonote::add_graft_rta_header_to_extra(tx_in.extra, rta_hdr_in);

  crypto::hash tx_hash;
  ASSERT_TRUE(cryptonote::get_transaction_hash(tx_in, tx_hash));


  std::vector<cryptonote::rta_signature> signatures1, signatures2;

  ASSERT_TRUE(cryptonote::add_graft_rta_signatures_to_extra2(tx_in.extra2, signatures1));


  ASSERT_TRUE(serialization::dump_binary(tx_in, blob));
  ASSERT_TRUE(serialization::parse_binary(blob, tx_out));
  ASSERT_EQ(tx_in, tx_out);
  ASSERT_TRUE(cryptonote::get_graft_rta_signatures_from_extra2(tx_out, signatures2));
  ASSERT_EQ(signatures1, signatures2);

}


// TODO(loki): These tests are broken because they rely on testnet which has
// since been restarted, and so the genesis block of these predefined wallets
// are broken
//             - 2019-02-25 Doyle

#if 0
TEST(Serialization, portability_wallet)
{
  const cryptonote::network_type nettype = cryptonote::TESTNET;
  //const bool restricted = false;
  //tools::wallet2 w(nettype, restricted);
  tools::wallet2 w(nettype);
  const boost::filesystem::path wallet_file = unit_test::data_dir / "wallet_serialization_portability";
  string password = "test";
  bool r = false;
  try
  {
    w.load(wallet_file.string(), password);
    r = true;
  }
  catch (const exception& e)
  {}
  ASSERT_TRUE(r);
  /*
  fields of tools::wallet2 to be checked:
    std::vector<crypto::hash>                                       m_blockchain
    std::vector<transfer_details>                                   m_transfers               // TODO
    cryptonote::account_public_address                              m_account_public_address
    std::unordered_map<crypto::key_image, size_t>                   m_key_images
    std::unordered_map<crypto::hash, unconfirmed_transfer_details>  m_unconfirmed_txs
    std::unordered_multimap<crypto::hash, payment_details>          m_payments
    std::unordered_map<crypto::hash, crypto::secret_key>            m_tx_keys
    std::unordered_map<crypto::hash, confirmed_transfer_details>    m_confirmed_txs
    std::unordered_map<crypto::hash, std::string>                   m_tx_notes
    std::unordered_map<crypto::hash, payment_details>               m_unconfirmed_payments
    std::unordered_map<crypto::public_key, size_t>                  m_pub_keys
    std::vector<tools::wallet2::address_book_row>                   m_address_book
  */
  // blockchain


  // reset blockchain to block1
  //  auto bk1 = w.m_blockchain.at(0);
  //  w.m_blockchain.clear();
  //  w.m_blockchain.push_back(bk1);
  //  w.store();

  ASSERT_TRUE(w.m_blockchain.size() == 1);

//  std::cout << "bc1 hash: " << epee::string_tools::pod_to_hex(w.m_blockchain[0]) << std::endl;
//  std::cout << "transfers.size: " << w.m_transfers.size() << std::endl;
//  std::cout << "m_account_public_address.m_view_public_key: " << epee::string_tools::pod_to_hex(w.m_account_public_address.m_view_public_key)<< std::endl;
//  std::cout << "m_account_public_address.m_spend_public_key: " << epee::string_tools::pod_to_hex(w.m_account_public_address.m_spend_public_key)<< std::endl;
//  std::cout << "m_key_images.size(): " << w.m_key_images.size() << std::endl;

  ASSERT_TRUE(epee::string_tools::pod_to_hex(w.m_blockchain[0]) == "94752090109e778fc031bde61d01bbcedc6131a4f3c0f37311bf7d37e5be6c0c");
  // transfers (TODO)
  ASSERT_TRUE(w.m_transfers.size() == 3);

  // account public address
  ASSERT_TRUE(epee::string_tools::pod_to_hex(w.m_account_public_address.m_view_public_key) == "16599404f42d78ff494f604bda38430c4720c77bdae1e7b4960ffca825c05b5e");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(w.m_account_public_address.m_spend_public_key) == "0c3b464f0f6922879ea76cdc553f18620b3528c7c8ab665e3b52c54b72795c90");
  // key images
  ASSERT_TRUE(w.m_key_images.size() == 3);
  {
    crypto::key_image ki[3];
    epee::string_tools::hex_to_pod("9c19942199c94fe15c39afd079a454caa6ce84c018009adad310f4e26e520821", ki[0]);
    epee::string_tools::hex_to_pod("38fd58fe20b4350ddd1f91a84f6cd44ad48ffaab71814506c0beb08a275a8394", ki[1]);
    epee::string_tools::hex_to_pod("cb135da497d98f9a000c6915131cf4b9481744918a8cf95e17e8711c6ea268d4", ki[2]);


//    for (const auto &it : w.m_key_images) {
//        std::cout << "key_image: " << epee::string_tools::pod_to_hex(it.first) << std::endl;
//        std::cout << "size: " << it.second << std::endl;
//    }

    ASSERT_EQ_MAP(0, w.m_key_images, ki[0]);
    ASSERT_EQ_MAP(1, w.m_key_images, ki[1]);
    ASSERT_EQ_MAP(2, w.m_key_images, ki[2]);

  }

  // unconfirmed txs
  ASSERT_TRUE(w.m_unconfirmed_txs.size() == 0);

  // payments
  ASSERT_TRUE(w.m_payments.size() == 1);
  {
    auto pd0 = w.m_payments.begin();
    auto pd1 = pd0;
    ++pd1;
    ASSERT_TRUE(epee::string_tools::pod_to_hex(pd0->first) == "0000000000000000000000000000000000000000000000000000000000000000");
    ASSERT_TRUE(epee::string_tools::pod_to_hex(pd1->first) == "0000000000000000000000000000000000000000000000000000000000000000");
    if (epee::string_tools::pod_to_hex(pd0->second.m_tx_hash) == "199ea366ca190788e12e7849c9625dd7cad33d2b926de10294005fc20cd41576")
      swap(pd0, pd1);

//    std::cout << "pd0->second.m_tx_hash:" << epee::string_tools::pod_to_hex(pd0->second.m_tx_hash) << std::endl;
//    std::cout << "pd1->second.m_tx_hash:" << epee::string_tools::pod_to_hex(pd1->second.m_tx_hash) << std::endl;

//    std::cout << "pd0->second.m_amount: " << pd0->second.m_amount << std::endl;
//    std::cout << "pd1->second.m_amount: " << pd1->second.m_amount << std::endl;

//    std::cout << "pd0->second.m_block_height: " << pd0->second.m_block_height << std::endl;
//    std::cout << "pd1->second.m_block_height: " << pd1->second.m_block_height << std::endl;

//    std::cout << "pd0->second.m_unlock_time: " << pd0->second.m_unlock_time << std::endl;
//    std::cout << "pd1->second.m_unlock_time: " << pd1->second.m_unlock_time << std::endl;

//    std::cout << "pd0->second.m_timestamp: " << pd0->second.m_timestamp << std::endl;
//    std::cout << "pd1->second.m_timestamp: " << pd1->second.m_timestamp << std::endl;


    ASSERT_TRUE(epee::string_tools::pod_to_hex(pd0->second.m_tx_hash) == "98fd487fc3f6a30114f2d4e8dde8cbded186850bfeeaa0c0c2af2053da8003cd");
    ASSERT_TRUE(epee::string_tools::pod_to_hex(pd1->second.m_tx_hash) == "199ea366ca190788e12e7849c9625dd7cad33d2b926de10294005fc20cd41576");
    ASSERT_TRUE(pd0->second.m_amount == 1200000000000);
    ASSERT_TRUE(pd1->second.m_amount == 13400000000000);
    ASSERT_TRUE(pd1->second.m_block_height == 586);
    ASSERT_TRUE(pd0->second.m_block_height == 632);
    ASSERT_TRUE(pd0->second.m_unlock_time == 0);
    ASSERT_TRUE(pd1->second.m_unlock_time == 0);
    ASSERT_TRUE(pd0->second.m_timestamp == 1510673887);
    ASSERT_TRUE(pd1->second.m_timestamp == 1510673233);

  }

  // tx keys
  ASSERT_TRUE(w.m_tx_keys.size() == 1);
  {
    const std::vector<std::pair<std::string, std::string>> txid_txkey =
    {
      {"22d51db704e40da3b4b359f51b7560446af049dfd2cf01d26d761631a371b911", "474054ac26dd71f87c99f62a29fd7bc05fcc8cb6bded11e03732f359be152e03"},
    };

    for (size_t i = 0; i < txid_txkey.size(); ++i)
    {
      crypto::hash txid;
      crypto::secret_key txkey;
      epee::string_tools::hex_to_pod(txid_txkey[i].first, txid);
      epee::string_tools::hex_to_pod(txid_txkey[i].second, txkey);
      ASSERT_EQ_MAP(txkey, w.m_tx_keys, txid);
    }
  }

  // confirmed txs
  ASSERT_TRUE(w.m_confirmed_txs.size() == 2);

  // tx notes
  ASSERT_TRUE(w.m_tx_notes.size() == 1);
  {
    crypto::hash h[2];
    epee::string_tools::hex_to_pod("199ea366ca190788e12e7849c9625dd7cad33d2b926de10294005fc20cd41576", h[0]);
    epee::string_tools::hex_to_pod("22d51db704e40da3b4b359f51b7560446af049dfd2cf01d26d761631a371b911", h[1]);
    ASSERT_EQ_MAP("sample note", w.m_tx_notes, h[0]);
    ASSERT_EQ_MAP("sample note 2", w.m_tx_notes, h[1]);
  }

  // unconfirmed payments
  ASSERT_TRUE(w.m_unconfirmed_payments.size() == 0);

  // pub keys
  ASSERT_TRUE(w.m_pub_keys.size() == 3);
  {
    crypto::public_key pubkey[3];

//    for (auto it : w.m_pub_keys) {
//        std::cout << "pk: " << it.first << ", size: " << it.second << std::endl;
//    }
    epee::string_tools::hex_to_pod("6932b7994a60ac1a65d901466f54782748d72d1949aa0be736673f80112e6336", pubkey[0]);
    epee::string_tools::hex_to_pod("accad0137be7cfd8f5378ebdb6d6226144592aff24a6a84f1489e03e764e2fef", pubkey[1]);
    epee::string_tools::hex_to_pod("9ca6a989cb0910c2e3ad02e1f0bad5d54629bab5d5a306358548d183ec10bbba", pubkey[2]);

    ASSERT_EQ_MAP(0, w.m_pub_keys, pubkey[0]);
    ASSERT_EQ_MAP(1, w.m_pub_keys, pubkey[1]);
    ASSERT_EQ_MAP(2, w.m_pub_keys, pubkey[2]);

  }

  // address book
  ASSERT_TRUE(w.m_address_book.size() == 1);
  {
    auto address_book_row = w.m_address_book.begin();
    ASSERT_TRUE(epee::string_tools::pod_to_hex(address_book_row->m_address.m_spend_public_key) == "8c1c6fd0d62bce5b69c6738b76bc0b1183e27896a4ea937fa69d91599509d886");
    ASSERT_TRUE(epee::string_tools::pod_to_hex(address_book_row->m_address.m_view_public_key) == "1c0e75cd947b909a605d7cbc8735543d16502e25028a5d6036a67a24f7c56147");
    ASSERT_TRUE(epee::string_tools::pod_to_hex(address_book_row->m_payment_id) == "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    ASSERT_TRUE(address_book_row->m_description == "testnet wallet 9xUcAx");
  }
}

#define OUTPUT_EXPORT_FILE_MAGIC "Graft output export\003"
TEST(Serialization, portability_outputs)
{
  const bool restricted = false;
  tools::wallet2 w(cryptonote::TESTNET, restricted);

  const boost::filesystem::path wallet_file = unit_test::data_dir / "wallet_testnet";
  const string password = "test";
  w.load(wallet_file.string(), password);

  // read file
  const boost::filesystem::path filename = unit_test::data_dir / "outputs";
  std::string data;
  bool r = epee::file_io_utils::load_file_to_string(filename.string(), data);

  ASSERT_TRUE(r);
  const size_t magiclen = strlen(OUTPUT_EXPORT_FILE_MAGIC);
  ASSERT_FALSE(data.size() < magiclen || memcmp(data.data(), OUTPUT_EXPORT_FILE_MAGIC, magiclen));
  // decrypt (copied from wallet2::decrypt)
  auto decrypt = [] (const std::string &ciphertext, const crypto::secret_key &skey, bool authenticated) -> string
  {
    const size_t prefix_size = sizeof(chacha_iv) + (authenticated ? sizeof(crypto::signature) : 0);
    if(ciphertext.size() < prefix_size)
      return {};
    crypto::chacha_key key;
    crypto::generate_chacha_key(&skey, sizeof(skey), key, 1);
    const crypto::chacha_iv &iv = *(const crypto::chacha_iv*)&ciphertext[0];
    std::string plaintext;
    plaintext.resize(ciphertext.size() - prefix_size);
    if (authenticated)
    {
      crypto::hash hash;
      crypto::cn_fast_hash(ciphertext.data(), ciphertext.size() - sizeof(signature), hash);
      crypto::public_key pkey;
      crypto::secret_key_to_public_key(skey, pkey);
      const crypto::signature &signature = *(const crypto::signature*)&ciphertext[ciphertext.size() - sizeof(crypto::signature)];
      if(!crypto::check_signature(hash, pkey, signature))
        return {};
    }
    crypto::chacha8(ciphertext.data() + sizeof(iv), ciphertext.size() - prefix_size, key, iv, &plaintext[0]);
    return plaintext;
  };
  crypto::secret_key view_secret_key;
  epee::string_tools::hex_to_pod("816d922dbc4a0ae6c5649a9dd8c0aba50e9fbd996de32405d228c7c61f14a90e", view_secret_key);
  bool authenticated = true;
  data = decrypt(std::string(data, magiclen), view_secret_key, authenticated);
  ASSERT_FALSE(data.empty());
  // check public view/spend keys
  const size_t headerlen = 2 * sizeof(crypto::public_key);
  ASSERT_FALSE(data.size() < headerlen);
  const crypto::public_key &public_spend_key = *(const crypto::public_key*)&data[0];
  const crypto::public_key &public_view_key = *(const crypto::public_key*)&data[sizeof(crypto::public_key)];
  ASSERT_TRUE(epee::string_tools::pod_to_hex(public_spend_key) == "ed550c634928d78d493fd1bb127639f4a1ad5531c9a7ce8ab86f339b23f38dbf");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(public_view_key) == "b285c18da8936c1996cb1ca2063bb44e114f9d6287817ba134cbb801dfbdc099");
  r = false;
  std::vector<tools::wallet2::transfer_details> outputs;

  try
  {
    std::string body(data, headerlen);
    std::stringstream iss;
    iss << body;
    try
    {
      boost::archive::portable_binary_iarchive ar(iss);
      ar >> outputs;
      r = true;
    }
    catch (...)
    {
    }
  }
  catch (...)
  { }
  ASSERT_TRUE(r);
  /*
  fields of tools::wallet2::transfer_details to be checked:
    uint64_t                        m_block_height
    cryptonote::transaction_prefix  m_tx                        // TODO
    crypto::hash                    m_txid
    size_t                          m_internal_output_index
    uint64_t                        m_global_output_index
    bool                            m_spent
    uint64_t                        m_spent_height
    crypto::key_image               m_key_image
    rct::key                        m_mask
    uint64_t                        m_amount
    bool                            m_rct
    bool                            m_key_image_known
    size_t                          m_pk_index
  */

  // std::cout << "ouputs.size: " << outputs.size() << std::endl;
  ASSERT_TRUE(outputs.size() == 3);
  auto& td0 = outputs[0];
  auto& td1 = outputs[1];
  auto& td2 = outputs[2];
  ASSERT_TRUE(td0.m_block_height == 558);
  ASSERT_TRUE(td1.m_block_height == 559);
  ASSERT_TRUE(td2.m_block_height == 571);
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td0.m_txid) == "d7cc6d9a6fd6e5677cdaa814e1205f18948b20dae6c8c4e52077ece17ae1744a");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td1.m_txid) == "5660064154833a523398529e1594504443917b78c034cbf3ef65498cabf71f01");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td2.m_txid) == "65c07779e1682cf497740a8262a4f5f2d22e1b58d4a5187ce62b53c2ca742ef7");
  /*
  std::cout << "td0.m_internal_output_index: " << td0.m_internal_output_index << std::endl;
  std::cout << "td1.m_internal_output_index: " << td1.m_internal_output_index << std::endl;
  std::cout << "td2.m_internal_output_index: " << td2.m_internal_output_index << std::endl;
  */

  ASSERT_TRUE(td0.m_internal_output_index == 1);
  ASSERT_TRUE(td1.m_internal_output_index == 1);
  ASSERT_TRUE(td2.m_internal_output_index == 1);

  /*
  std::cout << "td0.m_global_output_index: " << td0.m_global_output_index << std::endl;
  std::cout << "td1.m_global_output_index: " << td1.m_global_output_index << std::endl;
  std::cout << "td2.m_global_output_index: " << td2.m_global_output_index << std::endl;
  */

  ASSERT_TRUE(td0.m_global_output_index == 591);
  ASSERT_TRUE(td1.m_global_output_index == 594);
  ASSERT_TRUE(td2.m_global_output_index == 608);

  /*
  std::cout << "td0.m_spent: " << td0.m_spent << std::endl;
  std::cout << "td1.m_spent: " << td1.m_spent << std::endl;
  std::cout << "td2.m_spent: " << td2.m_spent << std::endl;
  */

  ASSERT_TRUE (td0.m_spent);
  ASSERT_FALSE(td1.m_spent);
  ASSERT_FALSE(td2.m_spent);

  /*
  std::cout << "td0.m_spent_height: " << td0.m_spent_height << std::endl;
  std::cout << "td1.m_spent_height: " << td1.m_spent_height << std::endl;
  std::cout << "td2.m_spent_height: " << td2.m_spent_height << std::endl;
  */

  ASSERT_TRUE(td0.m_spent_height == 571);
  ASSERT_TRUE(td1.m_spent_height == 0);
  ASSERT_TRUE(td2.m_spent_height == 0);

  /*
  std::cout << "td0.m_key_image: " << epee::string_tools::pod_to_hex(td0.m_key_image) << std::endl;
  std::cout << "td1.m_key_image: " << epee::string_tools::pod_to_hex(td1.m_key_image) << std::endl;
  std::cout << "td2.m_key_image: " << epee::string_tools::pod_to_hex(td2.m_key_image) << std::endl;

  std::cout << "td0.m_mask: " << epee::string_tools::pod_to_hex(td0.m_mask) << std::endl;
  std::cout << "td1.m_mask: " << epee::string_tools::pod_to_hex(td1.m_mask) << std::endl;
  std::cout << "td2.m_mask: " << epee::string_tools::pod_to_hex(td2.m_mask) << std::endl;

  std::cout << "td0.m_amount: " << td0.m_amount << std::endl;
  std::cout << "td1.m_amount: " << td1.m_amount << std::endl;
  std::cout << "td2.m_amount: " << td2.m_amount << std::endl;
  */

  ASSERT_TRUE(epee::string_tools::pod_to_hex(td0.m_key_image) == "f2ec6edb5214711b4ec20708f3985f8a2ae92800fd4e1301a1a73a6ee9b78119");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td1.m_key_image) == "10f63431577a46ff3b2dc7a57921e6914ce87e5ca0c748c31b21cebde2167d14");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td2.m_key_image) == "61e06b0a1f29f19cef5181963ab5735f3f3dee5a5b26d85964c81dfb60e799f7");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td0.m_mask) == "64df2b95101186541cdf29bf3988115d825122c5b4f07e8eb4fa6d2e49230f05");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td1.m_mask) == "48cbd8dabab25085cc878bd2af84a463e53cb6dbf523363361b26ddcd9c86809");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td2.m_mask) == "7f8dcd6f36bc1bf466c5771d688b7251203aa51a8b6fc1d100758e67e03ad504");
  ASSERT_TRUE(td0.m_amount == 2000000000000);
  ASSERT_TRUE(td1.m_amount == 1000000000000);
  ASSERT_TRUE(td2.m_amount == 796000000000);
  ASSERT_TRUE(td0.m_rct);
  ASSERT_TRUE(td1.m_rct);
  ASSERT_TRUE(td2.m_rct);
  ASSERT_TRUE(td0.m_key_image_known);
  ASSERT_TRUE(td1.m_key_image_known);
  ASSERT_TRUE(td2.m_key_image_known);
  ASSERT_TRUE(td0.m_pk_index == 0);
  ASSERT_TRUE(td1.m_pk_index == 0);
  ASSERT_TRUE(td2.m_pk_index == 0);
}

namespace helper {
    void dump_unsigned_tx(const tools::wallet2::unsigned_tx_set &tx, cryptonote::network_type nettype)
    {
        std::cout << "txes.size(): " << tx.txes.size() << std::endl;
        for (int i = 0; i < tx.txes.size(); ++i) {
            auto & tcd = tx.txes[i];
            std::cout << "tcd[" << i << "].sources.size(): "  << tcd.sources.size() << std::endl;
            auto & tses = tcd.sources;
            for (int j = 0; j < tses.size(); ++j) {
                auto &tse = tses[j];
                auto &outputs = tse.outputs;
                for (int k = 0; k < outputs.size(); ++k) {
                    auto &out = outputs[k];
                    std::cout << "out[" << j << "][" << k << "].first:  " << out.first << std::endl;
                    std::cout << "out[" << j << "][" << k << "].second: " << epee::string_tools::pod_to_hex(out.second) << std::endl;
                }
                std::cout << "tse[" << j << "].real_output: " << tse.real_output;
                std::cout << "tse[" << j << "].real_out_tx_key: " << epee::string_tools::pod_to_hex(tse.real_out_tx_key) << std::endl;
                std::cout << "tse[" << j << "].amount: " << tse.amount << std::endl;
                std::cout << "tse[" << j << "].rct: " << tse.rct << std::endl;
                std::cout << "tse[" << j << "].mask: " << epee::string_tools::pod_to_hex(tse.mask) << std::endl;
            }
            std::cout << "tcd[" << i << "].change_dts.amount: "  << tcd.change_dts.amount << std::endl;
            std::cout << "tcd[" << i << "].change_dts.addr:   "  << cryptonote::get_account_address_as_str(nettype, false, tcd.change_dts.addr) << std::endl;

            auto &splitted_dsts = tcd.splitted_dsts;
            for (int j = 0; j < splitted_dsts.size(); ++j) {
                auto splitted_dst = splitted_dsts[j];
                std::cout << "tcd[" << i << "].splitted_dsts[" << j << "].amount: " << splitted_dst.amount << std::endl;
                std::cout << "tcd[" << i << "].splitted_dsts[" << j << "].addr: " <<  cryptonote::get_account_address_as_str(nettype, false, splitted_dst.addr) << std::endl;
            }

            auto &selected_transfers = tcd.selected_transfers;


            int j = 0;
            for (auto it = selected_transfers.begin(); it != selected_transfers.end(); ++it) {
                std::cout << "tcd[" << i << "].selected_transfer[" << j++ << "]: " << *it << std::endl;
            }

            std::cout << "tcd[" << i << "].extra.size(): " << tcd.extra.size() << std::endl;
            std::cout << "tcd[" << i << "].unlock_time: " << tcd.unlock_time << std::endl;
            std::cout << "tcd[" << i << "].rct: " << tcd.use_rct << std::endl;

            auto &dests = tcd.dests;
            for (int j = 0; j < dests.size(); ++j) {
                std::cout << "tcd[" << i << "].dests[" << j << "].amount: " <<  tcd.dests[j].amount << std::endl;
                std::cout << "tcd[" << i << "].dests[" << j << "].addr: " << cryptonote::get_account_address_as_str(nettype, false, tcd.dests[j].addr) << std::endl;
            }
        }


        for (int i = 0; i < tx.transfers.size(); ++i) {
            auto &td = tx.transfers[i];
            std::cout << "td[" << i << "].m_block_height: " << td.m_block_height << std::endl;
            std::cout << "td[" << i << "].m_txid: " << epee::string_tools::pod_to_hex(td.m_txid) << std::endl;
            std::cout << "td[" << i << "].m_internal_output_index: " << td.m_internal_output_index << std::endl;
            std::cout << "td[" << i << "].m_global_output_index: " << td.m_global_output_index << std::endl;
            std::cout << "td[" << i << "].m_spent: " << td.m_spent << std::endl;
            std::cout << "td[" << i << "].m_spent_height: " << td.m_spent_height << std::endl;
            std::cout << "td[" << i << "].m_key_image: " << epee::string_tools::pod_to_hex(td.m_key_image) << std::endl;
            std::cout << "td[" << i << "].m_mask: " << epee::string_tools::pod_to_hex(td.m_mask) << std::endl;
            std::cout << "td[" << i << "].m_amount: " << td.m_amount << std::endl;
            std::cout << "td[" << i << "].m_rct: " << td.m_rct << std::endl;
            std::cout << "td[" << i << "].m_key_image_known: " << td.m_key_image_known << std::endl;
            std::cout << "td[" << i << "].m_pk_index: " << td.m_pk_index << std::endl;
        }

    }
}

#define UNSIGNED_TX_PREFIX "Graft unsigned tx set\003"
TEST(Serialization, portability_unsigned_tx)
{
  const boost::filesystem::path filename = unit_test::data_dir / "unsigned_monero_tx";
  std::string s;
  const cryptonote::network_type nettype = cryptonote::TESTNET;
  bool r = epee::file_io_utils::load_file_to_string(filename.string(), s);
  ASSERT_TRUE(r);
  size_t const magiclen = strlen(UNSIGNED_TX_PREFIX);
  ASSERT_FALSE(strncmp(s.c_str(), UNSIGNED_TX_PREFIX, magiclen));
  unsigned_tx_set exported_txs;
  s = s.substr(magiclen);
  r = false;

  try
  {
    s = w.decrypt_with_view_secret_key(s);
    try
    {
      std::istringstream iss(s);
      boost::archive::portable_binary_iarchive ar(iss);
      ar >> exported_txs;
      r = true;
    }
    catch (...)
    {
    }
  }
  catch (...)
  {}
  ASSERT_TRUE(r);
  // helper::dump_unsigned_tx(exported_txs, true);
  /*
  fields of tools::wallet2::unsigned_tx_set to be checked:
    std::vector<tx_construction_data> txes
    std::vector<wallet2::transfer_details> m_transfers

  fields of toolw::wallet2::tx_construction_data to be checked:
    std::vector<cryptonote::tx_source_entry>      sources
    cryptonote::tx_destination_entry              change_dts
    std::vector<cryptonote::tx_destination_entry> splitted_dsts
    std::list<size_t>                             selected_transfers
    std::vector<uint8_t>                          extra
    uint64_t                                      unlock_time
    bool                                          use_rct
    std::vector<cryptonote::tx_destination_entry> dests

  fields of cryptonote::tx_source_entry to be checked:
    std::vector<std::pair<uint64_t, rct::ctkey>>  outputs
    size_t                                        real_output
    crypto::public_key                            real_out_tx_key
    size_t                                        real_output_in_tx_index
    uint64_t                                      amount
    bool                                          rct
    rct::key                                      mask

  fields of cryptonote::tx_destination_entry to be checked:
    uint64_t                amount
    account_public_address  addr
  */

  // txes
  ASSERT_TRUE(exported_txs.txes.size() == 1);
  auto& tcd = exported_txs.txes[0];

  // tcd.sources
  ASSERT_TRUE(tcd.sources.size() == 2);
  auto& tse = tcd.sources[0];

  // tcd.sources[0].outputs
  ASSERT_TRUE(tse.outputs.size() == 10);
  auto& out0 = tse.outputs[0];
  auto& out1 = tse.outputs[1];
  auto& out2 = tse.outputs[2];
  auto& out3 = tse.outputs[3];
  auto& out4 = tse.outputs[4];

  ASSERT_TRUE(out0.first == 34);
  ASSERT_TRUE(out1.first == 79);
  ASSERT_TRUE(out2.first == 81);
  ASSERT_TRUE(out3.first == 97);
  ASSERT_TRUE(out4.first == 116);
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out0.second) == "bd79f6db4ed2d93cc000b841ec935b606162c8285dac368b09e3e06db6776057690c312586bbdf123d9e34ad7955e1c2ae5259cd3effd0b08b19cb556d65ec25");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out1.second) == "3f920b629e61b0666730961ad846e32289ac43bc72fc1339001119bca857c4f4be8aa4ab10aab1cd1920b83243ebdfb84c2275dc0eaeee8b7e202c3d1f314c92");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out2.second) == "764c2dbecec111f58645924a3d3c327ddd13b81fa089b5a61b43e6d102840aa7f06c06d7981152fa81cee8520c271b2f3bf3b4c913f3dc6d887bd7fcb30e299c");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out3.second) == "4a89575fddc7ba0134c11e4583910ab62947fdbf3924d81cd5662b9020216e2a3e979ba58c2c25010e4168a3e9f704aaa0a10316128093819ebd22d930955df0");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out4.second) == "66e61044129c9f3de919dad3e035d57be1923a1818465f0cb6ce155a2dc86f1cde69d7c1c2558e4fdfe015a579e748603b74361a224d19bf7d089f817649387b");
  // tcd.sources[0].{real_output, real_out_tx_key, real_output_in_tx_index, amount, rct, mask}
  ASSERT_TRUE(tse.real_output == 4);

  ASSERT_TRUE(epee::string_tools::pod_to_hex(tse.real_out_tx_key) == "5e68e9272cc31399ae309e28ed4b83426fcc8f7bc98fda358b6b15d024b9842d");
  ASSERT_TRUE(tse.real_output_in_tx_index == 0);
  ASSERT_TRUE(tse.amount == 5000000000000);
  ASSERT_TRUE(tse.rct);

  ASSERT_TRUE(epee::string_tools::pod_to_hex(tse.mask) == "796309c7e57439028f111714bd04c8bbe22167bd2f7c04c21dc99b0c16478003");
  // tcd.change_dts

  ASSERT_TRUE(tcd.change_dts.amount == 7784000000000);

  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, tcd.change_dts.addr) == ADDRESS1);

  // tcd.splitted_dsts
  ASSERT_TRUE(tcd.splitted_dsts.size() == 2);
  auto& splitted_dst0 = tcd.splitted_dsts[0];
  auto& splitted_dst1 = tcd.splitted_dsts[1];

  ASSERT_TRUE(splitted_dst0.amount == 1000000000000);
  ASSERT_TRUE(splitted_dst1.amount == 7784000000000);

  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, splitted_dst0.addr) == ADDRESS2);
  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, splitted_dst1.addr) == ADDRESS1);

  // tcd.selected_transfers
  ASSERT_TRUE(tcd.selected_transfers.size() == 2);
  ASSERT_TRUE(tcd.selected_transfers.front() == 0);

  // tcd.extra
  ASSERT_TRUE(tcd.extra.size() == 33);
  // tcd.{unlock_time, use_rct}
  ASSERT_TRUE(tcd.unlock_time == 0);
  // ASSERT_TRUE(tcd.use_rct);

  // tcd.dests
  ASSERT_TRUE(tcd.dests.size() == 1);
  auto& dest = tcd.dests[0];

  ASSERT_TRUE(dest.amount == 1000000000000);
  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, dest.addr) == ADDRESS2);
  // transfers
  ASSERT_TRUE(exported_txs.transfers.size() == 2);
  auto& td0 = exported_txs.transfers[0];
  auto& td1 = exported_txs.transfers[1];

  ASSERT_TRUE(td0.m_block_height == 116);
  ASSERT_TRUE(td1.m_block_height == 137);
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td0.m_txid) == "751ff215db8f9f5a336bf0df14d379551fa9880da981c33f469f858c434d9f94");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td1.m_txid) == "1f15f51b40cc56c6b627b2ae22b783be9e1d62609cae92f1ebccd10c2166d7ff");
  ASSERT_TRUE(td0.m_internal_output_index == 0);
  ASSERT_TRUE(td1.m_internal_output_index == 1);
  ASSERT_TRUE(td0.m_global_output_index == 116);
  ASSERT_TRUE(td1.m_global_output_index == 140);

  // TODO: not clear how to save "spent" tx to file with regular (non view only) wallet
  ASSERT_FALSE(td0.m_spent);
  ASSERT_FALSE(td1.m_spent);

  ASSERT_TRUE(td0.m_spent_height == 0);
  ASSERT_TRUE(td1.m_spent_height == 0);
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td0.m_key_image) == "2a37328f8b3b8ce84188eb3c8645b42609fc29987189bfd2e78053b82f9c3beb");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td1.m_key_image) == "a10c3eed1263324df14745a9e832548a6fe8e65f7cdfc56d68e1ab6088fcda43");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td0.m_mask) == "796309c7e57439028f111714bd04c8bbe22167bd2f7c04c21dc99b0c16478003");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(td1.m_mask) == "e1c9ec9eecae42f5a36b4de28738eb0718b026019686507bed8246fa3e96a202");

  ASSERT_TRUE(td0.m_amount == 5000000000000);
  ASSERT_TRUE(td1.m_amount == 3896000000000);

  ASSERT_TRUE(td0.m_rct);
  ASSERT_TRUE(td1.m_rct);

  // TODO: ASSERT_TRUE in monero code and test tx file
  ASSERT_FALSE(td0.m_key_image_known);
  ASSERT_FALSE(td1.m_key_image_known);
  ASSERT_TRUE(td0.m_pk_index == 0);
  ASSERT_TRUE(td1.m_pk_index == 0);

}

#define SIGNED_TX_PREFIX "Graft signed tx set\003"
TEST(Serialization, portability_signed_tx)
{
  // TODO: Graft: pick the Loki's code with 'wallet'
  const boost::filesystem::path filename = unit_test::data_dir / "signed_monero_tx";
  const cryptonote::network_type nettype = cryptonote::TESTNET;
  std::string s;
  bool r = epee::file_io_utils::load_file_to_string(filename.string(), s);
  ASSERT_TRUE(r);
  const size_t magiclen = strlen(SIGNED_TX_PREFIX);
  ASSERT_FALSE(strncmp(s.c_str(), SIGNED_TX_PREFIX, magiclen));
  const cryptonote::network_type nettype = cryptonote::TESTNET;
  std::string s;
  bool r = epee::file_io_utils::load_file_to_string(filename.string(), s);
  ASSERT_TRUE(r);
  size_t const magiclen = strlen(SIGNED_TX_PREFIX);
  ASSERT_FALSE(strncmp(s.c_str(), SIGNED_TX_PREFIX, magiclen));
  tools::wallet2::signed_tx_set exported_txs;
  s = s.substr(magiclen);
  r = false;

  try
  {
    std::istringstream iss(s);
    boost::archive::portable_binary_iarchive ar(iss);
    ar >> exported_txs;
    r = true;
  }
  catch (const std::exception &e)
  {
  }

  ASSERT_TRUE(r);
  /*
  fields of tools::wallet2::signed_tx_set to be checked:
    std::vector<pending_tx>         ptx
    std::vector<crypto::key_image>  key_images

  fields of tools::walllet2::pending_tx to be checked:
    cryptonote::transaction                       tx                  // TODO
    uint64_t                                      dust
    uint64_t                                      fee
    bool                                          dust_added_to_fee
    cryptonote::tx_destination_entry              change_dts
    std::list<size_t>                             selected_transfers
    std::string                                   key_images
    crypto::secret_key                            tx_key
    std::vector<cryptonote::tx_destination_entry> dests
    tx_construction_data                          construction_data
  */
  // ptx
  ASSERT_TRUE(exported_txs.ptx.size() == 1);
  auto& ptx = exported_txs.ptx[0];

  // ptx.{dust, fee, dust_added_to_fee}
  ASSERT_TRUE (ptx.dust == 0);
  ASSERT_TRUE (ptx.fee == 112000000000);
  ASSERT_FALSE(ptx.dust_added_to_fee);
  // ptx.change.{amount, addr}

  ASSERT_TRUE(ptx.change_dts.amount == 7784000000000);
  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, ptx.change_dts.addr) == ADDRESS1);

  // ptx.selected_transfers
  ASSERT_TRUE(ptx.selected_transfers.size() == 2);
  ASSERT_TRUE(ptx.selected_transfers.front() == 0);
  // ptx.{key_images, tx_key}
  ASSERT_TRUE(ptx.key_images == std::string("<f03885a4220f3df8b12f4798d9d67932457bfb90dc6099293455f6da94f0cfa1> <f8b8af82c1be1a10d3900bbcbf318ae9388e5111f655a3bcab98852731d231cf> "));
  ASSERT_TRUE(epee::string_tools::pod_to_hex(ptx.tx_key) == "0100000000000000000000000000000000000000000000000000000000000000");

  // ptx.dests
  ASSERT_TRUE(ptx.dests.size() == 1);

  ASSERT_TRUE(ptx.dests[0].amount == 1000000000000);
  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, ptx.dests[0].addr) == ADDRESS2);
  // ptx.construction_data
  auto& tcd = ptx.construction_data;
  ASSERT_TRUE(tcd.sources.size() == 2);
  auto& tse = tcd.sources[0];

  // ptx.construction_data.sources[0].outputs
  ASSERT_TRUE(tse.outputs.size() == 5);
  auto& out0 = tse.outputs[0];
  auto& out1 = tse.outputs[1];
  auto& out2 = tse.outputs[2];
  auto& out3 = tse.outputs[3];
  auto& out4 = tse.outputs[4];
  ASSERT_TRUE(out0.first == 34);
  ASSERT_TRUE(out1.first == 79);
  ASSERT_TRUE(out2.first == 81);
  ASSERT_TRUE(out3.first == 97);
  ASSERT_TRUE(out4.first == 116);
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out0.second) == "bd79f6db4ed2d93cc000b841ec935b606162c8285dac368b09e3e06db6776057690c312586bbdf123d9e34ad7955e1c2ae5259cd3effd0b08b19cb556d65ec25");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out1.second) == "3f920b629e61b0666730961ad846e32289ac43bc72fc1339001119bca857c4f4be8aa4ab10aab1cd1920b83243ebdfb84c2275dc0eaeee8b7e202c3d1f314c92");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out2.second) == "764c2dbecec111f58645924a3d3c327ddd13b81fa089b5a61b43e6d102840aa7f06c06d7981152fa81cee8520c271b2f3bf3b4c913f3dc6d887bd7fcb30e299c");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out3.second) == "4a89575fddc7ba0134c11e4583910ab62947fdbf3924d81cd5662b9020216e2a3e979ba58c2c25010e4168a3e9f704aaa0a10316128093819ebd22d930955df0");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(out4.second) == "66e61044129c9f3de919dad3e035d57be1923a1818465f0cb6ce155a2dc86f1cde69d7c1c2558e4fdfe015a579e748603b74361a224d19bf7d089f817649387b");
  // ptx.construction_data.sources[0].{real_output, real_out_tx_key, real_output_in_tx_index, amount, rct, mask}
  ASSERT_TRUE(tse.real_output == 4);
  ASSERT_TRUE(epee::string_tools::pod_to_hex(tse.real_out_tx_key) == "5e68e9272cc31399ae309e28ed4b83426fcc8f7bc98fda358b6b15d024b9842d");
  ASSERT_TRUE(tse.real_output_in_tx_index == 0);
  ASSERT_TRUE(tse.amount == 5000000000000);
  ASSERT_TRUE(tse.rct);
  ASSERT_TRUE(epee::string_tools::pod_to_hex(tse.mask) == "796309c7e57439028f111714bd04c8bbe22167bd2f7c04c21dc99b0c16478003");
  // ptx.construction_data.change_dts

  ASSERT_TRUE(tcd.change_dts.amount == 7784000000000);
  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, tcd.change_dts.addr) == ADDRESS1);
  // ptx.construction_data.splitted_dsts
  ASSERT_TRUE(tcd.splitted_dsts.size() == 2);
  auto& splitted_dst0 = tcd.splitted_dsts[0];
  auto& splitted_dst1 = tcd.splitted_dsts[1];

  ASSERT_TRUE(splitted_dst0.amount == 1000000000000);
  ASSERT_TRUE(splitted_dst1.amount == 7784000000000);
  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, splitted_dst0.addr) == ADDRESS2);
  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, splitted_dst1.addr) == ADDRESS1);

  // ptx.construction_data.selected_transfers
  ASSERT_TRUE(tcd.selected_transfers.size() == 2);
  ASSERT_TRUE(tcd.selected_transfers.front() == 0);
  // ptx.construction_data.extra
  ASSERT_TRUE(tcd.extra.size() == 33);
  // ptx.construction_data.{unlock_time, use_rct}
  ASSERT_TRUE(tcd.unlock_time == 0);
  // ASSERT_TRUE(tcd.use_rct);

  // ptx.construction_data.dests
  ASSERT_TRUE(tcd.dests.size() == 1);
  auto& dest = tcd.dests[0];

  ASSERT_TRUE(dest.amount == 1000000000000);
  ASSERT_TRUE(cryptonote::get_account_address_as_str(nettype, false, dest.addr) == ADDRESS2);

  // key_images
  ASSERT_TRUE(exported_txs.key_images.size() == 2);
  auto& ki0 = exported_txs.key_images[0];
  auto& ki1 = exported_txs.key_images[1];
  ASSERT_TRUE(epee::string_tools::pod_to_hex(ki0) == "f03885a4220f3df8b12f4798d9d67932457bfb90dc6099293455f6da94f0cfa1");
  ASSERT_TRUE(epee::string_tools::pod_to_hex(ki1) == "f8b8af82c1be1a10d3900bbcbf318ae9388e5111f655a3bcab98852731d231cf");

}
#endif
