// Copyright (c) 2014-2019 Daniel Kraft
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <coins.h>
#include <key_io.h>
#include <names/encoding.h>
#include <names/mempool.h>
#include <primitives/transaction.h>
#include <script/names.h>
#include <sync.h>
#include <test/test_bitcoin.h>
#include <txmempool.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

/* No space between BOOST_FIXTURE_TEST_SUITE and '(', so that extraction of
   the test-suite name works with grep as done in the Makefile.  */
BOOST_AUTO_TEST_SUITE(name_mempool_tests)

namespace
{

class NameMempoolTestSetup : public TestingSetup
{

public:

  CScript ADDR;

  const LockPoints lp;

  NameMempoolTestSetup ()
  {
    ENTER_CRITICAL_SECTION (mempool.cs);
    mempool.clear ();

    ADDR = CScript () << OP_TRUE;
  }

  ~NameMempoolTestSetup ()
  {
    LEAVE_CRITICAL_SECTION (mempool.cs);
  }

  /**
   * Returns a valtype name based on the given string.
   */
  static valtype
  Name (const std::string& str)
  {
    return DecodeName (str, NameEncoding::ASCII);
  }

  /**
   * Returns the hash bytes for a name_new.
   */
  static valtype
  NewHash (const std::string& nm, const char rand)
  {
    const valtype nameVal = Name (nm);
    const valtype randVal(20, rand);

    valtype toHash(randVal);
    toHash.insert (toHash.end (), nameVal.begin (), nameVal.end ());
    const uint160 hash = Hash160 (toHash);

    return valtype (hash.begin (), hash.end ());
  }

  /**
   * Builds a name_register script for the given name and value.
   */
  static CScript
  RegisterScript (const CScript& addr, const std::string& nm,
                  const std::string& val)
  {
    const valtype value = DecodeName (val, NameEncoding::ASCII);

    return CNameScript::buildNameRegister (addr, Name (nm), value);
  }

  /**
   * Builds a name_update script based on the given name and value.
   */
  static CScript
  UpdateScript (const CScript& addr, const std::string& nm,
                const std::string& val)
  {
    const valtype value = DecodeName (val, NameEncoding::ASCII);

    return CNameScript::buildNameUpdate (addr, Name (nm), value);
  }

  /**
   * Builds a transaction spending to a name-output script.  The transaction
   * is not valid, but it is "valid enough" for testing the name mempool
   * rules with it.
   */
  static CTransaction
  Tx (const CScript& out)
  {
    CMutableTransaction mtx;
    mtx.vout.push_back (CTxOut (COIN, out));

    return mtx;
  }

  /**
   * Builds a mempool entry for the given transaction.
   */
  CTxMemPoolEntry
  Entry (const CTransaction& tx)
  {
    return CTxMemPoolEntry (MakeTransactionRef (tx), 0, 0, 100, false, 1, lp);
  }

};

} // anonymous namespace

/* ************************************************************************** */

BOOST_FIXTURE_TEST_CASE (invalid_tx, NameMempoolTestSetup)
{
  /* Invalid transactions should not crash / assert fail the mempool check.  */

  CMutableTransaction mtx;
  mempool.checkNameOps (CTransaction (mtx));

  mtx.vout.push_back (CTxOut (COIN, RegisterScript (ADDR, "foo", "x")));
  mtx.vout.push_back (CTxOut (COIN, RegisterScript (ADDR, "bar", "y")));
  mtx.vout.push_back (CTxOut (COIN, UpdateScript (ADDR, "foo", "x")));
  mtx.vout.push_back (CTxOut (COIN, UpdateScript (ADDR, "bar", "y")));
  mempool.checkNameOps (CTransaction (mtx));
}

BOOST_FIXTURE_TEST_CASE (empty_mempool, NameMempoolTestSetup)
{
  /* While the mempool is empty (we do not add any transactions in this test),
     all should be fine without respect to conflicts among the transactions.  */

  BOOST_CHECK (!mempool.registersName (Name ("foo")));
  BOOST_CHECK (!mempool.updatesName (Name ("foo")));

  BOOST_CHECK (mempool.checkNameOps (Tx (RegisterScript (ADDR, "foo", "x"))));
  BOOST_CHECK (mempool.checkNameOps (Tx (RegisterScript (ADDR, "foo", "y"))));

  BOOST_CHECK (mempool.checkNameOps (Tx (UpdateScript (ADDR, "foo", "x"))));
  BOOST_CHECK (mempool.checkNameOps (Tx (UpdateScript (ADDR, "foo", "y"))));
}

