const conf = require('../eosioConfig')
const env = require('../.env.js')
const { api, tapos } = require('./lib/eosjs')(env.keys[env.defaultChain], conf.endpoints[env.defaultChain][1])
const contractAccount = conf.accountName[env.defaultChain]
var watchAccountSample = require('./lib/sample_watchaccount')
function chainName() {
  if (env.defaultChain == 'jungle') return 'jungle3'
  else return env.defaultChain
}

async function doAction(name, data, account, auth) {
  try {
    if (!data) data = {}
    if (!account) account = contractAccount
    if (!auth) auth = account
    console.log("Do Action:", name, data)
    const authorization = [{ actor: auth, permission: 'active' }]
    const result = await api.transact({
      // "delay_sec": 0,
      actions: [{ account, name, data, authorization }]
    }, tapos)
    const txid = result.transaction_id
    console.log(`https://${chainName()}.bloks.io/transaction/` + txid)
    // console.log(txid)
    return result
  } catch (error) {
    console.error(error.toString())
    if (error.json) console.error("Logs:", error.json?.error?.details[1]?.message)
  }
}

const methods = {
  async whitelisttkn(contract, max_deposit) {
    await doAction('whitelisttkn', { tknwhitelist: { contract, max_deposit } })
  },
  async dopowerup(payer, receiver, net_frac, cpu_frac, max_payment) {
    if (!cpu_frac) cpu_frac = 1000000000
    else cpu_frac = parseInt(cpu_frac)
    if (!net_frac) net_frac = 900
    else net_frac = parseInt(net_frac)
    if (!max_payment) max_payment = "1.0000 EOS"

    await doAction('dopowerup', { payer, receiver, cpu_frac, net_frac, max_payment })
  },
  async autopowerup(payer, watch_account, net_frac, cpu_frac, max_payment) {
    if (!cpu_frac) cpu_frac = 1000000000
    else cpu_frac = parseInt(cpu_frac)
    if (!net_frac) net_frac = 900
    else net_frac = parseInt(net_frac)
    if (!max_payment) max_payment = "1.0000 EOS"

    await doAction('autopowerup', { payer, watch_account, cpu_frac, net_frac, max_payment })
  },
  async setconfig(fee_pct, freeze, per_action_fee, minimum_fee, memo) {
    if (freeze == "true") freeze = true
    else freeze = false
    await doAction('setconfig', { cfg: { fee_pct: parseFloat(fee_pct), freeze, minimum_fee, per_action_fee, memo } })
  },
  async clrwhitelist() {
    await doAction('clrwhitelist')
  },
  async withdraw(owner, quantity, receiver) {
    if (!receiver) receiver = owner
    await doAction('withdraw', { owner, quantity, receiver })
  },
  async watchaccount(owner, name) {
    if (name) watchAccountSample.account = name
    await doAction('watchaccount', { owner, watch_data: watchAccountSample }, contractAccount, owner)
  },
  async open(owner, contract, sym, ram_payer) {
    if (!ram_payer) ram_payer = owner
    const extsymbol = { contract, sym }
    await doAction('open', { owner, extsymbol, ram_payer }, contractAccount, ram_payer)
  },
  async dobuyram(payer, receiver, bytes) {
    await doAction('dobuyram', { payer, receiver, bytes }, contractAccount, payer)
  },
  async clearconfig() {
    await doAction('clearconfig')
  },
  async sxrebalance(maintain_bal) {
    await doAction('sxrebalance', { maintain_bal })
  },
  async updatememo(memo) {
    await doAction('updatememo', { memo })
  },
}


if (require.main == module) {
  if (Object.keys(methods).find(el => el === process.argv[2])) {
    console.log("Starting:", process.argv[2])
    methods[process.argv[2]](...process.argv.slice(3)).catch((error) => console.error(error))
      .then((result) => console.log('Finished'))
  } else {
    console.log("Available Commands:")
    console.log(JSON.stringify(Object.keys(methods), null, 2))
  }
}
module.exports = methods
