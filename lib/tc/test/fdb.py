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

        try:
            del db["3"]
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

        db[2] = 'test1'
        db[7] = 'test2'
        self.assertEqual(list(db.iterkeys()), [1,2,7])
        self.assertEqual(db.keys(), [1,2,7])
        self.assertEqual(list(db.itervalues()), ['hej', 'test1', 'test2'])
        self.assertEqual(db.values(), ['hej', 'test1', 'test2'])
        self.assertEqual(list(db.iteritems()), [(1, 'hej'), (2, 'test1'), (7, 'test2')])

        for i in xrange(1, 101):
            db[i] = 'test'

        self.assertEqual(len(db), 100)

    def testIterKeys(self):
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
        for i in range(1, 6):
            db[i] = 'test'

        self.assertEqual(list(db.iterkeys()), [1, 2, 3, 4, 5])
        del db[2]
        self.assertEqual(list(db.iterkeys()), [1, 3, 4, 5])
        del db[5]
        self.assertEqual(list(db.iterkeys()), [1, 3, 4])
        del db[1]
        self.assertEqual(list(db.iterkeys()), [3, 4])
        db[1] = 'test2'
        self.assertEqual(list(db.iterkeys()), [1, 3, 4])
        db[2] = 'test2'
        self.assertEqual(list(db.iterkeys()), [1, 2, 3, 4])
        db[5] = 'test2'
        self.assertEqual(list(db.iterkeys()), [1, 2, 3, 4, 5])

        for i in db.iterkeys():
            db[i] = 'k %d' % i

        for i in range(1, 6):
            self.assertEqual(db[i], 'k %d' % i)

        p = 0
        for i in db.iterkeys():
            self.assert_(db.out(i))
            p += 1

        self.assertEqual(p, 5)

        self.assertEqual(list(db.iterkeys()), [])

    def testIterValues(self):
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
        for i in range(1, 6):
            db[i] = 't %d' % i

        self.assertEqual(list(db.itervalues()), ['t 1', 't 2', 't 3', 't 4', 't 5'])
        del db[2]
        self.assertEqual(list(db.itervalues()), ['t 1', 't 3', 't 4', 't 5'])
        del db[5]
        self.assertEqual(list(db.itervalues()), ['t 1', 't 3', 't 4'])
        del db[1]
        self.assertEqual(list(db.itervalues()), ['t 3', 't 4'])
        db[1] = 't x'
        self.assertEqual(list(db.itervalues()), ['t x', 't 3', 't 4'])
        db[2] = 't y'
        self.assertEqual(list(db.itervalues()), ['t x', 't y', 't 3', 't 4'])
        db[5] = 't z'
        self.assertEqual(list(db.itervalues()), ['t x', 't y', 't 3', 't 4', 't z'])

        for i in range(1, 6):
            del db[i]

        self.assertEqual(list(db.itervalues()), [])

    def testIterItems(self):
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
        for i in range(1, 6):
            db[i] = 't %d' % i

        self.assertEqual(list(db.iteritems()), [(1, 't 1'), (2, 't 2'), (3, 't 3'), (4, 't 4'), (5, 't 5')])
        del db[2]
        self.assertEqual(list(db.iteritems()), [(1, 't 1'), (3, 't 3'), (4, 't 4'), (5, 't 5')])
        del db[5]
        self.assertEqual(list(db.iteritems()), [(1, 't 1'), (3, 't 3'), (4, 't 4')])
        del db[1]
        self.assertEqual(list(db.iteritems()), [(3, 't 3'), (4, 't 4')])
        db[1] = 't x'
        self.assertEqual(list(db.iteritems()), [(1, 't x'), (3, 't 3'), (4, 't 4')])
        db[2] = 't y'
        self.assertEqual(list(db.iteritems()), [(1, 't x'), (2, 't y'), (3, 't 3'), (4, 't 4')])
        db[5] = 't z'
        self.assertEqual(list(db.iteritems()), [(1, 't x'), (2, 't y'), (3, 't 3'), (4, 't 4'), (5, 't z')])

        for (i, v) in db.iteritems():
            db[i] = 'k %d' % i

        for i in range(1, 6):
            self.assertEqual(db[i], 'k %d' % i)

        p = 0
        for (i, v) in db.iteritems():
            self.assert_(db.out(i))
            p += 1
        self.assertEqual(p, 5)

        self.assertEqual(list(db.itervalues()), [])

    def testPutkeep(self):
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
        for i in range(1, 6):
            db[i] = 't %d' % i

        self.assertFalse(db.putkeep(1, 'k 1'))
        self.assertEqual(db[1], 't 1')
        self.assert_(db.putkeep(6, 'k 6'))
        self.assertEqual(db[6], 'k 6')

        try:
            db.putkeep('2', 'k 2')
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

        try:
            db.putkeep(1)
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

    def testPut(self):
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
        for i in range(1, 6):
            db[i] = 't %d' % i

        self.assert_(db.put(1, 'k 1'))
        self.assertEqual(db[1], 'k 1')
        self.assert_(db.put(6, 'k 6'))
        self.assertEqual(db[6], 'k 6')

        try:
            db.put('2', 'k 2')
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

        try:
            db.putcat(1)
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

    def testPutcat(self):
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
        for i in range(1, 6):
            db[i] = 't %d' % i

        self.assert_(db.putcat(1, 'k 1'))
        self.assertEqual(db[1], 't 1k 1')
        self.assert_(db.putcat(6, 'k 6'))
        self.assertEqual(db[6], 'k 6')

        try:
            db.putcat('2', 'k 2')
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

        try:
            db.putcat(1)
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')

    def testLen(self):
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)

        self.assertEqual(len(db), 0)

        for i in range(1, 6):
            db[i] = 't %d' % i

        self.assertEqual(len(db), 5)
        del db[1]
        self.assertEqual(len(db), 4)
        try:
            del db[1]
        except:
            pass
        self.assertEqual(len(db), 4)
        db[6] = 'a'
        self.assertEqual(len(db), 5)
        db[4] = '123'
        self.assertEqual(len(db), 5)

        for i in db.iterkeys():
            self.assert_(db.out(i))

        self.assertEqual(len(db), 0)

    def testGet(self):
        db = tc.fdb(DBNAME, tc.FDBOCREAT | tc.FDBOWRITER)
        for i in range(1, 6):
            db[i] = 't %d' % i

        self.assertEqual(db.get(1), 't 1')
        self.assertEqual(db.get(7), None)
        self.assertEqual(db.get(3), 't 3')

        try:
            db.get('2')
        except TypeError:
            pass
        else:
            self.fail('Should raise TypeError')
        

def suite():
    return unittest.TestSuite([
        unittest.makeSuite(TestFDB)
    ])

if __name__=='__main__':
    unittest.main()
