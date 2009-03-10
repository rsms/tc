# encoding: utf-8
import os, sys
import unittest
import tc
import struct

DBNAME = 'test.tdb'

class TestTDB(unittest.TestCase):
  def setUp(self):
    if os.path.exists(DBNAME):
      os.remove(DBNAME)
  
  def tearDown(self):
    if os.path.exists(DBNAME):
      os.remove(DBNAME)
  
  def testAll(self):
    db = tc.TDB(DBNAME, tc.TDBOWRITER | tc.TDBOCREAT)
    #db.open(DBNAME2, tc.HDBOWRITER | tc.HDBOCREAT)
    
    cols = {'name': 'John Doe', 'age': '45', 'city': u'Internets'}
    db.put('jdoe', cols)
    db.put('bulgur', {'name': u'Bulgur Röv', 'age': '12'})
    
    rec = db.get('jdoe')
    self.assertTrue(isinstance(rec, dict))
    self.assertEquals(rec['name'], 'John Doe')
    self.assertEquals(rec['city'], 'Internets')
    rec = db.get('bulgur')
    self.assertTrue(isinstance(rec, dict))
    self.assertEquals(rec['name'], 'Bulgur R\xc3\xb6v') # utf-8
    
    db.delete('bulgur')
    
    self.assertRaises(KeyError, db.get, 'bulgur')
  

def suite():
  return unittest.TestSuite([
    unittest.makeSuite(TestTDB)
  ])

if __name__=='__main__':
  unittest.main()
