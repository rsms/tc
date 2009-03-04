# encoding: utf-8
import unittest, sys, os

def suite():
  suites = []
  import hdb, bdb, fdb
  suites.append(bdb.suite())
  suites.append(fdb.suite())
  suites.append(hdb.suite())
  return unittest.TestSuite(suites)

def test(*va, **kw):
  runner = unittest.TextTestRunner(*va, **kw)
  return runner.run(suite())

if __name__ == "__main__":
  test()
