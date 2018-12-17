#!/usr/bin/env python3
# Copyright (c) 2014-2018 Daniel Kraft
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# RPC test for names at segwit addresses.

from test_framework.names import NameTestFramework, val

from test_framework.blocktools import (
  add_witness_commitment,
  create_block,
  create_coinbase,
)
from test_framework.messages import (
  CScriptWitness,
  CTransaction,
  CTxInWitness,
  CTxWitness,
)
from test_framework.util import (
  assert_equal,
  assert_greater_than,
  assert_raises_rpc_error,
  bytes_to_hex_str,
  hex_str_to_bytes,
)

from decimal import Decimal
import io

SEGWIT_ACTIVATION_HEIGHT = 432


class NameSegwitTest (NameTestFramework):

  def set_test_params (self):
    self.setup_name_test ([["-debug", "-par=1"]] * 1)

  def checkNameValueAddr (self, name, value, addr):
    """
    Verifies that the given name has the given value and address.
    """

    data = self.checkName (0, name, value)
    assert_equal (data['address'], addr)

  def findUnspent (self, amount):
    """
    Finds and returns an unspent input the node has with at least the
    given value.
    """

    unspents = self.node.listunspent ()
    for u in unspents:
      if u['amount'] >= amount:
        return u

    raise AssertionError ("Could not find suitable unspent output")

  def buildDummySegwitNameUpdate (self, name, value, addr):
    """
    Builds a transaction that updates the given name to the given value and
    address.  We assume that the name is at a native segwit script.  The witness
    of the transaction will be set to two dummy stack elements so that the
    program itself is "well-formed" even if it won't execute successfully.
    """

    data = self.node.name_show (name)
    u = self.findUnspent (Decimal ('0.01'))
    ins = [data, u]
    outs = {addr: Decimal ('0.01')}

    txHex = self.node.createrawtransaction (ins, outs)
    nameOp = {"op": "name_update", "name": name, "value": value}
    txHex = self.node.namerawtransaction (txHex, 0, nameOp)['hex']
    txHex = self.node.signrawtransactionwithwallet (txHex)['hex']

    tx = CTransaction ()
    tx.deserialize (io.BytesIO (hex_str_to_bytes (txHex)))
    tx.wit = CTxWitness ()
    tx.wit.vtxinwit.append (CTxInWitness ())
    tx.wit.vtxinwit[0].scriptWitness = CScriptWitness ()
    tx.wit.vtxinwit[0].scriptWitness.stack = [b"dummy"] * 2
    txHex = bytes_to_hex_str (tx.serialize ())

    return txHex

  def tryUpdateSegwitName (self, name, value, addr):
    """
    Tries to update the given name to the given value and address.
    """

    txHex = self.buildDummySegwitNameUpdate (name, value, addr)
    self.node.sendrawtransaction (txHex, True)

  def tryUpdateInBlock (self, name, value, addr, withWitness):
    """
    Tries to update the given name with a dummy witness directly in a block
    (to bypass any checks done on the mempool).
    """

    txHex = self.buildDummySegwitNameUpdate (name, value, addr)
    tx = CTransaction ()
    tx.deserialize (io.BytesIO (hex_str_to_bytes (txHex)))

    tip = self.node.getbestblockhash ()
    height = self.node.getblockcount () + 1
    nTime = self.node.getblockheader (tip)["mediantime"] + 1
    block = create_block (int (tip, 16), create_coinbase (height), nTime)

    block.vtx.append (tx)
    add_witness_commitment (block, 0)
    block.solve ()

    blkHex = bytes_to_hex_str (block.serialize (withWitness))
    return self.node.submitblock (blkHex)

  def run_test (self):
    self.node = self.nodes[0]

    # Register a test name to a bech32 pure-segwit address.
    addr = self.node.getnewaddress ("test", "bech32")
    name = "d/test"
    value = "{}"
    self.node.name_register (name, value, {"destAddress": addr})
    self.node.generate (1)
    self.checkNameValueAddr (name, value, addr)

    # Before segwit activation, the script should behave as anyone-can-spend.
    # It will still fail due to non-mandatory flag checks when submitted
    # into the mempool.
    assert_greater_than (SEGWIT_ACTIVATION_HEIGHT, self.node.getblockcount ())
    assert_raises_rpc_error (-26, 'Script failed an OP_EQUALVERIFY operation',
                             self.tryUpdateSegwitName,
                             name, val ("wrong value"), addr)
    self.node.generate (1)
    self.checkNameValueAddr (name, value, addr)

    # But directly in a block, the update should work with a dummy witness.
    assert_equal (self.tryUpdateInBlock (name, val ("stolen"), addr,
                                         withWitness=False),
                  None)
    self.checkNameValueAddr (name, val ("stolen"), addr)

    # Activate segwit.  Restore original value for name.
    self.node.generate (400)
    self.node.name_update (name, value, {"destAddress": addr})
    self.node.generate (1)
    self.checkNameValueAddr (name, value, addr)

    # Verify that now trying to update the name without a proper signature
    # fails differently.
    assert_greater_than (self.node.getblockcount (), SEGWIT_ACTIVATION_HEIGHT)
    assert_equal (self.tryUpdateInBlock (name, val ("wrong value"), addr,
                                         withWitness=True),
                  'non-mandatory-script-verify-flag'
                    + ' (Script failed an OP_EQUALVERIFY operation)')
    self.checkNameValueAddr (name, value, addr)

    # Updating the name ordinarily (with signature) should work fine even
    # though it is at a segwit address.  Also spending from P2SH-segwit
    # should work fine.
    addrP2SH = self.node.getnewaddress ("test", "p2sh-segwit")
    self.node.name_update (name, val ("value 2"), {"destAddress": addrP2SH})
    self.node.generate (1)
    self.checkNameValueAddr (name, val ("value 2"), addrP2SH)
    self.node.name_update (name, val ("value 3"), {"destAddress": addr})
    self.node.generate (1)
    self.checkNameValueAddr (name, val ("value 3"), addr)


if __name__ == '__main__':
  NameSegwitTest ().main ()
