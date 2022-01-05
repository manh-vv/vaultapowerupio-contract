#include <eospowerupio.hpp>

void eospowerupio::sub_balance(const name& owner, const asset& value) {
  account_table account_t(get_self(), owner.value);
  account_table::const_iterator account_itr = account_t.require_find(value.symbol.code().raw(), "unable to find owner balance");
  const asset owner_bal = account_itr->balance;
  check(value <= owner_bal, "cost of: " + value.to_string() + " exceeds owner balance of: " + owner_bal.to_string());

  const asset new_balance = owner_bal - value;
  if(new_balance.amount == 0) account_t.erase(account_itr);
  else {
    account_t.modify(account_itr, same_payer, [&](account_row& row) {
      row.balance = new_balance;
    });
  }
}

void eospowerupio::add_balance(const name& owner, const asset& value, const name& ram_payer, const bool& enforce_max) {
  account_table account_t(get_self(), owner.value);
  account_table::const_iterator account_itr = account_t.find(value.symbol.code().raw());

  tknwhitelist_table tknwhitelist_t(get_self(), get_self().value);
  tknwhitelist_row whitelisted = *tknwhitelist_t.require_find(value.symbol.code().raw(), "token not whitelisted");


  if(account_itr == account_t.end()) {
    // check(false,)
    // if(enforce_max) check(value.amount >= 10000,"must deposit at least 1 EOS ");
    account_t.emplace(ram_payer, [&](account_row& row) {
      row.balance = value;
    });
  } else {
    account_t.modify(account_itr, same_payer, [&](account_row& row) {
      row.balance += value;
    });
  }

  if(owner == get_self()) return;
  if(!enforce_max) return;

  // make sure the balance does not exceed the maximum.
  asset user_bal = account_t.get(value.symbol.code().raw()).balance;
  check(user_bal <= whitelisted.max_deposit,"user balance exceeds the maximum balance: "+ whitelisted.max_deposit.to_string() );
}

void eospowerupio::check_tknwhitelist(symbol sym, name token_contract) {
  tknwhitelist_table tknwhitelist_t(get_self(), get_self().value);
  const char* invalid_token_err = "deposited token is not whitelisted";
  tknwhitelist_row whitelisted = *tknwhitelist_t.require_find(sym.code().raw(), invalid_token_err);
  check(whitelisted.contract == token_contract, invalid_token_err);
};

void eospowerupio::check_open(const name contract, const name account, const symbol_code symcode) {
  eosio::token::accounts _accounts(contract, account.value);
  auto itr = _accounts.find(symcode.raw());
  check(itr != _accounts.end(), get_self().to_string() + ": " + account.to_string() + " account must have " + symcode.to_string() + " `open` balance");
}

void eospowerupio::save_state( const state new_state) {
  // can only be created once (to prevent double entry attacks)
  state_table _state(get_self(), get_self().value);
  check(!_state.exists(), get_self().to_string() + ": balance already exists, must now use `checkbalance`");

  // save contract balance
  _state.set(new_state, get_self());
}
