#pragma once

#include <cstdint>

#include "cryptonote_config.h"

namespace config
{

namespace graft
{

constexpr uint8_t STAKE_TRANSACTION_PROCESSING_DB_VERSION = 13;

constexpr uint64_t STAKE_MIN_UNLOCK_TIME = 10;
constexpr uint64_t STAKE_MAX_UNLOCK_TIME_V15 = 5000;
constexpr uint64_t STAKE_MAX_UNLOCK_TIME = 23040;
constexpr uint64_t STAKE_MIN_UNLOCK_TIME_FOR_WALLET = 60;
constexpr uint64_t STAKE_VALIDATION_PERIOD = 6;
constexpr uint64_t TRUSTED_RESTAKING_PERIOD = 6;
constexpr uint64_t SUPERNODE_HISTORY_SIZE = 100;

//  50,000 GRFT –  tier 1
//  90,000 GRFT –  tier 2
//  150,000 GRFT – tier 3
//  250,000 GRFT – tier 4
constexpr uint64_t TIER1_STAKE_AMOUNT = COIN *  50000;
constexpr uint64_t TIER2_STAKE_AMOUNT = COIN *  90000;
constexpr uint64_t TIER3_STAKE_AMOUNT = COIN * 150000;
constexpr uint64_t TIER4_STAKE_AMOUNT = COIN * 250000;

constexpr size_t TIERS_COUNT = 4;

constexpr size_t    CHECKPOINT_SAMPLE_SIZE =  8;
constexpr uint64_t  CHECKPOINT_NUM_CHECKPOINTS_FOR_CHAIN_FINALITY = 2;  // Number of consecutive checkpoints before, blocks preceeding the N checkpoints are locked in
constexpr uint64_t  CHECKPOINT_INTERVAL                           = 4;  // Checkpoint every 4 blocks and prune when too old except if (height % CHECKPOINT_STORE_PERSISTENTLY_INTERVAL == 0)
constexpr uint64_t  CHECKPOINT_STORE_PERSISTENTLY_INTERVAL        = 60; // Persistently store the checkpoints at these intervals
constexpr uint64_t  CHECKPOINT_VOTE_LIFETIME                      = CHECKPOINT_STORE_PERSISTENTLY_INTERVAL; // Keep the last 60 blocks worth of votes
constexpr size_t    CHECKPOINT_QUORUM_SIZE                 = 8;
constexpr size_t    CHECKPOINT_MIN_VOTES                   = 5;
constexpr size_t    CHECKPOINT_NUM_BLOCKS_FOR_HASH         = 10;

constexpr int16_t CHECKPOINT_NUM_QUORUMS_TO_PARTICIPATE_IN = 8;
constexpr int16_t CHECKPOINT_MAX_MISSABLE_VOTES            = 4;
static_assert(CHECKPOINT_MAX_MISSABLE_VOTES < CHECKPOINT_NUM_QUORUMS_TO_PARTICIPATE_IN,
              "The maximum number of votes a service node can miss cannot be greater than the amount of checkpoint "
              "quorums they must participate in before we check if they should be deregistered or not.");
static_assert(CHECKPOINT_MIN_VOTES <= CHECKPOINT_QUORUM_SIZE, "The number of votes required to add a checkpoint can't exceed the actual quorum size, otherwise we never add checkpoints.");
constexpr uint64_t VOTE_LIFETIME                           = BLOCKS_EXPECTED_IN_HOURS(2);

// NOTE: We can reorg up to last 2 checkpoints + the number of extra blocks before the next checkpoint is set
constexpr uint64_t  REORG_SAFETY_BUFFER_BLOCKS_POST_HF18 = (CHECKPOINT_INTERVAL * CHECKPOINT_NUM_CHECKPOINTS_FOR_CHAIN_FINALITY) + (CHECKPOINT_INTERVAL - 1);
constexpr uint64_t  REORG_SAFETY_BUFFER_BLOCKS_PRE_HF18  = 20;
static_assert(REORG_SAFETY_BUFFER_BLOCKS_POST_HF18 < VOTE_LIFETIME, "Safety buffer should always be less than the vote lifetime");
static_assert(REORG_SAFETY_BUFFER_BLOCKS_PRE_HF18  < VOTE_LIFETIME, "Safety buffer should always be less than the vote lifetime");

// TODO: applicable for "only-checkpointing" ?
constexpr uint64_t VOTE_OR_TX_VERIFY_HEIGHT_BUFFER    = 5;


}

}
