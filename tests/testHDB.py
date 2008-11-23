#!/usr/bin/env python
# -*- coding:utf-8 -*-

import os, sys
import unittest
import pytc
import struct

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
      KeyError,
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

    result = []
    for key in db.iterkeys():
      result.append(key)
    self.assertEqual(sorted(result), ['hamu', 'kiki', 'moru', 'moruta'])

    result = []
    for value in db.itervalues():
      result.append(value)
    self.assertEqual(sorted(result), ['ju', 'nya-nya-nya-', 'pui', 'puipui'])

    result = []
    for (key, value) in db.iteritems():
      result.append((key, value))
    self.assertEqual(sorted(result), [
      ('hamu', 'ju'), ('kiki', 'nya-nya-nya-'), ('moru', 'pui'), ('moruta', 'puipui')])

    # this bug is reported by id:a2c
    self.assertRaises(TypeError, eval, 'db[:]', globals(), locals())

    db['gunya'] = 'tekito'
    self.assertEqual(db['gunya'], 'tekito')
    del db['gunya']
    self.assertRaises(
      KeyError,
      db.get, 'gunya')

    self.assert_('hamu' in db)
    self.assert_('python' not in db)

    # vanish
    db.vanish()
    self.assertEqual(db.rnum(), 0)

    # addint
    db['int'] = struct.pack('i', 0)
    db.addint('int', 1)
    self.assertEqual(struct.unpack('i', db['int'])[0], 1)

    # adddouble
    db['double'] = struct.pack('d', 0.0)
    db.adddouble('double', 1.0)
    self.assertEqual(struct.unpack('d', db['double'])[0], 1.0)

    # Error handling with no record. Thanks to Hatem Nassrat.
    try:
      db['absence']
    except Exception, e:
      self.assertEqual(type(e), KeyError)

    # remove
    os.remove(DBNAME)


if __name__=='__main__': unittest.main()
