#include <eospowerupio.hpp>

void eospowerupio::deposit(const name from, const name to, const asset quantity, const std::string memo) {
  require_auth(from);
  if(to != get_self()) return;
  if(from == name("vaults.sx")) return;
  if(from == name("flash.sx")) return;
  if(from == name("eospwrupdefi")) return;
  if(from == name("eosio") || from == name("eosio.rex")) return;

  check(memo == "donation" || memo == "deposit", "Specify memo as 'donation' to donate or 'deposit' to fund your internal contract balance.");

  config_table config_t(get_self(), get_self().value);
  check(!config_t.get().freeze, "contract is currently frozen, try again later.");

  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "must transfer positive quantity");
  check(memo.size() <= 256, "memo has more than 256 bytes");

  const name token_contract = get_first_receiver();
  check_tknwhitelist(quantity.symbol, token_contract);

  if(memo == "donation") {
    add_balance(get_self(), quantity, get_self(), false);
  } else {
    add_balance(from, quantity, get_self(), true);
  }
};

ACTION eospowerupio::withdraw(const name owner, const asset quantity, const name receiver) {
  require_auth(get_self());
  // check(has_auth(get_self()) || has_auth(owner), "missing authentication of owner");
  sub_balance(owner, quantity);

  tknwhitelist_table tknwhitelist_t(get_self(), get_self().value);
  tknwhitelist_row whitelisted = *tknwhitelist_t.require_find(quantity.symbol.code().raw(), "withdraw token is not whitelisted");

  token::transfer_action withdraw(whitelisted.contract, {get_self(), "active"_n});
  withdraw.send(get_self(), owner, quantity, "withdraw");
}

ACTION eospowerupio::whitelisttkn(const tknwhitelist_row tknwhitelist) {
  require_auth(get_self());
  tknwhitelist_table tknwhitelist_t(get_self(), get_self().value);
  check(is_account(tknwhitelist.contract), "contract account doesn't exist");
  tknwhitelist_t.emplace(get_self(), [&](tknwhitelist_row& row) { row = tknwhitelist; });
}

ACTION eospowerupio::autopowerup(const name payer, const name watch_account, const uint64_t net_frac, const uint64_t cpu_frac, const asset max_payment) {
  require_auth(get_self());
  watchlist_table watchlist_t(get_self(), payer.value);
  const eospowerupio::watchlist_row watchlist_r = watchlist_t.get(watch_account.value, "the watch account is not registered for this payer");
  check(watchlist_r.active, "watch_account is paused");

  dopowerup(payer, watch_account, net_frac, cpu_frac, max_payment);
}

ACTION eospowerupio::dopowerup(const name payer, const name receiver, const uint64_t net_frac, const uint64_t cpu_frac, const asset max_payment) {
  check(has_auth(get_self()) || has_auth(payer), "missing authentication of payer");
  const name tkn_contract = name("eosio.token");
  const symbol tkn_sym = symbol("EOS", 4);
  const asset balance = eosio::token::get_balance(tkn_contract, get_self(), tkn_sym.code());
  config_table config_t(get_self(), get_self().value);
  const config conf = config_t.get();

  // save state data, for notifications from billaccount
  state state_data;
  state_data.receiver = receiver;
  state_data.balance = balance;
  state_data.contract = tkn_contract;
  state_data.memo = conf.memo;
  state_data.received_cpu_ms = cpu_frac_to_ms(float(cpu_frac));
  state_data.received_net_kb = net_frac_to_kb(float(net_frac));
  state_data.action = name("dopowerup");
  save_state(state_data);

  // trigger inline actions
  action(permission_level {get_self(), "active"_n}, name("eosio"), name("powerup"), make_tuple(get_self(), receiver, uint32_t(1), net_frac, cpu_frac, max_payment)).send();
  action(permission_level {get_self(), "active"_n}, get_self(), name("billaccount"), make_tuple(payer, tkn_contract, tkn_sym.code())).send();
}

