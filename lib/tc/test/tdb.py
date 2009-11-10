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
    #db = tc.TDB(DBNAME, tc.TDBOWRITER | tc.TDBOCREAT)
    #db.open(DBNAME2, tc.HDBOWRITER | tc.HDBOCREAT)
    db = tc.TDB()
    # tune
    db.tune(100, 32, 64, tc.TDBTTCBS)
    # open
    db.open(DBNAME, tc.TDBOWRITER | tc.TDBOCREAT)

    
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
    
    #
    # order:
    #
    
    db.put('torgny',  {'name': 'Torgny Korv', 'age': '31', 'colors': 'red,blue,green'})
    db.put('rosa',    {'name': 'Rosa Flying', 'age': '29', 'colors': 'pink,blue,green'})
    db.put('jdoe',    {'name': 'John Doe',    'age': '45', 'colors': 'red,green,orange'})
    
    q = db.query()
    # No filters
    self.assertTrue('torgny' in q.keys())
    self.assertTrue('rosa' in q.keys())
    self.assertTrue('jdoe' in q.keys())
    q.filter('age', tc.TDBQCNUMGE, '30')
    # Rosa is too young
    self.assertTrue('torgny' in q.keys())
    self.assertTrue('rosa' not in q.keys())
    self.assertTrue('jdoe' in q.keys())
    q.filter('colors', tc.TDBQCSTROR, 'blue')
    # John Doe don't like blue
    self.assertTrue('torgny' in q.keys())
    self.assertTrue('rosa' not in q.keys())
    self.assertTrue('jdoe' not in q.keys())
    
    q = db.query()
    # Fresh query
    self.assertTrue('torgny' in q.keys())
    self.assertTrue('rosa' in q.keys())
    self.assertTrue('jdoe' in q.keys())
    # order asc by name
    q.order('name') # tc.TDBQOSTRASC is the default type
    pks = q.keys()
    self.assertEquals(pks[0], 'jdoe')
    self.assertEquals(pks[1], 'rosa')
    self.assertEquals(pks[2], 'torgny')
    # order desc by name
    q.order('name', tc.TDBQOSTRDESC)
    pks = q.keys()
    self.assertEquals(pks[0], 'torgny')
    self.assertEquals(pks[1], 'rosa')
    self.assertEquals(pks[2], 'jdoe')
    # order asc by age
    q.order(type=tc.TDBQONUMASC, column='age')
    pks = q.keys()
    self.assertEquals(pks[0], 'rosa')
    self.assertEquals(pks[1], 'torgny')
    self.assertEquals(pks[2], 'jdoe')
    # order desc by age
    q.order('age', tc.TDBQONUMDESC)
    pks = q.keys()
    self.assertEquals(pks[0], 'jdoe')
    self.assertEquals(pks[1], 'torgny')
    self.assertEquals(pks[2], 'rosa')
    # order asc by primary key (by None/NULL -> empty string)
    q.order(None)
    pks = q.keys()
    self.assertEquals(pks[0], 'jdoe')
    self.assertEquals(pks[1], 'rosa')
    self.assertEquals(pks[2], 'torgny')
    # order asc by primary key (by empty string)
    q.order('')
    pks = q.keys()
    self.assertEquals(pks[0], 'jdoe')
    self.assertEquals(pks[1], 'rosa')
    self.assertEquals(pks[2], 'torgny')
    # order desc by primary key
    q.order('', tc.TDBQOSTRDESC)
    pks = q.keys()
    self.assertEquals(pks[0], 'torgny')
    self.assertEquals(pks[1], 'rosa')
    self.assertEquals(pks[2], 'jdoe')
    
    q = db.query()
    q.order('age', tc.TDBQONUMDESC)
    # get thos with ages < 40
    q.filter('age', tc.TDBQCNUMLE, '40')
    pks = q.keys()
    self.assertEquals(pks[0], 'torgny')
    self.assertEquals(pks[1], 'rosa')
    self.assertEquals(len(pks), 2)
    
    # add to the filter only those > 30 ( and < 40 from before)
    q.filter('age', tc.TDBQCNUMGE, '30')
    pks = q.keys()
    self.assertEquals(pks[0], 'torgny')
    self.assertEquals(len(pks), 1)
    
    # filter out those who have blue in the colors
    q = db.query()
    q.order('age', tc.TDBQONUMDESC)
    q.filter('colors', tc.TDBQCSTRINC, 'blue')
    pks = q.keys()
    self.assertEquals(pks[0], 'torgny')
    self.assertEquals(pks[1], 'rosa')
    self.assertEquals(len(pks), 2)
    
    # add a filter to the existing:
    q.filter('colors', tc.TDBQCSTRINC, 'pink')
    pks = q.keys()
    self.assertEquals(pks[0], 'rosa')
    self.assertEquals(len(pks), 1)
  

def suite():
  return unittest.TestSuite([
    unittest.makeSuite(TestTDB)
  ])

if __name__=='__main__':
  unittest.main()