BOOST_FIXTURE_TEST_CASE (name_register, NameMempoolTestSetup)
{
  const auto tx1 = Tx (RegisterScript (ADDR, "foo", "x"));
  const auto tx2 = Tx (RegisterScript (ADDR, "foo", "y"));

  const auto e = Entry (tx1);
  BOOST_CHECK (e.isNameRegistration () && !e.isNameUpdate ());
  BOOST_CHECK (e.getName () == Name ("foo"));

  mempool.addUnchecked (e);
  BOOST_CHECK (mempool.registersName (Name ("foo")));
  BOOST_CHECK (!mempool.updatesName (Name ("foo")));
  BOOST_CHECK (!mempool.checkNameOps (tx2));

  mempool.removeRecursive (tx1);
  BOOST_CHECK (!mempool.registersName (Name ("foo")));
  BOOST_CHECK (mempool.checkNameOps (tx1));
  BOOST_CHECK (mempool.checkNameOps (tx2));
}

BOOST_FIXTURE_TEST_CASE (name_update, NameMempoolTestSetup)
{
  const auto tx1 = Tx (UpdateScript (ADDR, "foo", "x"));
  const auto tx2 = Tx (UpdateScript (ADDR, "foo", "y"));

  const auto e = Entry (tx1);
  BOOST_CHECK (!e.isNameRegistration () && e.isNameUpdate ());
  BOOST_CHECK (e.getName () == Name ("foo"));

  mempool.addUnchecked (e);
  BOOST_CHECK (!mempool.registersName (Name ("foo")));
  BOOST_CHECK (mempool.updatesName (Name ("foo")));
  BOOST_CHECK (!mempool.checkNameOps (tx2));

  mempool.removeRecursive (tx1);
  BOOST_CHECK (!mempool.updatesName (Name ("foo")));
  BOOST_CHECK (mempool.checkNameOps (tx1));
  BOOST_CHECK (mempool.checkNameOps (tx2));
}

BOOST_FIXTURE_TEST_CASE (getTxForName, NameMempoolTestSetup)
{
  BOOST_CHECK (mempool.getTxForName (Name ("reg")).IsNull ());
  BOOST_CHECK (mempool.getTxForName (Name ("upd")).IsNull ());

  const auto txReg = Tx (RegisterScript (ADDR, "reg", "x"));
  const auto txUpd = Tx (UpdateScript (ADDR, "upd", "x"));

  mempool.addUnchecked (Entry (txReg));
  mempool.addUnchecked (Entry (txUpd));

  BOOST_CHECK (mempool.getTxForName (Name ("reg")) == txReg.GetHash ());
  BOOST_CHECK (mempool.getTxForName (Name ("upd")) == txUpd.GetHash ());
}

BOOST_FIXTURE_TEST_CASE (mempool_sanity_check, NameMempoolTestSetup)
{
  mempool.addUnchecked (Entry (Tx (RegisterScript (ADDR, "reg", "x"))));
  mempool.addUnchecked (Entry (Tx (UpdateScript (ADDR, "upd", "x"))));

  CCoinsViewCache view(pcoinsTip.get());
  const CNameScript nameOp(UpdateScript (ADDR, "upd", "y"));
  CNameData data;
  data.fromScript (100, COutPoint (uint256 (), 0), nameOp);
  view.SetName (Name ("upd"), data, false);
  mempool.checkNames (&view);
}

BOOST_FIXTURE_TEST_CASE (registration_conflicts, NameMempoolTestSetup)
{
  const auto tx1 = Tx (RegisterScript (ADDR, "foo", "a"));
  const auto tx2 = Tx (RegisterScript (ADDR, "foo", "b"));
  const auto e = Entry (tx1);

  mempool.addUnchecked (e);
  BOOST_CHECK (mempool.registersName (Name ("foo")));
  BOOST_CHECK (!mempool.checkNameOps (tx2));

  CNameConflictTracker tracker(mempool);
  mempool.removeConflicts (tx2);
  BOOST_CHECK (tracker.GetNameConflicts ()->size () == 1);
  BOOST_CHECK (tracker.GetNameConflicts ()->front ()->GetHash ()
                == tx1.GetHash ());

  BOOST_CHECK (!mempool.registersName (Name ("foo")));
  BOOST_CHECK (mempool.checkNameOps (tx1));
  BOOST_CHECK (mempool.checkNameOps (tx2));
  BOOST_CHECK (mempool.mapTx.empty ());
}

/* ************************************************************************** */

BOOST_AUTO_TEST_SUITE_END ()
