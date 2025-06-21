#pragma once

#if __INTELLISENSE__
#pragma diag_suppress 2486
#endif
#include <algorithm>
#include <common.hpp>
#include <eosio.token.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <optional>
// #include <eosio/transaction.hpp>
// #include <eosio/crypto.hpp>
#define SECONDS_WEEK 604800

using namespace eosio;
using namespace std;
CONTRACT eospowerupio: public contract {
 public:
  using contract::contract;
  // Structs

  TABLE account_row {
    asset balance;
    uint64_t primary_key() const { return balance.symbol.code().raw(); }
  };
  using account_table = multi_index<"account"_n, account_row>;

  TABLE watchlist_row {
    name account;
    uint16_t min_cpu_ms;
    uint16_t powerup_quantity_ms;
    uint16_t min_kb_ram;
    uint16_t buy_ram_quantity_kb;
    bool active;
    uint64_t primary_key() const { return account.value; }
  };
  using watchlist_table = multi_index<"watchlist"_n, watchlist_row>;

  TABLE tknwhitelist_row {
    name contract;
    asset max_deposit;
    symbol get_symbol() const { return max_deposit.symbol; }
    uint64_t primary_key() const { return get_symbol().code().raw(); }
  };
  using tknwhitelist_table = multi_index<"tknwhitelist"_n, tknwhitelist_row>;

  TABLE referrals_row {
    name payer;
    name referrer;
    uint64_t primary_key() const { return payer.value; };
    uint64_t by_referrer() const { return referrer.value; };
  };
  using referrals_table = multi_index<"referrals"_n, referrals_row, indexed_by<"byreferrer"_n, const_mem_fun<referrals_row, uint64_t, &referrals_row::by_referrer>>>;

  TABLE referralfees_row {
    name referrer;
    asset unclaimed_fees;
    uint64_t primary_key() const { return referrer.value; };
  };
  using referralfees_table = multi_index<"referralfees"_n, referralfees_row>;

  TABLE state {
    name contract;
    asset balance;
    name receiver;
    name action;
    float received_cpu_ms = 0;
    float received_net_kb = 0;
    float received_ram_kb = 0;
    string memo = "";
  };
  typedef singleton<"state"_n, state> state_table;

  TABLE config {
    float fee_pct;
    bool freeze;
    asset per_action_fee;
    asset minimum_fee;
    string memo;
  };
  typedef singleton<"config"_n, config> config_table;

  TABLE staked_row {  // scope is user account name
    uint32_t template_id;
    uint64_t asset_id;
    eosio::time_point_sec locked_until;
    uint64_t primary_key() const { return (uint64_t)template_id; }
  };
  typedef multi_index<"staked"_n, staked_row> staked_table;

  // TABLE invited_row {
  //   uint64_t primary_key() const { return (uint64_t)template_id; }
  // }

  // Actions
  ACTION withdraw(const name owner, const asset quantity, const name receiver);
  ACTION whitelisttkn(const tknwhitelist_row tknwhitelist);
  ACTION dopowerup(const name payer, const name receiver, const uint64_t net_frac, const uint64_t cpu_frac, const asset max_payment);
  ACTION billaccount(const name owner, const name contract, const symbol_code symcode);
  ACTION setconfig(const config cfg);
  ACTION watchaccount(const name owner, const watchlist_row watch_data);
  ACTION rmwatchaccnt(const name owner, const name watch_account);
  ACTION autopowerup(const name payer, const name watch_account, const uint64_t net_frac, const uint64_t cpu_frac, const asset max_payment);
  ACTION open(const name& owner, const extended_symbol& extsymbol, const name& ram_payer);
  ACTION dobuyram(const name payer, const name receiver, const uint32_t bytes);
  ACTION autobuyram(const name payer, const name watch_account);
  ACTION logpowerup(string message, name action, name payer, name receiver, asset cost, asset fee, asset total_billed, float received_cpu_ms, float received_net_kb);
  ACTION logbuyram(string message, name action, name payer, name receiver, asset cost, asset fee, asset total_billed, float received_ram_kb);
  ACTION clearconfig();
  ACTION updatememo(string memo);
  ACTION setreferrer(name account, name referrer);
  ACTION claimreffees(name referrer);

  using logpowerup_action = action_wrapper<"logpowerup"_n, &eospowerupio::logpowerup>;
  using logbuyram_action = action_wrapper<"logbuyram"_n, &eospowerupio::logbuyram>;

  // Functions
  [[eosio::on_notify("*::transfer")]] void deposit(const name from, const name to, const asset quantity, const std::string memo);
  void check_tknwhitelist(symbol sym, name token_contract);

  float cpu_frac_to_ms(float cpu_frac) {
    return cpu_frac / float(29411000);
  }
  float net_frac_to_kb(float net_frac) {
    return net_frac / float(5500);
  }
#if defined(DEBUG)
  ACTION clrwhitelist();

  template <typename T>
  void cleanTable(name code, uint64_t account, const uint32_t batchSize) {
    T db(code, account);
    uint32_t counter = 0;
    auto itr = db.begin();
    while(itr != db.end() && counter++ < batchSize) {
      itr = db.erase(itr);
    }
  }
#endif

 private:
  uint32_t bronze_template_id = 3648;
  uint32_t silver_template_id = 3649;
  uint32_t gold_template_id = 3650;
  name nft_contract = name("powerup.nfts");
  void check_open(const name contract, const name account, const symbol_code symcode);
  void save_state(const state new_state);
  void sub_balance(const name& owner, const asset& value);
  void add_referralfees(const name& owner, const asset& value);
  void sub_referralfees(const name& owner, const asset& value);
  void add_balance(const name& owner, const asset& value, const name& ram_payer, const bool& enforce_max);
};
