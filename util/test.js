const conf = require('../eosioConfig')
const env = require('../.env.js')
const { doAction } = require('./do.js')
const contractAccount = conf.accountName[env.defaultChain]

const methods = {
  async setConfig(){
    const config = require('./lib/sample_config')
    const data = {cfg:config}
    await doAction('setconfig',data)
  },

  async deposit(quantity,from,){
    const data = {
      from,
      to: contractAccount,
      quantity,
      memo:"deposit"
    }
    let contract = 'eosio.token'
    // if (quantity.split(' ')[1] == 'BOID') contract = 'token.boid'
    await doAction('transfer', data, contract, from)
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
