const conf = require("../eosioConfig")
const env = require("../.env.js")

async function doAction(name, data, account, auth, chain = "jungle") {
  const { api, tapos } = require("./lib/eosjs")(env.keys[chain], conf.endpoints[chain][0])
  const contractAccount = conf.accountName[chain]

  try {
    if (!data) data = {}
    if (!account) account = contractAccount
    if (!auth) auth = account
    console.log("Do Action:", name, data)
    const authorization = [{ actor: auth, permission: "active" }]
    const result = await api.transact(
      {
        // "delay_sec": 0,
        actions: [{ account, name, data, authorization }],
      },
      tapos
    )
    const txid = result.transaction_id
    console.log(`${conf.explorers[chain]}/transaction/${txid}`)
    // console.log(txid)
    return result
  } catch (error) {
    console.error(error.toString())
    if (error.json) console.error("Logs:", error.json?.error?.details[1]?.message)
  }
}

const methods = {
  async whitelisttkn(contract, max_deposit, chain = "jungle") {
    await doAction("whitelisttkn", { tknwhitelist: { contract, max_deposit } }, null, null, chain)
  },
}

if (require.main == module) {
  if (Object.keys(methods).find((el) => el === process.argv[2])) {
    console.log("Starting:", process.argv[2])
    methods[process.argv[2]](...process.argv.slice(3))
      .catch((error) => console.error(error))
      .then((result) => console.log("Finished"))
  } else {
    console.log("Available Commands:")
    console.log(JSON.stringify(Object.keys(methods), null, 2))
  }
}

module.exports = methods
