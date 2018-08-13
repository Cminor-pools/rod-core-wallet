// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <pow.h>
#include <powdata.h>
#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>

#include <assert.h>

#include <chainparamsseeds.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>

namespace
{

constexpr const char pszTimestampTestnet[] = "Decentralised Autonomous Worlds";
constexpr const char pszTimestampMainnet[]
    = "HUC #2,351,800: "
      "8730ea650d24cd01692a5adb943e7b8720b0ba8a4c64ffcdf5a95d9b3fb57b7f";

/* Premined amount is 222,222,222 CHI.  This is the maximum possible number of
   coins needed in case everything is sold in the ICO.  If this is not the case
   and we need to reduce the coin supply, excessive coins will be burnt by
   sending to an unspendable OP_RETURN output.  */
constexpr CAmount premineAmount = 222222222 * COIN;

/*
The premine on regtest is sent to a 1-of-2 multisig address.

The two addresses and corresponding privkeys are:
  cRH94YMZVk4MnRwPqRVebkLWerCPJDrXGN:
    b69iyynFSWcU54LqXisbbqZ8uTJ7Dawk3V3yhht6ykxgttqMQFjb
  ceREF8QnXPsJ2iVQ1M4emggoXiXEynm59D:
    b3fgAKVQpMj24gbuh6DiXVwCCjCbo1cWiZC2fXgWEU9nXy6sdxD5

This results in the multisig address: dHNvNaqcD7XPDnoRjAoyfcMpHRi5upJD7p
Redeem script:
  512103c278d06b977e67b8ea45ef24e3c96a9258c47bc4cce3d0b497b690d672497b6e21
  0221ac9dc97fe12a98374344d08b458a9c2c1df9afb29dd6089b94a3b4dc9ad57052ae

The constant below is the HASH160 of the dedeem script.  In other words, the
final premine script will be:
  OP_HASH160 hexPremineAddress OP_EQUAL
*/
constexpr const char hexPremineAddressRegtest[]
    = "2b6defe41aa3aa47795b702c893c73e716d485ab";

/*
The premine on testnet and mainnet is sent to a 2-of-4 multisig address.  The
keys are held by the founding members of the Xaya team.

The address is:
  DHy2615XKevE23LVRVZVxGeqxadRGyiFW4

The hash of the redeem script is the constant below.  With it, the final
premine script is:
  OP_HASH160 hexPremineAddress OP_EQUAL
*/
constexpr const char hexPremineAddressMainnet[]
    = "8cb1c236d34c74221fe4163bbba739b52e95f484";

CBlock CreateGenesisBlock(const CScript& genesisInputScript, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = genesisInputScript;
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = 0;
    genesis.nNonce   = 0;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);

    std::unique_ptr<CPureBlockHeader> fakeHeader(new CPureBlockHeader ());
    fakeHeader->nNonce = nNonce;
    fakeHeader->hashMerkleRoot = genesis.GetHash ();
    genesis.pow.setCoreAlgo (PowAlgo::NEOSCRYPT);
    genesis.pow.setBits (nBits);
    genesis.pow.setFakeHeader (std::move (fakeHeader));

    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 */
CBlock
CreateGenesisBlock (const uint32_t nTime, const uint32_t nNonce,
                    const uint32_t nBits,
                    const std::string& timestamp,
                    const uint160& premineP2sh)
{
  const std::vector<unsigned char> timestampData(timestamp.begin (),
                                                 timestamp.end ());
  const CScript genesisInput = CScript () << timestampData;

  std::vector<unsigned char>
    scriptHash (premineP2sh.begin (), premineP2sh.end ());
  std::reverse (scriptHash.begin (), scriptHash.end ());
  const CScript genesisOutput = CScript ()
    << OP_HASH160 << scriptHash << OP_EQUAL;

  const int32_t nVersion = 1;
  return CreateGenesisBlock (genesisInput, genesisOutput, nTime, nNonce, nBits,
                             nVersion, premineAmount);
}

/**
 * Mines the genesis block (by finding a suitable nonce only).  When done, it
 * prints the found nonce and block hash and exits.
 */
void MineGenesisBlock (CBlock& block, const Consensus::Params& consensus)
{
  std::cout << "Mining genesis block..." << std::endl;

  block.nTime = GetTime ();

  auto& fakeHeader = block.pow.initFakeHeader (block);
  while (!block.pow.checkProofOfWork (fakeHeader, consensus))
    {
      assert (fakeHeader.nNonce < std::numeric_limits<uint32_t>::max ());
      ++fakeHeader.nNonce;
      if (fakeHeader.nNonce % 1000 == 0)
        std::cout << "  nNonce = " << fakeHeader.nNonce << "..." << std::endl;
    }

  std::cout << "Found nonce: " << fakeHeader.nNonce << std::endl;
  std::cout << "nTime: " << block.nTime << std::endl;
  std::cout << "Block hash: " << block.GetHash ().GetHex () << std::endl;
  std::cout << "Merkle root: " << block.hashMerkleRoot.GetHex () << std::endl;
  exit (EXIT_SUCCESS);
}

} // anonymous namespace

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

