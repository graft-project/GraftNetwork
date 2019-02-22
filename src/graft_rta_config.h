#pragma once

#include <cstdint>

#include "cryptonote_config.h"

namespace config
{

namespace graft
{

constexpr uint64_t STAKE_MIN_UNLOCK_TIME = 50;
constexpr uint64_t STAKE_MAX_UNLOCK_TIME = 1000;
constexpr uint64_t STAKE_VALIDATION_PERIOD = 6;
constexpr uint64_t TRUSTED_RESTAKING_PERIOD = 6;
//constexpr uint64_t TRUSTED_RESTAKING_PERIOD = 10000;

//  50,000 GRFT –  tier 1
//  90,000 GRFT –  tier 2
//  150,000 GRFT – tier 3
//  250,000 GRFT – tier 4
constexpr uint64_t TIER1_STAKE_AMOUNT = COIN *  50000;
constexpr uint64_t TIER2_STAKE_AMOUNT = COIN *  90000;
constexpr uint64_t TIER3_STAKE_AMOUNT = COIN * 150000;
constexpr uint64_t TIER4_STAKE_AMOUNT = COIN * 250000;

}

}
