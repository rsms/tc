# encoding: utf-8
import os, sys
import unittest
import tc
import struct

DBNAME = 'test.bdb'
DBNAME2 = 'test.bdb.copy'

class TestFDB(unittest.TestCase):
    db = None

    def setUp(self):
        if os.path.exists(DBNAME):
            os.remove(DBNAME)

        self.db = tc.FDB(DBNAME, tc.FDBOWRITER | tc.FDBOCREAT | tc.FDBOTRUNC)
  
    def tearDown(self):
        self.db.close()

        if os.path.exists(DBNAME):
            os.remove(DBNAME)

    def testPut(self):
        self.db[1] = 'a'

        try:
            self.db["string"] = 'a'
        except TypeError:
            pass
        else:
            self.fail("Shouldn't be able to write to database")

        self.db[2] = 'b'
        self.assertEqual(self.db[2], 'b')

        self.db[2] = 'e'
        self.assertEqual(self.db[2], 'e')

        self.assertEqual(self.db.put(2, 'c'), True)
        self.assertEqual(self.db[2], 'c')

        self.assertEqual(self.db.put(2, 'f'), True)
        self.assertEqual(self.db[2], 'f')

        self.assertEqual(self.db.putkeep(2, 'd'), False)
        self.assertEqual(self.db[2], 'f')

        self.assertEqual(self.db.putkeep(3, 'd'), True)
        self.assertEqual(self.db[3], 'd')

        self.assertEqual(self.db.putcat(4, 'g'), True)
        self.assertEqual(self.db[4], 'g')

        self.assertEqual(self.db.putcat(3, 'h'), True)
        self.assertEqual(self.db[3], 'dh')

    def testRead(self):
        self.db[1] = 'a'
        self.db.close()

        self.db = tc.FDB(DBNAME, tc.FDBOREADER)

        self.assertEqual(self.db[1], 'a')

        try:
            self.db[2] = 'b'
        except Exception:
            pass
        else:
            self.fail("Shouldn't be able to write to database")
        self.db.close()

        self.db = tc.FDB()
        self.db.open(DBNAME, tc.FDBOREADER)
        
        self.assertEqual(self.db[1], 'a')

        try:
            self.db[2] = 'b'
        except Exception:
            pass
        else:
            self.fail("Shouldn't be able to write to database")

    def testGet(self):
        self.db[1] = 'a'
        self.assertEqual(self.db[1], 'a')
        try:
            self.db[2]
        except KeyError:
            pass
        else:
            self.fail("Key 2 shouldn't exists")

        self.assertEqual(self.db.get(1), 'a')
        try:
            self.db.get(2)
        except KeyError:
            pass
        else:
            self.fail("Key 2 shouldn't exists")

    # Should also test max file size
    def testTune(self):
        self.db.close()
        self.db = tc.FDB()
        self.db.tune(4, 0)
        self.db.open(DBNAME, tc.FDBOWRITER | tc.FDBOCREAT | tc.FDBOTRUNC)

        self.db[1] = 'abcd'
        self.assertEqual(self.db[1], 'abcd')
        self.db[2] = 'abcdef'
        self.assertEqual(self.db[2], 'abcd')

        self.db.put(1, 'abcd')
        self.assertEqual(self.db.get(1), 'abcd')
        self.db.put(1, 'abcdef')
        self.assertEqual(self.db.get(2), 'abcd')

    def testLen(self):
        self.db[1] = 'a'
        self.db[2] = 'b'
        self.assertEqual(len(self.db), 2)
        self.db[3] = 'c'
        self.assertEqual(len(self.db), 3)
        self.db[2] = 'd'
        self.assertEqual(len(self.db), 3)
        del self.db[2]
        self.assertEqual(len(self.db), 2)
        self.db.out(1)
        self.assertEqual(len(self.db), 1)

    def testKeys(self):
        self.db[1] = 'a'
        self.db[2] = 'b'
        self.db[17] = 'c'
        self.assertEqual(self.db.keys(), [1, 2, 17])
        del self.db[2]
        self.db[172121] = 'v'
        self.assertEqual(self.db.keys(), [1, 17, 172121])


def suite():
    return unittest.TestSuite([
        unittest.makeSuite(TestFDB)
    ])

if __name__=='__main__':
    unittest.main()