void CChainParams::TurnOffSegwitForUnitTests ()
{
  consensus.BIP16Height = 1000000;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 4200000;
        consensus.initialSubsidy = 1 * COIN;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 1;
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.powLimitNeoscrypt = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        /* The target spacing is independent for each mining algorithm, so that
           the effective block frequency is half the value (with two algos).  */
        consensus.nPowTargetSpacing = 2 * 30;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // CSV (BIP68, BIP112 and BIP113) as well as segwit (BIP141, BIP143 and
        // BIP147) are deployed together with P2SH.

        // The best chain should have at least this much work.
        // The value is the chain work of the Namecoin mainnet chain at height
        // 60,000, with best block hash:
        // de8764d0a6af200416c709b9e652b62bffafe0bc2dfe921821d64e5b0fa827b9
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000237ea631ca515c459e211");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xde8764d0a6af200416c709b9e652b62bffafe0bc2dfe921821d64e5b0fa827b9"); //60000

        consensus.nAuxpowChainId = 1829;

        consensus.rules.reset(new Consensus::MainNetConsensus());

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xcc;
        pchMessageStart[1] = 0xbe;
        pchMessageStart[2] = 0xb4;
        pchMessageStart[3] = 0xfe;
        nDefaultPort = 8394;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock (1531470713, 482087, 0x1e0ffff0,
                                      pszTimestampMainnet,
                                      uint160S (hexPremineAddressMainnet));
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("e5062d76e5f50c42f493826ac9920b63a8def2626fd70a5cec707ec47a4c4651"));
        assert(genesis.hashMerkleRoot == uint256S("0827901b75ab43978c3cf20a78baf040faeb0e2eeff3a2c58ab6521a6d46f8fd"));

        vSeeds.emplace_back("seed.xaya.io");
        vSeeds.emplace_back("seed.xaya.domob.eu");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,28);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,30);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,130);
        /* FIXME: Update these below.  */
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "chi";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                {     0, uint256S("ce46f5f898b38e9c8c5e9ae4047ef5bccc42ec8eca0142202813a625e6dc2656")},
            }
        };

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 4096 de8764d0a6af200416c709b9e652b62bffafe0bc2dfe921821d64e5b0fa827b9
            /* nTime    */ 1533365986,
            /* nTxCount */ 65597,
            /* dTxRate  */ 0.035
        };

        /* disable fallback fee on mainnet */
        m_fallback_fee_enabled = false;
    }

    int DefaultCheckNameDB () const
    {
        return -1;
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 4200000;
        consensus.initialSubsidy = 1 * COIN;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 1;
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.powLimitNeoscrypt = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 2 * 30;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // CSV (BIP68, BIP112 and BIP113) as well as segwit (BIP141, BIP143 and
        // BIP147) are deployed together with P2SH.

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        consensus.nAuxpowChainId = 1829;

        consensus.rules.reset(new Consensus::TestNetConsensus());

        pchMessageStart[0] = 0xcc;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xfe;
        nDefaultPort = 18394;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock (1530623291, 343829, 0x1e0ffff0,
                                      pszTimestampTestnet,
                                      uint160S (hexPremineAddressMainnet));
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("5195fc01d0e23d70d1f929f21ec55f47e1c6ea1e66fae98ee44cbbc994509bba"));
        assert(genesis.hashMerkleRoot == uint256S("59d1a23342282179e810dff9238a97d07bd8602e3a1ba0efb5f519008541f257"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("seed.testnet.xaya.io");
        vSeeds.emplace_back("seed.testnet.xaya.domob.eu");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,88);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,90);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,230);
        /* FIXME: Update these below.  */
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "chitn";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {     0, uint256S("3bcc29e821e7fbd374c7460306eb893725d69dbee87c4774cdcd618059b6a578")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        /* enable fallback fee on testnet */
        m_fallback_fee_enabled = true;
    }

    int DefaultCheckNameDB () const
    {
        return -1;
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        // The subsidy for regtest net is kept same as upstream Bitcoin, so
        // that we don't have to update many of the tests unnecessarily.
        consensus.initialSubsidy = 50 * COIN;
        consensus.BIP16Height = 432; // Corresponds to activation height using BIP9 rules
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimitNeoscrypt = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 2 * 30;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        consensus.nAuxpowChainId = 1829;

        consensus.rules.reset(new Consensus::RegTestConsensus());

        pchMessageStart[0] = 0xcc;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18495;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock (1300000000, 0, 0x207fffff,
                                      pszTimestampTestnet,
                                      uint160S (hexPremineAddressRegtest));
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("6f750b36d22f1dc3d0a6e483af45301022646dfc3b3ba2187865f5a7d6d83ab1"));
        assert(genesis.hashMerkleRoot == uint256S("9f96a4c275320aaf6386652444be5baade11e2f9f40221a98b968ae5c32dd55a"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("18042820e8a9f538e77e93c500768e5be76720383cd17e9b419916d8f356c619")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,88);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,90);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,230);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "chirt";

        /* enable fallback fee on regtest */
        m_fallback_fee_enabled = true;
    }

    int DefaultCheckNameDB () const
    {
        return 0;
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}

void TurnOffSegwitForUnitTests ()
{
  globalChainParams->TurnOffSegwitForUnitTests ();
}
