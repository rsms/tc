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
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
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
            db["2"]
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

        self.assertRaises(TypeError, db.has_key, "1")

        self.assertEqual(len(db), 1)
        self.assert_(1 in db)
        self.assert_(2 not in db)
        self.assert_(db.out(1))
        self.assertEqual(len(db), 0)

        db[1] = 'hej'
        self.assertEqual(db[1], 'hej')
        db[2] = 'då';
        self.assertEqual(db[2], 'då')
        self.assertEqual(len(db), 2)
        del db[2]
        self.assertEqual(len(db), 1)

        try:
            del db[4]
        except KeyError:
            pass
        else:
            self.fail('Should raise KeyError')

        db[2] = 'test1'
        db[7] = 'test2'
        self.assertEqual(list(db.iterkeys()), [1,2,7])
        self.assertEqual(db.keys(), [1,2,7])
        self.assertEqual(list(db.itervalues()), ['hej', 'test1', 'test2'])
        self.assertEqual(db.values(), ['hej', 'test1', 'test2'])
        self.assertEqual(list(db.iteritems()), [(1, 'hej'), (2, 'test1'), (7, 'test2')])

def suite():
    return unittest.TestSuite([
        unittest.makeSuite(TestFDB)
    ])

if __name__=='__main__':
    unittest.main()
