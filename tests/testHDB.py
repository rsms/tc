#!/usr/bin/env python
# -*- coding:utf-8 -*-

import os, sys
import unittest
import pytc

DBNAME = 'test.hdb'
DBNAME2 = 'test.hdb.copy'

class TestHDB(unittest.TestCase):
  def setUp(self):
    if os.path.exists(DBNAME):
      os.remove(DBNAME)

  def testAll(self):
    # new
    db = pytc.HDB()
    # tune
    db.tune(100, 32, 64, pytc.HDBTTCBS)
    # open
    db.open(DBNAME2, pytc.HDBOWRITER | pytc.HDBOCREAT)
    # copy
    db.copy(DBNAME)
    # close
    db.close
    os.remove(DBNAME2)

    # open
    db = pytc.HDB(DBNAME, pytc.HDBOWRITER)

    # put
    db.put('hamu', 'ju')
    db.put('moru', 'pui')
    db.put('kiki', 'nya-')

    # get
    self.assertEqual(db.get('hamu'), 'ju')
    # vsiz
    self.assertEqual(db.vsiz('hamu'), len('ju'))

    # putkeep
    self.assertRaises(
      pytc.Error,
      db.putkeep, 'moru', 'puipui')
    db.putkeep('moruta', 'puipui')
    self.assertEqual(db.get('moruta'), 'puipui')

    # putcat
    db.putcat('kiki', 'nya-nya-')
    self.assertEqual(db.get('kiki'), 'nya-nya-nya-')

    # putasync
    db.putasync('gunya', 'darari')
    # sync
    db.sync
    self.assertEqual(db.get('gunya'), 'darari')

    # out
    db.out('gunya')
    self.assertRaises(
      pytc.Error,
      db.get, 'gunya')
    # optimize
    db.optimize(100, 32, 64, pytc.HDBTTCBS)

    # path
    self.assertEqual(db.path(), DBNAME)
    # rnum
    self.assertEqual(db.rnum(), 4)
    # fsiz
    self.assertNotEqual(db.fsiz(), 0)

    # iterinit
    db.iterinit()
    # iternext
    self.assertEqual(db.iternext(), 'hamu')
    db.iterinit()
    self.assertEqual(db.iternext(), 'hamu')

    # dict like interfaces
    result = []
    for key in db:
      result.append(key)
    self.assertEqual(sorted(result), ['hamu', 'kiki', 'moru', 'moruta'])

    self.assertEqual(sorted(db.keys()), ['hamu', 'kiki', 'moru', 'moruta'])
    self.assertEqual(sorted(db.values()), ['ju', 'nya-nya-nya-', 'pui', 'puipui'])
    self.assertEqual(sorted(db.items()),[
      ('hamu', 'ju'), ('kiki', 'nya-nya-nya-'), ('moru', 'pui'), ('moruta', 'puipui')])

    db['gunya'] = 'tekito'
    self.assertEqual(db['gunya'], 'tekito')
    del db['gunya']
    self.assertRaises(
      pytc.Error,
      db.get, 'gunya')

    self.assert_('hamu' in db)
    self.assert_('python' not in db)

    # vanish
    db.vanish()
    self.assertEqual(db.rnum(), 0)

    # remove
    os.remove(DBNAME)

if __name__=='__main__': unittest.main()
