# encoding: utf-8
import os, sys
import unittest
import tc
import struct

# TODO: setcmpfunc

DBNAME = 'test.tcf'
DBNAME2 = 'test.tcf.copy'

class TestFDB(unittest.TestCase):
    def setUp(self):
        if os.path.exists(DBNAME):
            os.remove(DBNAME)
  
    def tearDown(self):
        if os.path.exists(DBNAME):
            os.remove(DBNAME)

    def testBasics(self):
        # new
        db = tc.FDB(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
        db.put(1, 'test')
        self.assertEqual(db.get(1), 'test')
        self.assertEqual(db.__getitem__(1), 'test')
        self.assertEqual(db[1], 'test')
        self.assertEqual(db.get(2), None)
        
        try:
            db[2]
        except KeyError:
            pass
        else:
            self.fail('Should raise KeyError')

        try:
            not db.has_key("1")
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

        self.assertEqual(len(db), 1)
        self.assert_(1 in db)
        self.assert_(2 not in db)
        self.assert_(db.out(1))
        self.assertEqual(len(db), 0)


def suite():
    return unittest.TestSuite([
        unittest.makeSuite(TestFDB)
    ])

if __name__=='__main__':
    unittest.main()
