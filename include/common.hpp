#pragma once
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
// #include <eosio/name.hpp>

#define DEBUG

uint32_t now_sec() {return eosio::current_time_point().sec_since_epoch();}