ACTION eospowerupio::autobuyram(const name payer, const name watch_account) {
  require_auth(get_self());
  watchlist_table watchlist_t(get_self(), payer.value);
  watchlist_row watchlist = *watchlist_t.require_find(watch_account.value, "the watch account is not registered for this payer");
  check(watchlist.buy_ram_quantity_kb > 0, "this watch account has buy_ram_quantity_kb set to zero");
  dobuyram(payer, watch_account, watchlist.buy_ram_quantity_kb * 1000);
}

ACTION eospowerupio::dobuyram(const name payer, const name receiver, const uint32_t bytes) {
  check(has_auth(get_self()) || has_auth(payer), "missing authentication of payer");
  const name tkn_contract = name("eosio.token");
  const symbol tkn_sym = symbol("EOS", 4);
  const asset balance = eosio::token::get_balance(tkn_contract, get_self(), tkn_sym.code());

  config_table config_t(get_self(), get_self().value);
  const config conf = config_t.get();

  // save state data, for notifications from billaccount
  state state_data;
  state_data.receiver = receiver;
  state_data.balance = balance;
  state_data.contract = tkn_contract;
  state_data.memo = conf.memo;
  state_data.received_ram_kb = float(bytes) / 1000;
  state_data.action = name("dobuyram");
  save_state(state_data);

  // save_balance(tkn_contract, balance);
  action(permission_level {get_self(), "active"_n}, name("eosio"), name("buyrambytes"), make_tuple(get_self(), receiver, bytes)).send();
  action(permission_level {get_self(), "active"_n}, get_self(), name("billaccount"), make_tuple(payer, tkn_contract, tkn_sym.code())).send();
}

ACTION eospowerupio::billaccount(const name owner, const name contract, const symbol_code symcode) {
  require_auth(get_self());

  state_table state_t(get_self(), get_self().value);
  check(state_t.exists(), "contract paused");
  config_table config_t(get_self(), get_self().value);
  check(!config_t.get().freeze, "contract is currently frozen, try again later.");
  const state state = state_t.get();

  const asset prev_bal = state.balance;
  const asset current_bal = eosio::token::get_balance(contract, get_self(), symcode);

  asset cost = prev_bal - current_bal;
  if(cost.amount == 0) return;
  const asset base_cost = cost;
  asset fee = asset(0, cost.symbol);

  if(owner != get_self()) {
    // calculate fee
    const config conf = config_t.get();
    check(!conf.freeze, "contract is currently frozen, try again later.");
    float fee_pct = conf.fee_pct;
    asset static_fee = conf.per_action_fee;
    // asset static_fee = asset(1,symbol(symbol_code("EOS"),4));

    float fee_amt = max((float(cost.amount) * fee_pct / 100) + static_fee.amount, float(conf.minimum_fee.amount));
    // TODO If I change the payment token, I will need to modify this
    fee = asset(int64_t(fee_amt), cost.symbol);
    auto contractCut = fee;
    referrals_table referrals_t(get_self(), get_self().value);
    auto referrals_itr = referrals_t.find(owner.value);
    if(referrals_itr != referrals_t.end()) {
      auto& referrer = referrals_itr->referrer;
      staked_table staked_t(nft_contract, referrer.value);
      auto staked_itr = staked_t.find((uint64_t)silver_template_id);
      if(staked_itr != staked_t.end()) {
        fee *= (float).9;
        contractCut = fee / 2;
        add_referralfees(referrer, contractCut);
      }
    }
    cost += fee;
    add_balance(get_self(), contractCut, get_self(), false);
  }
  sub_balance(owner, cost);
  state_t.remove();

  if(state.action == name("dopowerup")) {
    logpowerup_action log_action(get_self(), {get_self(), "active"_n});
    log_action.send(state.memo, state.action, owner, state.receiver, base_cost, fee, cost, state.received_cpu_ms, state.received_net_kb);
  } else if(state.action == name("dobuyram")) {
    logbuyram_action log_action(get_self(), {get_self(), "active"_n});
    log_action.send(state.memo, state.action, owner, state.receiver, base_cost, fee, cost, state.received_ram_kb);
  }
}

ACTION eospowerupio::logpowerup(string message, name action, name payer, name receiver, asset cost, asset fee, asset total_billed, float received_cpu_ms, float received_net_kb) {
  require_auth(get_self());
  require_recipient(payer);
  require_recipient(receiver);
}

