const { setAbiAction, setCodeAction } = require("./lib/encodeContractData")
const conf = require("../eosioConfig")
const env = require("../.env.js")
const ms = require("ms")
const sleep = (ms) => new Promise((res) => setTimeout(res, ms))

const methodHelper = async (chain, type) => {
  const { api, tapos } = require("./lib/eosjs")(env.keys[chain], conf.endpoints[chain][0])

  const authorization = [{ actor: conf.accountName[chain], permission: "active" }]
  if (!type) type = "debug"

  console.log("Pushing ABI")
  const result = await api
    .transact({ actions: [setAbiAction(`../build/${type}/${conf.contractName}.abi`, authorization)] }, tapos)
    .catch((err) => console.log(err.toString()))
  if (result) console.log(`${conf.explorers[chain]}/transaction/${result.transaction_id}`)

  await sleep(ms("5s"))

  console.log("Pushing WASM")
  const result2 = await api
    .transact({ actions: [setCodeAction(`../build/${type}/${conf.contractName}.wasm`, authorization)] }, tapos)
    .catch((err) => console.log(err.toString()))
  if (result2) console.log(`${conf.explorers[chain]}/transaction/${result2.transaction_id}`)
}

const methods = {
  async kylin(type) {
    await methodHelper("kylin", type)
  },
  async jungle(type) {
    await methodHelper("jungle", type)
  },
  async eos(type) {
    await methodHelper("eos", type)
  },
}

if (require.main == module) {
  if (Object.keys(methods).find((el) => el === process.argv[2])) {
    console.log("Starting:", process.argv[2])
    methods[process.argv[2]](...process.argv.slice(3))
      .catch((error) => console.error(error.toString()))
      .then((result) => console.log("Finished"))
  } else {
    console.log("Available Commands:")
    console.log(JSON.stringify(Object.keys(methods), null, 2))
  }
}
// function sleep(arg0) {
//   throw new Error('Function not implemented.')
// }

// function ms(arg0) {
//   throw new Error('Function not implemented.')
// }
