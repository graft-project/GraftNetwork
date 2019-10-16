// Copyright (c) 2019, The Loki Project
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

#include "quorumnet.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/service_node_voting.h"
#include "cryptonote_core/service_node_rules.h"
#include "cryptonote_core/tx_blink.h"
#include "quorumnet/sn_network.h"
#include "quorumnet/conn_matrix.h"
#include "cryptonote_config.h"

#undef LOKI_DEFAULT_LOG_CATEGORY
#define LOKI_DEFAULT_LOG_CATEGORY "qnet"

namespace {

using namespace quorumnet;
using namespace service_nodes;

struct SNNWrapper {
    SNNetwork snn;
    cryptonote::core &core; // FIXME - may not be needed?  Can we get everything needed via sn_list?
    service_node_list &sn_list;

    std::mutex blinks_mutex;
    // { height => { txhash => blink_tx, ... }, ... }
    std::map<uint64_t, std::unordered_map<crypto::hash, std::shared_ptr<cryptonote::blink_tx>>> blinks;

    template <typename... Args>
    SNNWrapper(cryptonote::core &core, service_node_list &sn_list, Args &&...args) :
        snn{std::forward<Args>(args)...}, core{core}, sn_list{sn_list} {}
};

template <typename T>
std::string key_data_as_string(const T &key) {
    return {reinterpret_cast<const char *>(key.data), sizeof(key.data)};
}

crypto::x25519_public_key x25519_from_string(const std::string &pubkey) {
    crypto::x25519_public_key x25519_pub = crypto::x25519_public_key::null();
    if (pubkey.size() == sizeof(crypto::x25519_public_key))
        std::memcpy(x25519_pub.data, pubkey.data(), pubkey.size());
    return x25519_pub;
}

std::string get_connect_string(const service_node_list &sn_list, crypto::x25519_public_key x25519_pub) {
    if (!x25519_pub) {
        MDEBUG("no connection available: pubkey is empty");
        return "";
    }
    auto pubkey = sn_list.get_pubkey_from_x25519(x25519_pub);
    if (!pubkey) {
        MDEBUG("no connection available: could not find primary pubkey from x25519 pubkey " << x25519_pub);
        return "";
    }
    auto states = sn_list.get_service_node_list_state({{pubkey}});
    if (states.empty()) {
        MDEBUG("no connection available: primary pubkey " << pubkey << " is not registered");
        return "";
    }
    const auto &proof = *states[0].info->proof;
    if (!proof.public_ip || !proof.quorumnet_port) {
        MDEBUG("no connection available: primary pubkey " << pubkey << " has no associated ip and/or port");
        return "";
    }
    return "tcp://" + epee::string_tools::get_ip_string_from_int32(proof.public_ip) + ":" + std::to_string(proof.quorumnet_port);
}

constexpr el::Level easylogging_level(LogLevel level) {
    switch (level) {
        case LogLevel::fatal: return el::Level::Fatal;
        case LogLevel::error: return el::Level::Error;
        case LogLevel::warn:  return el::Level::Warning;
        case LogLevel::info:  return el::Level::Info;
        case LogLevel::debug: return el::Level::Debug;
        case LogLevel::trace: return el::Level::Trace;
    };
    return el::Level::Unknown;
};
bool snn_want_log(LogLevel level) {
    return ELPP->vRegistry()->allowed(easylogging_level(level), LOKI_DEFAULT_LOG_CATEGORY);
}
void snn_write_log(LogLevel level, const char *file, int line, std::string msg) {
    el::base::Writer(easylogging_level(level), file, line, ELPP_FUNC, el::base::DispatchAction::NormalLog).construct(LOKI_DEFAULT_LOG_CATEGORY) << msg;
}

void *new_snnwrapper(cryptonote::core &core, service_node_list &sn_list, const std::string &bind) {
    auto keys = core.get_service_node_keys();
    assert((bool) keys);
    MINFO("Starting quorumnet listener on " << bind << " with x25519 pubkey " << keys->pub_x25519);
    auto *obj = new SNNWrapper(core, sn_list,
            key_data_as_string(keys->pub_x25519),
            key_data_as_string(keys->key_x25519),
            std::vector<std::string>{{bind}},
            [&sn_list](const std::string &x25519_pub) { return get_connect_string(sn_list, x25519_from_string(x25519_pub)); },
            [&sn_list](const std::string &ip, const std::string &x25519_pubkey_str) {
                // TODO: this function could also check to see whether the given pubkey *should* be
                // contacting us (i.e. either will soon be or was recently in a shared quorum).
                return true;
                return (bool) sn_list.get_pubkey_from_x25519(x25519_from_string(x25519_pubkey_str));
            },
            snn_want_log, snn_write_log);

    obj->snn.data = obj; // Provide pointer to the instance for callbacks

    return obj;
}

void delete_snnwrapper(void *obj) {
    auto *snn = reinterpret_cast<SNNWrapper *>(obj);
    MINFO("Shutting down quorumnet listener");
    delete snn;
}


template <typename E>
E get_enum(const bt_dict &d, const std::string &key) {
    E result = static_cast<E>(get_int<std::underlying_type_t<E>>(d.at(key)));
    if (result < E::_count)
        return result;
    throw std::invalid_argument("invalid enum value for field " + key);
}



bt_dict serialize_vote(const quorum_vote_t &vote) {
    bt_dict result{
        {"v", vote.version},
        {"t", static_cast<uint8_t>(vote.type)},
        {"h", vote.block_height},
        {"g", static_cast<uint8_t>(vote.group)},
        {"i", vote.index_in_group},
        {"s", std::string{reinterpret_cast<const char *>(&vote.signature), sizeof(vote.signature)}},
    };
    if (vote.type == quorum_type::checkpointing)
        result["bh"] = std::string{vote.checkpoint.block_hash.data, sizeof(crypto::hash)};
    else {
        result["wi"] = vote.state_change.worker_index;
        result["sc"] = static_cast<std::underlying_type_t<new_state>>(vote.state_change.state);
    }
    return result;
}

quorum_vote_t deserialize_vote(const bt_value &v) {
    const auto &d = boost::get<bt_dict>(v); // throws if not a bt_dict
    quorum_vote_t vote;
    vote.version = get_int<uint8_t>(d.at("v"));
    vote.type = get_enum<quorum_type>(d, "t");
    vote.block_height = get_int<uint64_t>(d.at("h"));
    vote.group = get_enum<quorum_group>(d, "g");
    if (vote.group == quorum_group::invalid) throw std::invalid_argument("invalid vote group");
    vote.index_in_group = get_int<uint16_t>(d.at("i"));
    auto &sig = boost::get<std::string>(d.at("s"));
    if (sig.size() != sizeof(vote.signature)) throw std::invalid_argument("invalid vote signature size");
    std::memcpy(&vote.signature, sig.data(), sizeof(vote.signature));
    if (vote.type == quorum_type::checkpointing) {
        auto &bh = boost::get<std::string>(d.at("bh"));
        if (bh.size() != sizeof(vote.checkpoint.block_hash.data)) throw std::invalid_argument("invalid vote checkpoint block hash");
        std::memcpy(vote.checkpoint.block_hash.data, bh.data(), sizeof(vote.checkpoint.block_hash.data));
    } else {
        vote.state_change.worker_index = get_int<uint16_t>(d.at("wi"));
        get_enum<new_state>(d, "sc");
    }

    return vote;
}

/// Returns primary_pubkey => {x25519_pubkey, connect_string} pairs for the given primary pubkeys
template <typename It>
static auto get_zmq_remotes(SNNWrapper &snw, It begin, It end) {
    std::unordered_map<crypto::public_key, std::pair<crypto::x25519_public_key, std::string>> remotes;
    snw.sn_list.for_each_service_node_info(begin, end,
        [&remotes](const crypto::public_key &pubkey, const service_nodes::service_node_info &info) {
            if (!info.is_active()) return;
            auto &proof = *info.proof;
            if (!proof.pubkey_x25519 || !proof.quorumnet_port || !proof.public_ip) return;
            remotes.emplace(pubkey,
                std::make_pair(proof.pubkey_x25519, "tcp://" + epee::string_tools::get_ip_string_from_int32(proof.public_ip) + ":" + std::to_string(proof.quorumnet_port)));
        });
    return remotes;
}

static void relay_votes(void *obj, const std::vector<service_nodes::quorum_vote_t> &votes) {
    auto &snw = *reinterpret_cast<SNNWrapper *>(obj);

    auto my_keys_ptr = snw.core.get_service_node_keys();
    assert(my_keys_ptr);
    const auto &my_keys = *my_keys_ptr;

    // Loop twice: the first time we build up the set of remotes we need for all votes; then we look
    // up their proofs -- which requires a potentially expensive lock -- to get the x25519 pubkey
    // and port from the proof; then we do the sending in the second loop.
    std::unordered_set<crypto::public_key> need_remotes;
    std::vector<std::tuple<int, const service_nodes::quorum_vote_t *, const std::vector<crypto::public_key> *>> valid_votes;
    valid_votes.reserve(votes.size());
    for (auto &v : votes) {
        auto quorum = snw.sn_list.get_quorum(v.type, v.block_height);

        if (!quorum) {
            MWARNING("Unable to relay vote: no testing quorum vote for type " << v.type << " @ height " << v.block_height);
            continue;
        }

        auto &quorum_voters = quorum->validators;
        if (quorum_voters.size() < service_nodes::min_votes_for_quorum_type(v.type)) {
            MWARNING("Invalid vote relay: " << v.type << " quorum @ height " << v.block_height <<
                    " does not have enough validators (" << quorum_voters.size() << ") to reach the minimum required votes ("
                    << service_nodes::min_votes_for_quorum_type(v.type) << ")");
        }

        int my_pos = service_nodes::find_index_in_quorum_group(quorum_voters, my_keys.pub);
        if (my_pos < 0) {
            MWARNING("Invalid vote relay: vote to relay does not include this service node");
            MTRACE("me: " << my_keys.pub);
            for (const auto &v : quorum->validators)
                MTRACE("validator: " << v);
            for (const auto &v : quorum->workers)
                MTRACE("worker: " << v);
            continue;
        }

        for (int i : quorum_outgoing_conns(my_pos, quorum_voters.size()))
            need_remotes.insert(quorum_voters[i]);

        valid_votes.emplace_back(my_pos, &v, &quorum_voters);
    }

    MDEBUG("Relaying " << valid_votes.size() << " votes");
    if (valid_votes.empty())
        return;

    // pubkey => {x25519_pubkey, connect_string}
    auto remotes = get_zmq_remotes(snw, need_remotes.begin(), need_remotes.end());

    for (auto &vote_data : valid_votes) {
        int my_pos = std::get<0>(vote_data);
        auto &vote = *std::get<1>(vote_data);
        auto &quorum_voters = *std::get<2>(vote_data);

        bt_dict vote_to_send = serialize_vote(vote);

        for (int i : quorum_outgoing_conns(my_pos, quorum_voters.size())) {
            auto it = remotes.find(quorum_voters[i]);
            if (it == remotes.end()) {
                MINFO("Unable to relay vote to peer " << quorum_voters[i] << ": peer is inactive or we are missing a x25519 pubkey and/or quorumnet port");
                continue;
            }

            auto &remote_info = it->second;
            std::string x25519_pubkey{reinterpret_cast<const char *>(remote_info.first.data), sizeof(crypto::x25519_public_key)};
            MDEBUG("Relaying vote to peer " << remote_info.first << " @ " << remote_info.second);
            snw.snn.send(x25519_pubkey, "vote", vote_to_send, send_option::hint{remote_info.second});
        }
    }
}

void handle_vote(SNNetwork::message &m, void *self) {
    auto &snw = *reinterpret_cast<SNNWrapper *>(self);

    MDEBUG("Received a relayed vote from " << as_hex(m.pubkey));

    if (m.data.size() != 1) {
        MINFO("Ignoring vote: expected 1 data part, not " << m.data.size());
        return;
    }

    try {
        quorum_vote_t vote = deserialize_vote(m.data[0]);

        if (vote.block_height > snw.core.get_current_blockchain_height()) {
            MDEBUG("Ignoring vote: block height " << vote.block_height << " is too high");
            return;
        }

        cryptonote::vote_verification_context vvc = {};
        snw.core.add_service_node_vote(vote, vvc);
        if (vvc.m_verification_failed)
        {
            MWARNING("Vote verification failed; ignoring vote");
            return;
        }

        if (vvc.m_added_to_pool)
            relay_votes(self, {{vote}});
    }
    catch (const std::exception &e) {
        MWARNING("Deserialization of vote from " << as_hex(m.pubkey) << " failed: " << e.what());
    }
}

/// Gets an integer value out of a bt_dict, if present and fits (i.e. get_int succeeds); if not
/// present or conversion falls, returns `fallback`.
template <typename I>
static std::enable_if_t<std::is_integral<I>::value, I> get_or(bt_dict &d, const std::string &key, I fallback) {
    auto it = d.find(key);
    if (it != d.end()) {
        try { return get_int<I>(it->second); }
        catch (...) {}
    }
    return fallback;
}

/// A "blink" message is used to submit a blink tx from a node to members of the blink quorum and
/// also used to relay the blink tx between quorum members.  Fields are:
///
///     "!" - Non-zero positive integer value for a connecting node; we include the tag in any
///           response if present so that the initiator can associate the response to the request.
///           If there is no tag then there will be no success/error response.  Only included in
///           node-to-SN submission but not SN-to-SN relaying (which doesn't return a response
///           message).
///
///     "h" - Blink authorization height for the transaction.  Must be within 2 of the current
///           height for the tx to be accepted.  Mandatory.
///
///     "q" - checksum of blink quorum members.  Mandatory, and must match the receiving SN's
///           locally computed checksum of blink quorum members.
///
///     "t" - the serialized transaction data.
///
///     "#" - optional precomputed tx hash.  This is included in SN-to-SN relays to allow faster
///           ignoring of already-seen transactions.  It is an error if this is included and does
///           not match the actual hash of the transaction.
///
void handle_blink(SNNetwork::message &m, void *self) {
    auto &snw = *reinterpret_cast<SNNWrapper *>(self);

    // TODO: if someone sends an invalid tx (i.e. one that doesn't get to the distribution stage)
    // then put a timeout on that IP during which new submissions from them are dropped for a short
    // time.
    // If an incoming connection:
    // - We can refuse new connections from that IP in the ZAP handler
    // - We can (somewhat hackily) disconnect by getting the raw fd via the SRCFD property of the
    //   message and close it.
    // If an outgoing connection - refuse reconnections via ZAP and just close it.

    bool from_sn = m.from_sn();
    MDEBUG("Received a blink tx from " << (from_sn ? as_hex(m.pubkey) : "an anonymous node"));

    if (m.data.size() != 1) {
        MINFO("Rejecting blink message: expected one data entry not " << m.data.size());
        // FIXME: send error response
        return;
    }
    auto &data = boost::get<bt_dict>(m.data[0]);

    auto tag = get_or<uint64_t>(data, "!", 0);

    // verify that height is within-2 of current height
    auto blink_height = get_int<uint64_t>(data.at("h"));

    auto local_height = snw.core.get_current_blockchain_height();

    if (blink_height < local_height - 2) {
        MINFO("Rejecting blink tx because blink auth height is too low (" << blink_height << " vs. " << local_height << ")");
        // FIXME: send error response
        return;
    } else if (blink_height > local_height + 2) {
        // TODO: if within some threshold (maybe 5-10?) we could hold it and process it once we are
        // within 2.
        MINFO("Rejecting blink tx because blink auth height is too high (" << blink_height << " vs. " << local_height << ")");
        // FIXME: send error response
        return;
    }

    uint64_t q_base_height = cryptonote::blink_tx::quorum_height(blink_height, cryptonote::blink_tx::subquorum::base);
    if (q_base_height == 0) {
        MINFO("Rejecting blink tx: too early in chain to construct a blink quorum");
        // FIXME: send error response
        return;
    }

    auto t_it = data.find("t");
    if (t_it == data.end()) {
        MINFO("Rejecting blink tx: no tx data included in request");
        // FIXME: send error response
        return;
    }
    const std::string &tx_data = boost::get<std::string>(t_it->second);

    // "hash" is optional -- it lets us short-circuit processing the tx if we've already seen it,
    // and is added internally by SN-to-SN forwards but not the original submitter.  We don't trust
    // the hash if we haven't seen it before -- this is only used to skip propagation and
    // validation.
    boost::optional<crypto::hash> hint_tx_hash;
    if (data.count("hash")) {
        auto &tx_hash_str = boost::get<std::string>(data.at("hash"));
        if (tx_hash_str.size() == sizeof(crypto::hash)) {
            crypto::hash tx_hash;
            std::memcpy(tx_hash.data, tx_hash_str.data(), sizeof(crypto::hash));
            std::lock_guard<std::mutex> lock{snw.blinks_mutex};
            auto &umap = snw.blinks[blink_height];
            auto it = umap.find(tx_hash);
            if (it != umap.end()) {
                MDEBUG("Already seen and forwarded this blink tx, ignoring it.");
                return;
            }
            hint_tx_hash = std::move(tx_hash);
        } else {
            MINFO("Rejecting blink tx: invalid tx hash included in request");
            // FIXME: send error response
            return;
        }
    }

    auto btxptr = std::make_shared<cryptonote::blink_tx>(blink_height);
    auto &btx = *btxptr;

    // We currently just use two quorums, Q and Q' in the whitepaper, but this code is designed to
    // work fine with more quorums (but don't use a single subquorum; that could only be secure or
    // reliable but not both).
    constexpr auto NUM_QUORUMS = tools::enum_count<cryptonote::blink_tx::subquorum>;
    std::array<std::shared_ptr<const quorum>, NUM_QUORUMS> blink_quorums = {};

    uint64_t local_checksum = 0;
    for (uint8_t qi = 0; qi < NUM_QUORUMS; qi++) {
        auto height = btx.quorum_height(static_cast<cryptonote::blink_tx::subquorum>(qi));
        blink_quorums[qi] = snw.sn_list.get_quorum(quorum_type::blink, height);

        local_checksum += quorum_checksum(blink_quorums[qi]->validators, qi * BLINK_SUBQUORUM_SIZE);
    }

    if (!std::all_of(blink_quorums.begin(), blink_quorums.end(),
                [](const auto &quo) { auto v = quo->validators.size(); return v >= BLINK_MIN_VOTES && v <= BLINK_SUBQUORUM_SIZE; })) {
        MINFO("Rejecting blink tx: not enough blink nodes to form a quorum");
        // FIXME: send error response
        return;
    }

    auto input_checksum = get_int<uint64_t>(data.at("q"));
    if (input_checksum != local_checksum) {
        MINFO("Rejecting blink tx: wrong quorum checksum");
        // FIXME: send error response
        return;
    }

    auto keys = snw.core.get_service_node_keys();
    const auto &my_pubkey = keys->pub;

    std::array<int, NUM_QUORUMS> mypos;
    mypos.fill(-1);

    for (size_t qi = 0; qi < blink_quorums.size(); qi++) {
        auto &v = blink_quorums[qi]->validators;
        for (size_t pki = 0; pki < v.size(); pki++) {
            if (v[pki] == my_pubkey) {
                mypos[qi] = pki;
                break;
            }
        }
    }

    if (std::none_of(mypos.begin(), mypos.end(), [](auto pos) { return pos >= 0; })) {
        MINFO("Rejecting blink tx: this service node is not a member of the blink quorum!");
        // FIXME: send error response
        return;
    }

    crypto::hash tx_hash;
    if (!cryptonote::parse_and_validate_tx_from_blob(tx_data, btx.tx(), tx_hash)) {
        MINFO("Rejecting blink tx: failed to parse transaction data");
        // FIXME: send error response
        return;
    }

    if (hint_tx_hash && tx_hash != *hint_tx_hash) {
        MINFO("Rejecting blink tx: hint tx hash did not match actual tx hash");
        // FIXME: send error response
        return;
    }

    // See if we've already handled this blink tx.  We do this even if we already checked the hint
    // hash to avoid a race condition between there are here that could result in two nearly
    // simultaneous blink causing the blink to be forwarded multiple times.
    {
        std::lock_guard<std::mutex> lock(snw.blinks_mutex);
        auto &umap = snw.blinks[blink_height];
        auto it = umap.find(tx_hash);
        if (it != umap.end()) {
            MDEBUG("Already seen and forwarded this blink tx, ignoring it.");
            return;
        }

    }


    // TODO: Reply here to say we've accepted it for verification

    // The submission looks good.  We distribute it first, *before* we start verifying the actual tx
    // details, for two reasons: we want other quorum members to start verifying ASAP, and we want
    // to propagate to peers even if the things below fail on this node (because our peers might
    // succeed).  We test the bits *above*, however, because if they fail we won't agree on the
    // right quorum to send it to.
    //
    // FIXME - am I 100% sure I want to do the above?  Verifying the TX would cut off being able to
    // induce a node to broadcast a junk TX to other quorum members.

    std::unordered_set<crypto::public_key> need_remotes;
    for (auto &q : blink_quorums)
        for (auto &pubkey : q->validators)
            need_remotes.insert(pubkey);

    // pubkey => {x25519_pubkey, connect_string}
    auto remotes = get_zmq_remotes(snw, need_remotes.begin(), need_remotes.end());

    bt_dict relay_data{
        {"h", blink_height},
        {"q", local_checksum},
        {"tx", tx_data},
        {"hash", std::string{tx_hash.data, sizeof(tx_hash.data)}},
    };
    auto relay_blink = [&remotes, &snw, &relay_data](const auto &pubkey, bool optional = false) {
        auto it = remotes.find(pubkey);
        if (it == remotes.end()) {
            MINFO("Unable to relay blink tx to service node " << pubkey << ": service node is inactive or has not sent a x25519 pubkey, ip, and/or quorumnet port");
            return;
        }

        auto &remote_info = it->second;
        std::string x25519_pubkey{reinterpret_cast<const char *>(remote_info.first.data), sizeof(crypto::x25519_public_key)};
        MDEBUG("Relaying blink tx to peer " << remote_info.first << " @ " << remote_info.second);
        if (optional)
            snw.snn.send(x25519_pubkey, "blink", relay_data, send_option::optional{});
        else
            snw.snn.send(x25519_pubkey, "blink", relay_data, send_option::hint{remote_info.second});
    };

    for (size_t i = 0; i < mypos.size(); i++) {
        if (mypos[i] < 0)
            continue;

        // TODO: when we receive a new block, if our quorum starts soon we can tell SNNetwork to
        // pre-connect (to save the time in handshaking when we get an actual blink tx).

        auto &quorum = *blink_quorums[i];

        // Relay to all my outgoing targets within the quorum (connecting if not already connected)
        for (int j : quorum_outgoing_conns(mypos[i], quorum.validators.size())) {
            MTRACE("Intra-quorum blink relay to " << quorum.validators[j]);
            relay_blink(quorum.validators[j]);
        }

        // Relay to all my *incoming* sources within the quorum *if* I already have a connection
        // open with them, but don't open a new connection if I don't.
        for (int j : quorum_incoming_conns(mypos[i], quorum.validators.size())) {
            MTRACE("Intra-quorum optional blink relay to " << quorum.validators[j]);
            relay_blink(quorum.validators[j], true /*optional*/);
        }

        // If I'm in the last half* of the first quorum then I relay to the first half (roughly) of
        // the next quorum.  i.e. nodes 5-9 in Q send to nodes 0-4 in Q'.  For odd numbers the last
        // position gets left out (e.g. for 9 members total we would have 0-3 talk to 4-7 and no one
        // talks to 8).
        //
        // (* - half here means half the size of the smaller quorum)
        //
        // We also skip this entirely if this SN is in both quorums since then we're already
        // relaying to nodes in the next quorum.  (Ideally we'd do the same if the recipient is in
        // both quorums, but that's harder to figure out and so the special case isn't worth
        // worrying about).
        if (i + 1 < mypos.size() && mypos[i + 1] < 0) {
            auto &next_quorum = *blink_quorums[i + 1];
            int half = std::min<int>(quorum.validators.size(), next_quorum.validators.size()) / 2;
            if (mypos[i] >= half && mypos[i] < half*2) {
                MTRACE("Inter-quorum relay from Q to Q' service node " << next_quorum.validators[mypos[i] - half]);
                relay_blink(next_quorum.validators[mypos[i] - half]);
            }
        }

        // Exactly the same connections as above, but in reverse: the first half of Q' sends to the
        // second half of Q.  Typically this will end up reusing an already open connection, but if
        // there isn't such an open connection then we establish a new one.  (We could end up with
        // one each way, but that won't hurt anything).
        if (i > 0 && mypos[i - 1] < 0) {
            auto &prev_quorum = *blink_quorums[i - 1];
            int half = std::min<int>(quorum.validators.size(), prev_quorum.validators.size()) / 2;
            if (mypos[i] < half) {
                MTRACE("Inter-quorum relay from Q' to Q service node " << prev_quorum.validators[mypos[i] + half]);
                relay_blink(prev_quorum.validators[mypos[i] + half]);
            }
        }

        // Note: don't break here: it's possible for us to land in both quorums, which is fine (and likely on testnet)
    }

    // Lock key images.
}

} // empty namespace