ACTION eospowerupio::logbuyram(string message, name action, name payer, name receiver, asset cost, asset fee, asset total_billed, float received_ram_kb) {
  require_auth(get_self());
  require_recipient(payer);
  require_recipient(receiver);
}

ACTION eospowerupio::setconfig(const config cfg) {
  require_auth(get_self());
  config_table config_t(get_self(), get_self().value);
  config_t.set(cfg, get_self());
}

ACTION eospowerupio::clearconfig() {
  require_auth(get_self());
  config_table config_t(get_self(), get_self().value);
  config_t.remove();
}

ACTION eospowerupio::watchaccount(const name owner, const watchlist_row watch_data) {
  require_auth(owner);

  watchlist_table watchlist_t(get_self(), owner.value);
  watchlist_table::const_iterator watchlist_itr = watchlist_t.find(watch_data.account.value);
  check(watch_data.buy_ram_quantity_kb <= 100, "100 kb is the max ram quantity setting (per purchase)");
  check(is_account(watch_data.account), "target account does not exist");
  check(watch_data.min_cpu_ms >= 5 || watch_data.min_cpu_ms == 0, "min_cpu_ms must be zero or greater than/equal to 5");
  check(watch_data.powerup_quantity_ms >= 10, "powerup_quantity_ms can't be less than 10");
  check(watch_data.powerup_quantity_ms <= 5000, "powerup_quantity_ms can't be greater than 5000");

  if(watchlist_itr == watchlist_t.end()) {
    watchlist_t.emplace(owner, [&](watchlist_row& row) {
      row = watch_data;
    });
  } else {
    watchlist_t.modify(watchlist_itr, same_payer, [&](watchlist_row& row) {
      row = watch_data;
    });
  }
}

ACTION eospowerupio::rmwatchaccnt(const name owner, const name watch_account) {
  require_auth(owner);
  watchlist_table watchlist_t(get_self(), owner.value);
  watchlist_table::const_iterator watchlist_itr = watchlist_t.require_find(watch_account.value, "can't find this watch_account under this owner");
  watchlist_t.erase(watchlist_itr);
}

ACTION eospowerupio::open(const name& owner, const extended_symbol& extsymbol, const name& ram_payer) {
  require_auth(ram_payer);
  check_tknwhitelist(extsymbol.get_symbol(), extsymbol.get_contract());
  add_balance(owner, asset(0, extsymbol.get_symbol()), ram_payer, false);
}

ACTION eospowerupio::updatememo(string memo) {
  require_auth(get_self());
  config_table config_t(get_self(), get_self().value);
  config cfg = config_t.get();
  cfg.memo = memo;
  config_t.set(cfg, get_self());
}

ACTION eospowerupio::setreferrer(name account, name referrer) {
  require_auth(account);

  account_table account_t(get_self(), get_self().value);
  account_t.require_find(account.value, "Must deposit funds into your PowerUp account first before adding a referrer.");

  staked_table staked_t(nft_contract, referrer.value);
  staked_t.require_find((uint64_t)silver_template_id, "Not a valid referrer, no silver NFT stake found.");

  referrals_table referrals_t(get_self(), get_self().value);
  auto referrals_itr = referrals_t.find(account.value);

  if(referrals_itr == referrals_t.end()) {
    referrals_t.emplace(get_self(), [&](referrals_row& row) {
      row.payer = account;
      row.referrer = referrer;
    });
  } else {
    referrals_t.modify(referrals_itr, get_self(), [&](referrals_row& row) {
      row.referrer = referrer;
    });
  }
}

ACTION eospowerupio::claimreffees(name referrer) {
  check(has_auth(referrer) || has_auth(get_self()), "not authorized");
  referralfees_table referralfees_t(get_self(), get_self().value);
  auto referralfees_itr = referralfees_t.require_find(referrer.value, "No fees claimable for this account");
  auto& quantity = referralfees_itr->unclaimed_fees;
  token::transfer_action transfer("eosio.token"_n, {get_self(), "active"_n});
  transfer.send(get_self(), referrer, quantity, "claim referral fees");
  referralfees_t.erase(referralfees_itr);
}
