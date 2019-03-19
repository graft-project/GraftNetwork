
#pragma once

#include "common/util.h"
#include "crypto/chacha.h"

#include <string>

/////////////////////////////////////////////////////////////////////////////////////////
namespace dbg
{
/////////////////////////////////////////////////////////////////////////////////////////
inline std::string dump_as_hex_bytes(const crypto::chacha_key& key)
{
  return tools::dump_as_hex_bytes(&key[0], CHACHA_KEY_SIZE);
}
/////////////////////////////////////////////////////////////////////////////////////////
inline std::string dump_as_hex_bytes(const crypto::chacha_iv& iv)
{
  return tools::dump_as_hex_bytes(&iv.data[0], CHACHA_IV_SIZE);
}

inline std::string dump_as_hex_bytes(const crypto::secret_key& key)
{
  return tools::dump_as_hex_bytes((const u8*)(&key), sizeof(key));
}
/////////////////////////////////////////////////////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////////////////////////









/////////////////////////////////////////////////////////////////////////////////////////
namespace dbg {

class CodecCounter
{
  public:
    CodecCounter(const crypto::secret_key& key, const std::string& lbl);
    ~CodecCounter(void);
    CodecCounter(const CodecCounter&);
    CodecCounter& operator=(const CodecCounter&);

    CodecCounter(CodecCounter&&) = delete;
    CodecCounter& operator=(CodecCounter&&) = delete;
    //CodecCounter(CodecCounter&&) noexcept = delete;

    void on_call_enter(const crypto::secret_key& key);
    void on_call_exit(const crypto::secret_key& key);