namespace quorumnet {

/// Sets the cryptonote::quorumnet_* function pointers (allowing core to avoid linking to
/// cryptonote_protocol).  Called from daemon/daemon.cpp.  Also registers quorum command callbacks.
void init_core_callbacks() {
    cryptonote::quorumnet_new = new_snnwrapper;
    cryptonote::quorumnet_delete = delete_snnwrapper;
    cryptonote::quorumnet_relay_votes = relay_votes;

    // Receives a vote
    SNNetwork::register_quorum_command("vote", handle_vote);

    // Receives a new blink tx submission from an external node, or forward from other quorum
    // members who received it from an external node.
    SNNetwork::register_public_command("blink", handle_blink);

    // Sends a message back to the blink initiator that the transaction was accepted for relaying.
    // This is only sent by the entry point service nodes into the quorum to let it know the tx
    // looks good enough to pass to other quorum members, but it does *not* indicate approval.
//    SNNetwork::register_quorum_command("bl_start", handle_blink_started);

    // Sends a message back to the blink initiator that the transaction was NOT relayed, either
    // because the height was invalid or the quorum checksum failed.  This is only sent by the entry
    // point service nodes into the quorum to let it know the tx verification has not started from
    // that node.  It does not necessarily indicate a failure unless all entry point attempts return
    // the same.
//    SNNetwork::register_quorum_command("bl_nostart", handle_blink_not_started);

    // Sends a message from the entry SNs back to the initiator that the Blink tx has been rejected:
    // that is, enough signing rejections have occured that the Blink TX cannot proceed.
//    SNNetwork::register_quorum_command("bl_bad", handle_blink_failure);

    // Sends a message from the entry SNs back to the initiator that the Blink tx has been accepted
    // and is being broadcast to the network.
//    SNNetwork::register_quorum_command("bl_good", handle_blink_success);

    // Receives blink tx signatures or rejections between quorum members (either original or
    // forwarded).  These are propagated by the receiver if new
//    SNNetwork::register_quorum_command("blink_sign", handle_blink_signature);
}

}
