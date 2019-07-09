// Copyright (c) 2014-2019 Daniel Kraft
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef H_BITCOIN_NAMES_MEMPOOL
#define H_BITCOIN_NAMES_MEMPOOL

#include <names/common.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <map>
#include <memory>
#include <set>

class CCoinsView;
class CTxMemPool;
class CTxMemPoolEntry;

/**
 * Handle the name component of the transaction mempool.  This keeps track
 * of name operations that are in the mempool and ensures that all transactions
 * kept are consistent.  For instance, no two transactions are allowed to
 * register the same name, and name registration transactions are removed if a
 * conflicting registration makes it into a block.
 */
class CNameMemPool
{

private:

  /** The parent mempool object.  Used to e.g. remove conflicting tx.  */
  CTxMemPool& pool;

  /**
   * Keep track of names that are registered by transactions in the pool.
   * For any given name, at most one registering transaction is allowed in the
   * mempool (as all others would conflict with it).
   */
  std::map<valtype, uint256> mapNameRegs;

  /**
   * Keep track of all transactions that update a given name.  For each name,
   * this may be a whole chain of updates.  This field is used to remove the
   * transactions from the mempool should the name expire (and the updates
   * thus become invalid).
   */
  std::map<valtype, std::set<uint256>> updates;

public:

  /**
   * Constructs with reference to parent mempool.
   */
  explicit CNameMemPool (CTxMemPool& p)
    : pool(p)
  {}

  /**
   * Checks whether a particular name is being registered by
   * some transaction in the mempool.  Does not lock, this is
   * done by the parent mempool (which calls through afterwards).
   */
  bool
  registersName (const valtype& name) const
  {
    return mapNameRegs.count (name) > 0;
  }

  /**
   * Checks whether a particular name has at least one pending update.
   * Does not lock.
   */
  bool
  updatesName (const valtype& name) const
  {
    const auto mit = updates.find (name);
    if (mit == updates.end ())
      return false;
    return !mit->second.empty ();
  }

  /**
   * Returns the last outpoint of a (potential) chain of pending name operations
   * for the given name.  This is the output that should be spent with the
   * next constructed name update.  Returns a null outpoint if the name
   * is not being updated at the moment.
   */
  COutPoint lastNameOutput (const valtype& name) const;

  /**
   * Clears all data.
   */
  void
  clear ()
  {
    mapNameRegs.clear ();
    updates.clear ();
  }

  /**
   * Adds an entry without checking it.  It should have been checked
   * already.  If this conflicts with the mempool, it may throw.
   */
  void addUnchecked (const CTxMemPoolEntry& entry);

  /**
   * Removes the given mempool entry.  It is assumed that it is present.
   */
  void remove (const CTxMemPoolEntry& entry);

  /**
   * Removes conflicts for the given tx, based on name operations.  I.e.,
   * if the tx registers a name that conflicts with another registration
   * in the mempool, detect this and remove the mempool tx accordingly.
   */
  void removeConflicts (const CTransaction& tx);

  /**
   * Performs sanity checks.  Throws if it fails.
   */
  void check (const CCoinsView& coins) const;

  /**
   * Checks if a tx can be added (based on name criteria) without
   * causing a conflict.
   */
  bool checkTx (const CTransaction& tx) const;

};

/**
 * Utility class that listens to a mempool's removal notifications to track
 * name conflicts.  This is used for DisconnectTip and unit testing.
 */
class CNameConflictTracker
{

private:

  std::shared_ptr<std::vector<CTransactionRef>> txNameConflicts;
  CTxMemPool& pool;

public:

  explicit CNameConflictTracker (CTxMemPool &p);
  ~CNameConflictTracker ();

  inline const std::shared_ptr<const std::vector<CTransactionRef>>
  GetNameConflicts () const
  {
    return txNameConflicts;
  }

  void AddConflictedEntry (CTransactionRef txRemoved);

};

#endif // H_BITCOIN_NAMES_MEMPOOL