  private:
    std::string lbl_;
    u32 call_cnt_;
    std::string prev_val_;
    const crypto::secret_key* key_;
};

};
/////////////////////////////////////////////////////////////////////////////////////////







/////////////////////////////////////////////////////////////////////////////////////////
namespace dbg
{
/////////////////////////////////////////////////////////////////////////////////////////
inline std::string mk_hint_ptr_pair(const char* hint, const void* ptr)
{
  constexpr u32 buf_len = 128;
  char b[buf_len];
  memset(b, 0, buf_len);
  sprintf(b, "%s %p", hint, ptr);
  return std::string(b);
}
/////////////////////////////////////////////////////////////////////////////////////////
inline void change_report(const std::string& old_val, const std::string& new_val,
  std::ostringstream& m)
{
  m << std::endl << "old [" << old_val << "]"
    << std::endl << "new [" << new_val << "]";
};
/////////////////////////////////////////////////////////////////////////////////////////
inline CodecCounter::CodecCounter(const crypto::secret_key& key, const std::string& lbl)
: lbl_(lbl)
, call_cnt_(0)
, key_(&key)
{
  MDEBUG(mk_hint_ptr_pair("cc-ctor", this) << mk_hint_ptr_pair("  key", key_));
}
/////////////////////////////////////////////////////////////////////////////////////////
inline CodecCounter::~CodecCounter(void)
{
  MDEBUG(mk_hint_ptr_pair("cc-dtor", this) << mk_hint_ptr_pair("  key", key_));
}
/////////////////////////////////////////////////////////////////////////////////////////
inline CodecCounter::CodecCounter(const CodecCounter& o)
{
  lbl_ = "not-assigned";
  call_cnt_ = 0;
  key_ = nullptr;

  MDEBUG("CodecCounter::CodecCounter(const CodecCounter& o)"
    << mk_hint_ptr_pair("  o.key", o.key_));

  std::ostringstream m;
  m << "WARN - !!! - CodecCounter::copy-constructor is called";
  m << std::endl << "lbl [" << lbl_ << "] vs [" << o.lbl_ << "]"
    << std::endl << "cnt [" << call_cnt_ << " ] vs [" << o.call_cnt_ << "]"
    << std::endl << "prv [" << prev_val_ << "] vs [" << o.prev_val_ << "]"
    << std::endl << "key [" << dbg::dump_as_hex_bytes(*key_) << "] vs [" << dbg::dump_as_hex_bytes(*o.key_) << "]"
    << std::endl << mk_hint_ptr_pair("cc-op= this", this)
    << mk_hint_ptr_pair("  key", key_) << mk_hint_ptr_pair("  o.key", o.key_);

  if(o.key_->data[0])
    m << "   key-ptr replaced with o's";
  else
    m << "   key-ptr kept the same, no changes!";

  MDEBUG(m.str());

  lbl_ = o.lbl_;
  call_cnt_ = o.call_cnt_;
  prev_val_ = o.prev_val_;
  if(o.key_->data[0])
    key_ = o.key_;
}
/////////////////////////////////////////////////////////////////////////////////////////
inline CodecCounter& CodecCounter::operator=(const CodecCounter& o)
{
  MDEBUG("CodecCounter& CodecCounter::operator=(const CodecCounter& o)"
    << mk_hint_ptr_pair("  o.key", o.key_));

  //MDEBUG("CodecCounter::operator= is called !!!");
  //return *this = CodecCounter(o);

  std::ostringstream m;
  m << std::endl << "lbl [" << lbl_ << "] vs [" << o.lbl_ << "]"
    << std::endl << "cnt [" << call_cnt_ << " ] vs [" << o.call_cnt_ << "]"
    << std::endl << "prv [" << prev_val_ << "] vs [" << o.prev_val_ << "]"
    << std::endl << "key [" << dbg::dump_as_hex_bytes(*key_) << "] vs [" << dbg::dump_as_hex_bytes(*o.key_) << "]"
    << std::endl << mk_hint_ptr_pair("cc-op= this", this)
    << mk_hint_ptr_pair("  key", key_) << mk_hint_ptr_pair("  o.key", o.key_);

  if(o.key_->data[0])
    m << "   key-ptr replaced with o's";
  else
    m << "   key-ptr kept the same, no changes!";

  MDEBUG(m.str());

  lbl_ = o.lbl_;
  call_cnt_ = o.call_cnt_;
  prev_val_ = o.prev_val_;
  if(o.key_->data[0])
    key_ = o.key_;

  return *this;
}
/////////////////////////////////////////////////////////////////////////////////////////
inline void CodecCounter::on_call_enter(const crypto::secret_key& key)
{
  {
    std::ostringstream m;
    m << "enter " << mk_hint_ptr_pair(lbl_.c_str(), this);
    if(key_ != &key)
    {
      m << "  ext-key changed, FIX it and go ...";
      key_ = &key;
    }

    MDEBUG(m.str());
  }

  //MDEBUG(mk_hint_ptr_pair("enter cc", this) << mk_hint_ptr_pair("  key", key_)
  //  << mk_hint_ptr_pair("  ext-key", &key));

  //for(u32 i = 0, cnt = sizeof(key); i < cnt; ++i)
  //  if(key_->data[i] != key.data[i])
  //  {
  //    std::ostringstream m;
  //    m << "FAIL on call-on-enter - content is different"
  //      << std::endl << "ext [" << dbg::dump_as_hex_bytes(key) << "]"
  //      << std::endl << "int [" << dbg::dump_as_hex_bytes(*key_) << "]";
  //    MDEBUG(m.str());
  //    break;
  //  }

  const std::string kas = dbg::dump_as_hex_bytes(*key_); // kas = key as string
  const bool changed = (kas != prev_val_);
  if(changed)
  {
    std::ostringstream m;
    m << "WARN  " << lbl_ << "  enter:" << call_cnt_ << "  key changed";
    change_report(prev_val_, kas, m);
    MDEBUG(m.str());
  }
  prev_val_ = kas;
}
/////////////////////////////////////////////////////////////////////////////////////////
inline void CodecCounter::on_call_exit(const crypto::secret_key& key)
{
  if(key_ != &key)
    MDEBUG("FAIL FAIL FAIL on (key_ != &key) at CodecCounter::on_call_exit");

  //for(u32 i = 0, cnt = sizeof(key); i < cnt; ++i)
  //  if(key_->data[i] != key.data[i])
  //  {
  //    std::ostringstream m;
  //    m << "FAIL on call-on-exit - content is different"
  //      << std::endl << "ext [" << dbg::dump_as_hex_bytes(key) << "]"
  //      << std::endl << "int [" << dbg::dump_as_hex_bytes(*key_) << "]";
  //    MDEBUG(m.str());
  //    break;
  //  }

  const std::string kas = dbg::dump_as_hex_bytes(*key_); // kas = key as string
  const bool changed = (kas != prev_val_);

  std::ostringstream m;
  if(!changed)
  {
    m << "WARN  " << lbl_ << "  exit - key NOT changed";
    change_report(prev_val_, kas, m);
    m << std::endl;
  }

  m << lbl_ << "  exit:" << call_cnt_ << "  key-state ["
    << ((call_cnt_ % 2) ? "HIDDEN to OPEN" : "OPEN to HIDDEN") << "]"
    << mk_hint_ptr_pair("  cc", this)
    << mk_hint_ptr_pair("  key", key_)
    << mk_hint_ptr_pair("  ext-key", &key);

  change_report(prev_val_, kas, m);
  MDEBUG(m.str());

  prev_val_ = kas;
  ++call_cnt_;
}
/////////////////////////////////////////////////////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////////////////////////








//namespace {
//namespace dbg
//{
//
//std::string dump_as_hex_bytes(const crypto::chacha_key& key)
//{
//  return tools::dump_as_hex_bytes(&key[0], CHACHA_KEY_SIZE);
//}
//
//std::string dump_as_hex_bytes(const crypto::chacha_iv& iv)
//{
//  return tools::dump_as_hex_bytes(&iv.data[0], CHACHA_IV_SIZE);
//}
//
//std::string dump_as_hex_bytes(const crypto::secret_key& key)
//{
//  return tools::dump_as_hex_bytes((const u8*)(&key), sizeof(key));
//}
//
//}
//}
