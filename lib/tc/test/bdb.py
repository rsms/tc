# encoding: utf-8
import os, sys
import unittest
import tc
import struct

# TODO: setcmpfunc

DBNAME = 'test.bdb'
DBNAME2 = 'test.bdb.copy'

class TestBDB(unittest.TestCase):
  def setUp(self):
    if os.path.exists(DBNAME):
      os.remove(DBNAME)
  
  def tearDown(self):
    if os.path.exists(DBNAME):
      os.remove(DBNAME)
  
  def testAll(self):
    # new
    db = tc.BDB()
    # tune
    db.tune(2, 4, 19, 4, 5, tc.BDBTTCBS)
    # setcache
    db.setcache(1, 1)
    # open
    db.open(DBNAME2, tc.BDBOWRITER | tc.BDBOCREAT)
    # copy
    db.copy(DBNAME)
    # close
    db.close
    os.remove(DBNAME2)
    
    # open
    db = tc.BDB(DBNAME, tc.BDBOWRITER)
    
    # put
    db.put('hamu', 'ju')
    db.put('moru', 'pui')
    db.put('kiki', 'nya-')
    db.put('gunya', 'gorori')
    
    # get
    self.assertEqual(db.get('hamu'), 'ju')
    # vsiz
    self.assertEqual(db.vsiz('hamu'), len('ju'))
    
    # putkeep
    self.assertRaises(
      tc.Error,
      db.putkeep, 'moru', 'puipui')
    db.putkeep('moruta', 'puipui')
    self.assertEqual(db.get('moruta'), 'puipui')
    
    # putcat
    db.putcat('kiki', 'nya-nya-')
    self.assertEqual(db.get('kiki'), 'nya-nya-nya-')
    
    # putdup
    db.putdup('kiki', 'unya-n')
    # getlist
    self.assertEqual(db.getlist('kiki'), ['nya-nya-nya-', 'unya-n'])
    # vnum
    self.assertEqual(db.vnum('kiki'), 2)
    
    # out
    db.out('gunya')
    self.assertRaises(
      KeyError,
      db.get, 'gunya')
    
    # putlist
    db.putlist('gunya', ['gorori', 'harahetta', 'nikutabetai'])
    self.assertEqual(db.getlist('gunya'), ['gorori', 'harahetta', 'nikutabetai'])
    # outlist
    db.outlist('gunya')
    self.assertEqual(db.vnum('gunya'), 0)
    
    # optimize
    db.optimize(2, 4, 19, 4, 5, tc.BDBTTCBS)
    
    # path
    self.assertEqual(db.path(), DBNAME)
    # rnum
    self.assertEqual(db.rnum(), 5)
    # fsiz
    self.assertNotEqual(db.fsiz(), 0)
    
    # dict like interfaces
    result = []
    for key in db:
      result.append(key)
    self.assertEqual(sorted(result), ['hamu', 'kiki', 'kiki', 'moru', 'moruta'])
    
    self.assertEqual(sorted(db.keys()),
      ['hamu', 'kiki', 'kiki', 'moru', 'moruta'])
    self.assertEqual(sorted(db.values()),
      ['ju', 'nya-nya-nya-', 'pui', 'puipui', 'unya-n'])
    self.assertEqual(sorted(db.items()),[
      ('hamu', 'ju'), ('kiki', 'nya-nya-nya-'), ('kiki', 'unya-n'),
      ('moru', 'pui'), ('moruta', 'puipui')])
    
    result = []
    for key in db.iterkeys():
      result.append(key)
    self.assertEqual(sorted(result), ['hamu', 'kiki', 'kiki', 'moru', 'moruta'])
    
    result = []
    for value in db.itervalues():
      result.append(value)
    self.assertEqual(sorted(result),
      ['ju', 'nya-nya-nya-', 'pui', 'puipui', 'unya-n'])
    
    result = []
    for (key, value) in db.iteritems():
      result.append((key, value))
    self.assertEqual(sorted(result),[
      ('hamu', 'ju'), ('kiki', 'nya-nya-nya-'), ('kiki', 'unya-n'),
      ('moru', 'pui'), ('moruta', 'puipui')])
    
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
    
    # curnew
    cur = db.curnew()
    # BDBCUR.first
    cur.first()
    # BDBCUR.key
    self.assertEqual(cur.key(), 'hamu')
    # BDBCUR.val
    self.assertEqual(cur.val(), 'ju')
    # BDBCUR.rec
    self.assertEqual(cur.rec(), ('hamu', 'ju'))
    # BDBCUR.next
    cur.next()
    self.assertEqual(cur.rec(), ('kiki', 'nya-nya-nya-'))
    # BDBCUR.put
    cur.put('fungofungo', tc.BDBCPCURRENT)
    self.assertEqual(cur.rec(), ('kiki', 'fungofungo'))
    # BDBCUR.out
    cur.out()
    self.assertEqual(db.vnum('kiki'), 1)
    # BDBCUR.prev
    cur.prev()
    self.assertEqual(cur.rec(), ('hamu', 'ju'))
    # BDBCUR.jump
    cur.jump('moru')
    self.assertEqual(cur.rec(), ('moru', 'pui'))
    # BDBCUR.last
    cur.last()
    self.assertEqual(cur.rec(), ('moruta', 'puipui'))
    
    # tranbegin
    db.tranbegin()
    db.put('moru', 'pupupu')
    self.assertEqual(db.get('moru'), 'pupupu')
    # tranabort
    db.tranabort()
    self.assertEqual(db.get('moru'), 'pui')
    
    db.tranbegin()
    db.put('moru', 'pupupu')
    # trancommit
    db.trancommit()
    self.assertEqual(db.get('moru'), 'pupupu')
    
    db['nagasaki'] = 'ichiban'
    db['nagasaki-higashi'] = 'toh'
    db['nagasaki-nishi'] = 'zai'
    db['nagasaki-minami'] = 'nan'
    db['nagasaki-kita'] = 'boku'
    db['nagasaki-hokuyodai'] = 'hokuyodai'
    # range
    self.assertEqual(db.range('nagasaki', False, 'nagasaki-kita', True, 3),
                     ['nagasaki-higashi',
                      'nagasaki-hokuyodai',
                      'nagasaki-kita'])
    # rangefwm
    self.assertEqual(db.rangefwm('nagasaki', 5),
                     ['nagasaki', 'nagasaki-higashi',
                      'nagasaki-hokuyodai', 'nagasaki-kita',
                      'nagasaki-minami'])
    
    # vanish
    db.vanish()
    self.assertRaises(
      KeyError,
      db.rnum)
    
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
    
    os.remove(DBNAME)
  
  def testCmpFunc(self):
    db = tc.BDB()
    # setcmpfunc
    db.setcmpfunc(lambda x,y:len(x) == len(y), 1)
    db.open(DBNAME, tc.BDBOWRITER | tc.BDBOCREAT)
    
    db['kiki'] = 'nya-'
    db['moru'] = 'pui'
    self.assertEqual(db.get('kiki'), 'pui')
    
    os.remove(DBNAME)
  

def suite():
  return unittest.TestSuite([
    unittest.makeSuite(TestBDB)
  ])

if __name__=='__main__':
  unittest.main()
